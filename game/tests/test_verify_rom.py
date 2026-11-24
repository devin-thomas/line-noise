from __future__ import annotations

import contextlib
import io
import struct
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT))

from tools import verify_rom  # noqa: E402
from uppercut_gba_rom_verify import (  # noqa: E402
    ValidationError,
    calculate_header_complement,
    validate_elf_bytes,
    validate_rom_bytes,
    validate_rom_file,
)


NINTENDO_LOGO = bytes.fromhex(
    "24ffae51699aa2213d84820a84e409ad11248b98c0817f21a352be199309ce201"
    "0464a4af82731ec58c7e83382e3cebf85f4df94ce4b09c194568ac01372a7fc"
    "9f844d73a3ca9a615897a327fc039876231dc7610304ae56bf38840040a70efd"
    "ff52fe036f9530f197fbc08560d68025a963be03014e38e2f9a234ffbb3e034"
    "4780090cb88113a9465c07c6387f03cafd625e48b380aac7221d4f807"
)


def make_valid_rom() -> bytearray:
    rom = bytearray(0xC0)
    rom[0x04:0xA0] = NINTENDO_LOGO
    rom[0xA0:0xAC] = b"LINE NOISE!\x00"
    rom[0xAC:0xB0] = b"LNSE"
    rom[0xB0:0xB2] = b"00"
    rom[0xB2] = 0x96
    rom[0xBD] = 0xD8
    return rom


def make_valid_elf() -> bytearray:
    entry_point = 0x08000000
    ident = b"\x7fELF\x01\x01\x01" + bytes(9)
    header = struct.pack(
        "<16sHHIIIIIHHHHHH",
        ident,
        2,
        40,
        1,
        entry_point,
        52,
        0,
        0x05000000,
        52,
        32,
        1,
        0,
        0,
        0,
    )
    program_header = struct.pack(
        "<IIIIIIII",
        1,
        84,
        entry_point,
        entry_point,
        4,
        4,
        5,
        4,
    )
    return bytearray(header + program_header + b"\x00\x00\x00\x00")


class RomValidationTests(unittest.TestCase):
    def test_accepts_release_header(self) -> None:
        info = validate_rom_bytes(bytes(make_valid_rom()), verify_rom.CARTRIDGE)

        self.assertEqual(info.title, "LINE NOISE!")
        self.assertEqual(info.game_code, "LNSE")
        self.assertEqual(info.maker_code, "00")
        self.assertEqual(info.complement, 0xD8)

    def test_rejects_each_required_header_field(self) -> None:
        cases = (
            (0x04, b"\x00", "Nintendo logo"),
            (0xA0, b"?", "title"),
            (0xAC, b"?", "game code"),
            (0xB0, b"?", "maker code"),
            (0xB2, b"\x00", "fixed byte"),
        )
        for offset, replacement, message in cases:
            with self.subTest(field=message):
                rom = make_valid_rom()
                rom[offset : offset + len(replacement)] = replacement
                with self.assertRaisesRegex(ValidationError, message):
                    validate_rom_bytes(bytes(rom), verify_rom.CARTRIDGE)

    def test_rejects_wrong_title_with_other_header_fields_valid(self) -> None:
        rom = make_valid_rom()
        rom[0xA0:0xAC] = b"LINE NOISE?\x00"
        rom[0xBD] = 0xBA

        with self.assertRaisesRegex(ValidationError, "title"):
            validate_rom_bytes(bytes(rom), verify_rom.CARTRIDGE)

    def test_rejects_wrong_complement_checksum(self) -> None:
        rom = make_valid_rom()
        rom[0xBD] = 0xD9

        with self.assertRaisesRegex(ValidationError, "complement checksum"):
            validate_rom_bytes(bytes(rom), verify_rom.CARTRIDGE)

    def test_rejects_short_and_misaligned_roms(self) -> None:
        with self.assertRaisesRegex(ValidationError, "ROM size"):
            validate_rom_bytes(bytes(0xBF), verify_rom.CARTRIDGE)

        with self.assertRaisesRegex(ValidationError, "4-byte aligned"):
            validate_rom_bytes(bytes(make_valid_rom()) + b"\x00", verify_rom.CARTRIDGE)


class ElfValidationTests(unittest.TestCase):
    def test_accepts_executable_load_segment_covering_entry(self) -> None:
        info = validate_elf_bytes(bytes(make_valid_elf()))

        self.assertEqual(info.entry_point, 0x08000000)
        self.assertEqual(info.load_segment_count, 1)

    def test_rejects_header_only_file(self) -> None:
        elf = make_valid_elf()[:52]
        struct.pack_into("<I", elf, 28, 0)
        struct.pack_into("<H", elf, 42, 0)
        struct.pack_into("<H", elf, 44, 0)

        with self.assertRaisesRegex(
            ValidationError, "program header|entry point"
        ):
            validate_elf_bytes(bytes(elf))

    def test_rejects_truncated_program_header_table(self) -> None:
        elf = make_valid_elf()[:60]

        with self.assertRaisesRegex(ValidationError, "program header table"):
            validate_elf_bytes(bytes(elf))

    def test_rejects_entry_outside_executable_load_segment(self) -> None:
        elf = make_valid_elf()
        struct.pack_into("<I", elf, 24, 0x02000000)

        with self.assertRaisesRegex(ValidationError, "entry point"):
            validate_elf_bytes(bytes(elf))

    def test_rejects_entry_in_uninitialized_tail_of_executable_segment(self) -> None:
        elf = make_valid_elf()
        struct.pack_into("<I", elf, 24, 0x08000003)
        struct.pack_into("<I", elf, 68, 2)

        with self.assertRaisesRegex(ValidationError, "entry point"):
            validate_elf_bytes(bytes(elf))

    def test_rejects_load_segment_past_end_of_file(self) -> None:
        elf = make_valid_elf()
        struct.pack_into("<I", elf, 56, 0x1000)

        with self.assertRaisesRegex(ValidationError, "extends past"):
            validate_elf_bytes(bytes(elf))

    def test_rejects_wrong_elf_identity_fields(self) -> None:
        cases = (
            (4, 2, "ELF32"),
            (5, 2, "little-endian"),
            (18, 3, "machine"),
        )
        for offset, replacement, message in cases:
            with self.subTest(field=message):
                elf = make_valid_elf()
                elf[offset] = replacement
                with self.assertRaisesRegex(ValidationError, message):
                    validate_elf_bytes(bytes(elf))


class FileAndCliTests(unittest.TestCase):
    def test_file_reader_reports_missing_artifact(self) -> None:
        with self.assertRaisesRegex(ValidationError, "cannot read ROM"):
            validate_rom_file("definitely-missing-line-noise.gba", verify_rom.CARTRIDGE)

    def test_main_reports_success_and_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            rom_path = directory / "line-noise.gba"
            elf_path = directory / "line-noise.elf"
            rom_path.write_bytes(make_valid_rom())
            elf_path.write_bytes(make_valid_elf())

            stdout = io.StringIO()
            stderr = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                success = verify_rom.main([str(rom_path), str(elf_path)])
            self.assertEqual(success, 0)
            self.assertIn("PASS ROM", stdout.getvalue())
            self.assertIn("PASS ELF", stdout.getvalue())
            self.assertEqual(stderr.getvalue(), "")

            rom_path.write_bytes(b"invalid")
            stdout = io.StringIO()
            stderr = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                failure = verify_rom.main([str(rom_path), str(elf_path)])
            self.assertEqual(failure, 1)
            self.assertIn("FAIL ROM", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
