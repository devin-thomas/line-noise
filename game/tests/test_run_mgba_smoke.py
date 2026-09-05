from __future__ import annotations

import hashlib
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT_ROOT))

from tools import run_mgba_smoke  # noqa: E402


def png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    checksum = zlib.crc32(chunk_type + payload) & 0xFFFFFFFF
    return (
        struct.pack(">I", len(payload))
        + chunk_type
        + payload
        + struct.pack(">I", checksum)
    )


def make_rgb_png(*, solid: bool = False) -> bytes:
    width, height = 240, 160
    rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            if solid:
                row.extend((7, 11, 13))
            else:
                row.extend((x & 0xFF, y & 0xFF, (x + y) & 0xFF))
        rows.append(b"\x00" + bytes(row))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(b"".join(rows)))
        + png_chunk(b"IEND", b"")
    )


class PngValidationTests(unittest.TestCase):
    def test_rejects_short_file_with_forged_dimensions(self) -> None:
        forged = (
            b"\x89PNG\r\n\x1a\n"
            + b"NOTIHDR!"
            + struct.pack(">II", 240, 160)
        )
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "forged.png"
            path.write_bytes(forged)

            with self.assertRaisesRegex(RuntimeError, "PNG|chunk|truncated"):
                run_mgba_smoke.validate_png(path)

    def test_decodes_complete_rgb_png(self) -> None:
        data = make_rgb_png()
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "screen.png"
            path.write_bytes(data)

            image = run_mgba_smoke.validate_png(path)

        self.assertEqual((image.width, image.height), (240, 160))
        self.assertEqual(image.sha256, hashlib.sha256(data).hexdigest())
        self.assertEqual(len(image.pixels), 240 * 160 * 3)

    def test_rejects_bad_chunk_crc(self) -> None:
        data = bytearray(make_rgb_png())
        data[29] ^= 0x01
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "bad-crc.png"
            path.write_bytes(data)

            with self.assertRaisesRegex(RuntimeError, "CRC"):
                run_mgba_smoke.validate_png(path)

    def test_rejects_blank_and_duplicate_screenshot_sets(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory / "a.png").write_bytes(make_rgb_png(solid=True))
            with self.assertRaisesRegex(RuntimeError, "blank"):
                run_mgba_smoke.validate_screenshot_set(
                    directory, ("a.png",), golden_hashes={}
                )

            image = make_rgb_png()
            (directory / "a.png").write_bytes(image)
            (directory / "b.png").write_bytes(image)
            with self.assertRaisesRegex(RuntimeError, "duplicate"):
                run_mgba_smoke.validate_screenshot_set(
                    directory, ("a.png", "b.png"), golden_hashes={}
                )

    def test_rejects_unreviewed_golden(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory / "screen.png").write_bytes(make_rgb_png())

            with self.assertRaisesRegex(RuntimeError, "reviewed SHA-256"):
                run_mgba_smoke.validate_screenshot_set(
                    directory,
                    ("screen.png",),
                    golden_hashes={"screen.png": "0" * 64},
                )


class DebugLayoutTests(unittest.TestCase):
    DWARF = """
 <1><100>: Abbrev Number: 1 (DW_TAG_structure_type)
    <101>   DW_AT_name        : LnCord
    <102>   DW_AT_byte_size   : 60
 <2><110>: Abbrev Number: 2 (DW_TAG_member)
    <111>   DW_AT_name        : live
    <112>   DW_AT_data_member_location: 0
 <2><113>: Abbrev Number: 0
 <1><120>: Abbrev Number: 1 (DW_TAG_structure_type)
    <121>   DW_AT_name        : LnCallRuntime
    <122>   DW_AT_byte_size   : 10
 <2><130>: Abbrev Number: 2 (DW_TAG_member)
    <131>   DW_AT_name        : state
    <132>   DW_AT_data_member_location: 0
 <2><133>: Abbrev Number: 0
 <1><140>: Abbrev Number: 1 (DW_TAG_structure_type)
    <141>   DW_AT_name        : (indirect string, offset: 0x10): LnState
    <142>   DW_AT_byte_size   : 2452
 <2><150>: Abbrev Number: 2 (DW_TAG_member)
    <151>   DW_AT_name        : calls
    <152>   DW_AT_data_member_location: 176
 <2><160>: Abbrev Number: 2 (DW_TAG_member)
    <161>   DW_AT_name        : cords
    <162>   DW_AT_data_member_location: 216
 <2><170>: Abbrev Number: 2 (DW_TAG_member)
    <171>   DW_AT_name        : crossing_count
    <172>   DW_AT_data_member_location: 536
 <2><173>: Abbrev Number: 0
"""

    def test_derives_probe_layout_from_symbols_and_dwarf(self) -> None:
        symbols = "02005c70 00000994 b s_state\n"

        layout = run_mgba_smoke.parse_debug_layout(symbols, self.DWARF)

        self.assertEqual(layout.state_address, 0x02005C70)
        self.assertEqual(layout.state_size, 0x994)
        self.assertEqual(layout.calls_offset, 176)
        self.assertEqual(layout.call_stride, 10)
        self.assertEqual(layout.cords_offset, 216)
        self.assertEqual(layout.cord_stride, 60)
        self.assertEqual(layout.crossing_count_offset, 536)

    def test_rejects_state_symbol_outside_ewram(self) -> None:
        symbols = "08000000 00000994 b s_state\n"

        with self.assertRaisesRegex(RuntimeError, "EWRAM"):
            run_mgba_smoke.parse_debug_layout(symbols, self.DWARF)


if __name__ == "__main__":
    unittest.main()
