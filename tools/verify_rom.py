#!/usr/bin/env python3
"""Validate the Line Noise GBA ROM and its symbol-bearing ELF."""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from os import PathLike
from pathlib import Path
from typing import Iterable, Sequence


ROM_HEADER_SIZE = 0xC0
MAX_ROM_SIZE = 32 * 1024 * 1024
EXPECTED_DISPLAY_TITLE = "LINE NOISE!"
EXPECTED_TITLE = b"LINE NOISE!\x00"
EXPECTED_GAME_CODE = b"LNSE"
EXPECTED_MAKER_CODE = b"00"
EXPECTED_FIXED_BYTE = 0x96

NINTENDO_LOGO = bytes.fromhex(
    "24ffae51699aa2213d84820a84e409ad11248b98c0817f21a352be199309ce201"
    "0464a4af82731ec58c7e83382e3cebf85f4df94ce4b09c194568ac01372a7fc"
    "9f844d73a3ca9a615897a327fc039876231dc7610304ae56bf38840040a70efd"
    "ff52fe036f9530f197fbc08560d68025a963be03014e38e2f9a234ffbb3e034"
    "4780090cb88113a9465c07c6387f03cafd625e48b380aac7221d4f807"
)

ELF_HEADER_SIZE = 52
ELF_PROGRAM_HEADER_SIZE = 32
ELF_MAGIC = b"\x7fELF"
ELFCLASS32 = 1
ELFDATA2LSB = 1
EV_CURRENT = 1
ET_EXEC = 2
EM_ARM = 40
EF_ARM_EABIMASK = 0xFF000000
EF_ARM_EABI_VER5 = 0x05000000
PT_LOAD = 1
PF_X = 0x1

PathInput = str | PathLike[str]


@dataclass(frozen=True)
class RomInfo:
    size: int
    title: str
    game_code: str
    maker_code: str
    complement: int


@dataclass(frozen=True)
class ElfInfo:
    size: int
    entry_point: int
    flags: int
    eabi_version: int
    load_segment_count: int


class ValidationError(ValueError):
    """Raised when an artifact does not satisfy the release contract."""

    def __init__(self, errors: str | Iterable[str]) -> None:
        if isinstance(errors, str):
            details = (errors,)
        else:
            details = tuple(errors)
        if not details:
            details = ("artifact validation failed",)
        self.errors = details
        super().__init__("; ".join(details))


def calculate_header_complement(rom: bytes) -> int:
    """Return the GBA header complement byte for a complete ROM header."""
    return (-sum(rom[0xA0:0xBD]) - 0x19) & 0xFF


def validate_rom_bytes(rom: bytes) -> RomInfo:
    """Validate ROM bytes against the Line Noise cartridge contract."""
    errors: list[str] = []
    rom_size = len(rom)

    if not ROM_HEADER_SIZE <= rom_size <= MAX_ROM_SIZE:
        errors.append(
            f"ROM size must be between {ROM_HEADER_SIZE} and {MAX_ROM_SIZE} bytes, "
            f"found {rom_size}"
        )
    if rom_size % 4 != 0:
        errors.append(f"ROM size must be 4-byte aligned, found {rom_size} bytes")

    if rom_size >= ROM_HEADER_SIZE:
        if rom[0x04:0xA0] != NINTENDO_LOGO:
            errors.append("ROM Nintendo logo bytes do not match the GBA header logo")
        if rom[0xA0:0xAC] != EXPECTED_TITLE:
            errors.append("ROM title must be LINE NOISE! followed by one NUL padding byte")
        if rom[0xAC:0xB0] != EXPECTED_GAME_CODE:
            errors.append("ROM game code must be LNSE")
        if rom[0xB0:0xB2] != EXPECTED_MAKER_CODE:
            errors.append("ROM maker code must be 00")
        if rom[0xB2] != EXPECTED_FIXED_BYTE:
            errors.append(
                f"ROM fixed byte must be 0x{EXPECTED_FIXED_BYTE:02X}, "
                f"found 0x{rom[0xB2]:02X}"
            )

        expected_complement = calculate_header_complement(rom)
        if rom[0xBD] != expected_complement:
            errors.append(
                "ROM complement checksum mismatch: "
                f"expected 0x{expected_complement:02X}, found 0x{rom[0xBD]:02X}"
            )
    else:
        expected_complement = 0

    if errors:
        raise ValidationError(errors)

    return RomInfo(
        size=rom_size,
        title=EXPECTED_DISPLAY_TITLE,
        game_code=EXPECTED_GAME_CODE.decode("ascii"),
        maker_code=EXPECTED_MAKER_CODE.decode("ascii"),
        complement=expected_complement,
    )


def validate_elf_bytes(elf: bytes) -> ElfInfo:
    """Validate a devkitARM ELF32 ARM EABI5 executable header."""
    errors: list[str] = []
    elf_size = len(elf)

    if elf_size < ELF_HEADER_SIZE:
        raise ValidationError(
            f"ELF is too small for an ELF32 header: found {elf_size} bytes"
        )

    if elf[0:4] != ELF_MAGIC:
        errors.append("ELF magic is invalid")
    if elf[4] != ELFCLASS32:
        errors.append(f"ELF class must be ELF32, found class {elf[4]}")
    if elf[5] != ELFDATA2LSB:
        errors.append(f"ELF byte order must be little-endian, found encoding {elf[5]}")
    if elf[6] != EV_CURRENT:
        errors.append(f"ELF identification version must be 1, found {elf[6]}")

    can_parse_header = (
        elf[0:4] == ELF_MAGIC
        and elf[4] == ELFCLASS32
        and elf[5] == ELFDATA2LSB
    )
    if not can_parse_header:
        raise ValidationError(errors)

    (
        elf_type,
        machine,
        version,
        entry_point,
        program_header_offset,
        _section_header_offset,
        flags,
        header_size,
        program_header_entry_size,
        program_header_count,
        _section_header_entry_size,
        _section_header_count,
        _section_name_index,
    ) = struct.unpack_from("<HHIIIIIHHHHHH", elf, 16)

    if elf_type != ET_EXEC:
        errors.append(f"ELF type must be executable, found {elf_type}")
    if machine != EM_ARM:
        errors.append(f"ELF machine must be ARM ({EM_ARM}), found {machine}")
    if version != EV_CURRENT:
        errors.append(f"ELF header version must be 1, found {version}")
    if header_size != ELF_HEADER_SIZE:
        errors.append(
            f"ELF header size must be {ELF_HEADER_SIZE} bytes, found {header_size}"
        )

    eabi_bits = flags & EF_ARM_EABIMASK
    if eabi_bits != EF_ARM_EABI_VER5:
        errors.append(
            "ELF ARM ABI must be EABI5: "
            f"expected flags 0x{EF_ARM_EABI_VER5:08X}, found 0x{eabi_bits:08X}"
        )

    if entry_point == 0:
        errors.append("ELF entry point must be nonzero")
    if program_header_count == 0:
        errors.append("ELF must contain at least one program header")
    if program_header_entry_size != ELF_PROGRAM_HEADER_SIZE:
        errors.append(
            "ELF program header size must be "
            f"{ELF_PROGRAM_HEADER_SIZE} bytes, found {program_header_entry_size}"
        )
    if program_header_offset < ELF_HEADER_SIZE:
        errors.append(
            "ELF program header table must begin after the ELF header, "
            f"found offset {program_header_offset}"
        )

    program_table_size = program_header_entry_size * program_header_count
    program_table_end = program_header_offset + program_table_size
    can_parse_program_headers = (
        program_header_count > 0
        and program_header_entry_size == ELF_PROGRAM_HEADER_SIZE
        and program_header_offset >= ELF_HEADER_SIZE
        and program_table_end <= elf_size
    )
    if program_header_count > 0 and program_table_end > elf_size:
        errors.append(
            "ELF program header table extends past end of file: "
            f"ends at {program_table_end}, file has {elf_size} bytes"
        )

    load_segment_count = 0
    executable_entry_segment = False
    if can_parse_program_headers:
        for index in range(program_header_count):
            offset = program_header_offset + index * program_header_entry_size
            (
                segment_type,
                file_offset,
                virtual_address,
                _physical_address,
                file_size,
                memory_size,
                segment_flags,
                _alignment,
            ) = struct.unpack_from("<IIIIIIII", elf, offset)
            if segment_type != PT_LOAD:
                continue

            load_segment_count += 1
            if file_size > memory_size:
                errors.append(
                    f"ELF PT_LOAD segment {index} has file size {file_size} "
                    f"larger than memory size {memory_size}"
                )
            if file_offset > elf_size or file_size > elf_size - file_offset:
                errors.append(
                    f"ELF PT_LOAD segment {index} extends past end of file"
                )

            segment_end = virtual_address + memory_size
            if segment_end > 0x100000000:
                errors.append(
                    f"ELF PT_LOAD segment {index} address range overflows 32 bits"
                )
            file_backed_end = virtual_address + file_size
            entry_address = entry_point & ~1
            if (
                segment_flags & PF_X
                and file_size > 0
                and file_backed_end <= 0x100000000
                and virtual_address <= entry_address < file_backed_end
            ):
                executable_entry_segment = True

        if load_segment_count == 0:
            errors.append("ELF must contain at least one PT_LOAD segment")
        elif not executable_entry_segment:
            errors.append(
                "ELF entry point must be covered by an executable PT_LOAD segment"
            )

    if errors:
        raise ValidationError(errors)

    return ElfInfo(
        size=elf_size,
        entry_point=entry_point,
        flags=flags,
        eabi_version=5,
        load_segment_count=load_segment_count,
    )


def _read_artifact(path: PathInput, label: str) -> bytes:
    artifact_path = Path(path)
    try:
        return artifact_path.read_bytes()
    except OSError as error:
        detail = error.strerror or str(error)
        raise ValidationError(f"cannot read {label} {artifact_path}: {detail}") from error


def validate_rom_file(path: PathInput) -> RomInfo:
    """Read and validate a GBA ROM file."""
    return validate_rom_bytes(_read_artifact(path, "ROM"))


def validate_elf_file(path: PathInput) -> ElfInfo:
    """Read and validate an ELF file."""
    return validate_elf_bytes(_read_artifact(path, "ELF"))


def _print_failure(label: str, error: ValidationError) -> None:
    for detail in error.errors:
        print(f"FAIL {label}: {detail}", file=sys.stderr)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate Line Noise GBA ROM and ELF release artifacts."
    )
    parser.add_argument("rom", help="path to the line-noise.gba ROM")
    parser.add_argument("elf", help="path to the line-noise.elf debug executable")
    args = parser.parse_args(argv)

    failed = False
    try:
        rom_info = validate_rom_file(args.rom)
    except ValidationError as error:
        _print_failure("ROM", error)
        failed = True
    else:
        print(f"PASS ROM size: {rom_info.size} bytes, 4-byte aligned")
        print(
            "PASS ROM header: Nintendo logo, "
            f"{rom_info.title}/{rom_info.game_code}/{rom_info.maker_code}"
        )
        print(f"PASS ROM checksum: 0x{rom_info.complement:02X}")

    try:
        elf_info = validate_elf_file(args.elf)
    except ValidationError as error:
        _print_failure("ELF", error)
        failed = True
    else:
        print(
            "PASS ELF: ELF32 little-endian ARM EABI"
            f"{elf_info.eabi_version} executable, {elf_info.size} bytes"
        )

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
