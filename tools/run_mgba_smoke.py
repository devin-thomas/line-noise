#!/usr/bin/env python3
"""Run the native mGBA smoke test in an isolated, validated workspace."""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Sequence


ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "game"
DEFAULT_ROM = GAME / "line-noise.gba"
DEFAULT_ELF = GAME / "line-noise.elf"
DEFAULT_LUA = GAME / "tests" / "mgba_smoke.lua"
DEFAULT_OUTPUT = GAME / "validation" / "native"
DEFAULT_MGBA = Path(
    os.environ.get("MGBA")
    or shutil.which("mGBA")
    or "/Applications/mGBA.app/Contents/MacOS/mGBA"
)
DEFAULT_DEVKITARM = Path("/opt/devkitpro/devkitARM")

DEBUG_PROBE_VERSION = 1
EWRAM_START = 0x02000000
EWRAM_END = 0x02040000
EXPECTED_SCREENSHOTS = (
    "01-title.png",
    "02-mode-select.png",
    "03-shift-brief.png",
    "04-gameplay.png",
    "05-pause.png",
    "06-city-memory.png",
    "07-shift-report.png",
    "08-epilogue.png",
    "09-playback.png",
    "10-four-live-cords.png",
)
REVIEWED_SCREENSHOT_SHA256 = {
    "10-four-live-cords.png": (
        "464bd7954e7e0277afc300bc38d37d6f75a44f5b5ed0ca1a94b43aa6f472463c"
    )
}
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


@dataclass(frozen=True)
class PngImage:
    width: int
    height: int
    channels: int
    pixels: bytes
    sha256: str


@dataclass(frozen=True)
class DwarfStruct:
    byte_size: int
    members: Mapping[str, int]


@dataclass(frozen=True)
class DebugLayout:
    state_address: int
    state_size: int
    calls_offset: int
    call_stride: int
    call_state_offset: int
    cords_offset: int
    cord_stride: int
    cord_live_offset: int
    crossing_count_offset: int


def _attribute_value(lines: Sequence[str], attribute: str) -> str | None:
    for line in lines:
        marker = f"DW_AT_{attribute}"
        if marker not in line:
            continue
        value = line.split(marker, 1)[1].strip()
        if ":" in value:
            value = value.rsplit(":", 1)[1].strip()
        return value
    return None


def _parse_integer(value: str | None, label: str) -> int:
    if value is None:
        raise RuntimeError(f"DWARF {label} is missing")
    token = value.split()[0]
    try:
        return int(token, 0)
    except ValueError as error:
        raise RuntimeError(f"DWARF {label} is not a constant: {value}") from error


def _parse_dwarf_structs(dwarf_output: str) -> Mapping[str, tuple[DwarfStruct, ...]]:
    lines = dwarf_output.splitlines()
    die_pattern = re.compile(r"^\s*<(\d+)><[0-9a-fA-F]+>:")
    entries: list[tuple[int, str | None, int, int]] = []
    starts: list[tuple[int, str | None, int]] = []
    for index, line in enumerate(lines):
        match = die_pattern.match(line)
        if match is None:
            continue
        tag_match = re.search(r"\((DW_TAG_[^)]+)\)", line)
        starts.append(
            (int(match.group(1)), tag_match.group(1) if tag_match else None, index)
        )
    for index, (depth, tag, start) in enumerate(starts):
        end = starts[index + 1][2] if index + 1 < len(starts) else len(lines)
        entries.append((depth, tag, start, end))

    structures: dict[str, list[DwarfStruct]] = {}
    for index, (depth, tag, start, end) in enumerate(entries):
        if tag != "DW_TAG_structure_type":
            continue
        name = _attribute_value(lines[start:end], "name")
        size_value = _attribute_value(lines[start:end], "byte_size")
        if not name or size_value is None:
            continue

        members: dict[str, int] = {}
        child_index = index + 1
        while child_index < len(entries):
            child_depth, child_tag, child_start, child_end = entries[child_index]
            if child_depth <= depth:
                break
            if child_depth == depth + 1 and child_tag == "DW_TAG_member":
                member_name = _attribute_value(lines[child_start:child_end], "name")
                location = _attribute_value(
                    lines[child_start:child_end], "data_member_location"
                )
                if member_name and location is not None:
                    members[member_name] = _parse_integer(
                        location, f"{name}.{member_name} location"
                    )
            child_index += 1

        structure = DwarfStruct(
            byte_size=_parse_integer(size_value, f"{name} byte size"),
            members=members,
        )
        structures.setdefault(name, []).append(structure)
    return {name: tuple(values) for name, values in structures.items()}


def _require_structure(
    structures: Mapping[str, tuple[DwarfStruct, ...]],
    name: str,
    required_members: Sequence[str],
) -> DwarfStruct:
    candidates = [
        candidate
        for candidate in structures.get(name, ())
        if all(member in candidate.members for member in required_members)
    ]
    unique = {
        (candidate.byte_size, tuple(sorted(candidate.members.items()))): candidate
        for candidate in candidates
    }
    if len(unique) != 1:
        raise RuntimeError(
            f"expected one complete DWARF {name} definition, found {len(unique)}"
        )
    return next(iter(unique.values()))


def parse_debug_layout(symbol_output: str, dwarf_output: str) -> DebugLayout:
    """Derive the version-1 Lua probe layout from the current debug ELF."""
    symbol_pattern = re.compile(
        r"^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+[bBdD]\s+s_state$",
        re.MULTILINE,
    )
    symbols = symbol_pattern.findall(symbol_output)
    if len(symbols) != 1:
        raise RuntimeError(f"expected one sized s_state symbol, found {len(symbols)}")
    state_address, state_size = (int(value, 16) for value in symbols[0])
    if (
        state_size <= 0
        or state_address < EWRAM_START
        or state_address + state_size > EWRAM_END
    ):
        raise RuntimeError(
            "s_state must be a nonempty object wholly inside GBA EWRAM, "
            f"found 0x{state_address:08X}+0x{state_size:X}"
        )

    structures = _parse_dwarf_structs(dwarf_output)
    state = _require_structure(
        structures, "LnState", ("calls", "cords", "crossing_count")
    )
    call = _require_structure(structures, "LnCallRuntime", ("state",))
    cord = _require_structure(structures, "LnCord", ("live",))
    if state.byte_size != state_size:
        raise RuntimeError(
            "s_state symbol size does not match DWARF LnState size: "
            f"{state_size} != {state.byte_size}"
        )

    layout = DebugLayout(
        state_address=state_address,
        state_size=state_size,
        calls_offset=state.members["calls"],
        call_stride=call.byte_size,
        call_state_offset=call.members["state"],
        cords_offset=state.members["cords"],
        cord_stride=cord.byte_size,
        cord_live_offset=cord.members["live"],
        crossing_count_offset=state.members["crossing_count"],
    )
    if layout.call_stride <= 0 or not 0 <= layout.call_state_offset < layout.call_stride:
        raise RuntimeError("DWARF LnCallRuntime probe fields are out of bounds")
    if layout.cord_stride <= 0 or not 0 <= layout.cord_live_offset < layout.cord_stride:
        raise RuntimeError("DWARF LnCord probe fields are out of bounds")
    if not 0 <= layout.crossing_count_offset < state_size:
        raise RuntimeError("DWARF LnState.crossing_count is out of bounds")
    if layout.calls_offset < 0 or layout.calls_offset + 4 * layout.call_stride > state_size:
        raise RuntimeError("DWARF LnState.calls probe range is out of bounds")
    if layout.cords_offset < 0 or layout.cords_offset + 4 * layout.cord_stride > state_size:
        raise RuntimeError("DWARF LnState.cords probe range is out of bounds")
    return layout


def load_debug_layout(nm: Path, readelf: Path, elf: Path) -> DebugLayout:
    try:
        symbol_result = subprocess.run(
            [str(nm), "-S", "-an", str(elf)],
            check=True,
            capture_output=True,
            text=True,
        )
        dwarf_result = subprocess.run(
            [str(readelf), "--debug-dump=info", str(elf)],
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or error.stdout or str(error)).strip()
        raise RuntimeError(f"failed to inspect debug ELF: {detail}") from error
    return parse_debug_layout(symbol_result.stdout, dwarf_result.stdout)


def _paeth_predictor(left: int, above: int, upper_left: int) -> int:
    estimate = left + above - upper_left
    left_distance = abs(estimate - left)
    above_distance = abs(estimate - above)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above
    return upper_left


def _decode_scanlines(compressed: bytes, width: int, height: int, channels: int) -> bytes:
    decompressor = zlib.decompressobj()
    stride = width * channels
    expected_size = height * (stride + 1)
    try:
        raw = decompressor.decompress(compressed, expected_size + 1)
    except zlib.error as error:
        raise RuntimeError(f"PNG IDAT data is invalid: {error}") from error
    if len(raw) > expected_size or decompressor.unconsumed_tail:
        raise RuntimeError("PNG scanline data exceeds the expected image size")
    try:
        raw += decompressor.flush(expected_size + 1 - len(raw))
    except zlib.error as error:
        raise RuntimeError(f"PNG IDAT data is invalid: {error}") from error
    if not decompressor.eof or decompressor.unused_data:
        raise RuntimeError("PNG IDAT zlib stream is incomplete or has trailing data")

    if len(raw) != expected_size:
        raise RuntimeError(
            f"PNG scanline data must be {expected_size} bytes, found {len(raw)}"
        )
    pixels = bytearray(height * stride)
    source_offset = 0
    for row_index in range(height):
        filter_type = raw[source_offset]
        source_offset += 1
        if filter_type > 4:
            raise RuntimeError(f"PNG uses invalid row filter {filter_type}")
        row_offset = row_index * stride
        previous_offset = row_offset - stride
        for column in range(stride):
            encoded = raw[source_offset + column]
            left = pixels[row_offset + column - channels] if column >= channels else 0
            above = pixels[previous_offset + column] if row_index > 0 else 0
            upper_left = (
                pixels[previous_offset + column - channels]
                if row_index > 0 and column >= channels
                else 0
            )
            if filter_type == 0:
                value = encoded
            elif filter_type == 1:
                value = encoded + left
            elif filter_type == 2:
                value = encoded + above
            elif filter_type == 3:
                value = encoded + ((left + above) // 2)
            else:
                value = encoded + _paeth_predictor(left, above, upper_left)
            pixels[row_offset + column] = value & 0xFF
        source_offset += stride
    return bytes(pixels)


def validate_png(path: Path) -> PngImage:
    """Validate and fully decode an un-interlaced 240x160 8-bit RGB PNG."""
    try:
        data = path.read_bytes()
    except OSError as error:
        raise RuntimeError(f"cannot read screenshot {path}: {error}") from error
    if not data.startswith(PNG_SIGNATURE):
        raise RuntimeError(f"{path.name} is not a PNG file")

    offset = len(PNG_SIGNATURE)
    header: tuple[int, int, int, int, int, int, int] | None = None
    idat_parts: list[bytes] = []
    saw_end = False
    chunk_index = 0
    while offset < len(data):
        if len(data) - offset < 12:
            raise RuntimeError(f"{path.name} has a truncated PNG chunk")
        chunk_size = struct.unpack_from(">I", data, offset)[0]
        chunk_type = data[offset + 4 : offset + 8]
        chunk_end = offset + 12 + chunk_size
        if chunk_end > len(data):
            raise RuntimeError(f"{path.name} has a truncated PNG chunk payload")
        payload = data[offset + 8 : offset + 8 + chunk_size]
        expected_crc = struct.unpack_from(">I", data, offset + 8 + chunk_size)[0]
        actual_crc = zlib.crc32(chunk_type + payload) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            chunk_label = chunk_type.decode("ascii", "replace")
            raise RuntimeError(f"{path.name} PNG {chunk_label} CRC mismatch")

        if chunk_index == 0 and chunk_type != b"IHDR":
            raise RuntimeError(f"{path.name} PNG first chunk is not IHDR")
        if chunk_type == b"IHDR":
            if header is not None or chunk_size != 13:
                raise RuntimeError(f"{path.name} has an invalid PNG IHDR chunk")
            header = struct.unpack(">IIBBBBB", payload)
        elif chunk_type == b"IDAT":
            if header is None or saw_end:
                raise RuntimeError(f"{path.name} has a misplaced PNG IDAT chunk")
            idat_parts.append(payload)
        elif chunk_type == b"IEND":
            if chunk_size != 0:
                raise RuntimeError(f"{path.name} has an invalid PNG IEND chunk")
            saw_end = True
            offset = chunk_end
            if offset != len(data):
                raise RuntimeError(f"{path.name} has data after PNG IEND")
            break
        offset = chunk_end
        chunk_index += 1

    if header is None or not idat_parts or not saw_end:
        raise RuntimeError(f"{path.name} is missing required PNG chunks")
    width, height, bit_depth, color_type, compression, filtering, interlace = header
    if (width, height) != (240, 160):
        raise RuntimeError(
            f"{path.name} dimensions must be 240x160, found {width}x{height}"
        )
    if bit_depth != 8 or color_type != 2:
        raise RuntimeError(
            f"{path.name} must be an 8-bit RGB PNG, found depth {bit_depth}, "
            f"color type {color_type}"
        )
    if compression != 0 or filtering != 0 or interlace != 0:
        raise RuntimeError(f"{path.name} uses unsupported PNG encoding options")

    pixels = _decode_scanlines(b"".join(idat_parts), width, height, 3)
    return PngImage(
        width=width,
        height=height,
        channels=3,
        pixels=pixels,
        sha256=hashlib.sha256(data).hexdigest(),
    )


def validate_screenshot_set(
    directory: Path,
    expected_names: Sequence[str] = EXPECTED_SCREENSHOTS,
    *,
    golden_hashes: Mapping[str, str] = REVIEWED_SCREENSHOT_SHA256,
) -> Mapping[str, PngImage]:
    actual_names = tuple(sorted(path.name for path in directory.glob("*.png")))
    expected_sorted = tuple(sorted(expected_names))
    if actual_names != expected_sorted:
        missing = sorted(set(expected_names) - set(actual_names))
        extra = sorted(set(actual_names) - set(expected_names))
        raise RuntimeError(f"screenshot set mismatch; missing={missing}, extra={extra}")

    images: dict[str, PngImage] = {}
    decoded_hashes: dict[str, str] = {}
    for name in expected_names:
        image = validate_png(directory / name)
        pixel_values = {
            image.pixels[index : index + image.channels]
            for index in range(0, len(image.pixels), image.channels)
        }
        if len(pixel_values) < 4:
            raise RuntimeError(f"{name} appears blank or nearly blank")
        decoded_hash = hashlib.sha256(image.pixels).hexdigest()
        if decoded_hash in decoded_hashes:
            raise RuntimeError(
                f"duplicate screenshots: {decoded_hashes[decoded_hash]} and {name}"
            )
        decoded_hashes[decoded_hash] = name
        images[name] = image

    for name, expected_hash in golden_hashes.items():
        if name not in images:
            raise RuntimeError(f"reviewed SHA-256 references missing screenshot {name}")
        actual_hash = images[name].sha256
        if actual_hash != expected_hash:
            raise RuntimeError(
                f"{name} does not match reviewed SHA-256: expected {expected_hash}, "
                f"found {actual_hash}"
            )
    return images


def _run_checked(command: Sequence[str], label: str) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(command, check=False, capture_output=True, text=True)
    except OSError as error:
        raise RuntimeError(f"cannot run {label}: {error}") from error


def ensure_no_running_mgba() -> None:
    result = _run_checked(("pgrep", "-x", "mGBA"), "pgrep")
    if result.returncode == 0:
        pids = ", ".join(result.stdout.split()) or "unknown"
        raise RuntimeError(
            f"mGBA is already running (PID {pids}); close it before the smoke test"
        )
    if result.returncode != 1:
        detail = (result.stderr or result.stdout).strip()
        raise RuntimeError(f"cannot check for running mGBA processes: {detail}")


def ensure_accessibility() -> None:
    result = _run_checked(
        (
            "osascript",
            "-e",
            'tell application "System Events" to return UI elements enabled',
        ),
        "AppleScript accessibility check",
    )
    if result.returncode != 0 or result.stdout.strip().lower() != "true":
        detail = (result.stderr or result.stdout).strip()
        raise RuntimeError(
            "macOS Accessibility access is required for mGBA scripting automation"
            + (f": {detail}" if detail else "")
        )


SCRIPT_LOADER = r'''
on run argv
    set scriptPath to item 1 of argv
    set targetPid to (item 2 of argv) as integer
    tell application "System Events"
        set targetProcess to missing value
        repeat 200 times
            set matches to every application process whose unix id is targetPid
            if (count of matches) is 1 then
                set targetProcess to item 1 of matches
                exit repeat
            end if
            delay 0.1
        end repeat
        if targetProcess is missing value then error "mGBA process did not register with System Events"

        tell targetProcess
            set frontmost to true
            set toolsReady to false
            repeat 200 times
                if exists menu bar item "Tools" of menu bar 1 then
                    if exists menu item "Scripting..." of menu 1 of menu bar item "Tools" of menu bar 1 then
                        set toolsReady to true
                        exit repeat
                    end if
                end if
                delay 0.1
            end repeat
            if not toolsReady then error "mGBA Tools > Scripting menu did not become ready"
            click menu item "Scripting..." of menu 1 of menu bar item "Tools" of menu bar 1

            set scriptingReady to false
            repeat 200 times
                if exists window "Scripting" then
                    set scriptingReady to true
                    exit repeat
                end if
                delay 0.1
            end repeat
            if not scriptingReady then error "mGBA Scripting window did not open"
            perform action "AXRaise" of window "Scripting"
            set frontmost to true

            if not (exists menu item "Load script..." of menu 1 of menu bar item "File" of menu bar 1) then
                error "mGBA File > Load script... menu is unavailable"
            end if
            click menu item "Load script..." of menu 1 of menu bar item "File" of menu bar 1

            set filePanelReady to false
            repeat 100 times
                if exists sheet 1 of window "Scripting" then
                    set filePanelReady to true
                    exit repeat
                end if
                if exists window "Select script to load" then
                    set filePanelReady to true
                    exit repeat
                end if
                delay 0.1
            end repeat
            if not filePanelReady then error "mGBA script file panel did not open"
        end tell

        keystroke "g" using {command down, shift down}
        delay 0.3
        keystroke scriptPath
        key code 36
        delay 0.5
        key code 36
    end tell
end run
'''


def load_lua_script(lua: Path, pid: int) -> None:
    try:
        result = subprocess.run(
            ["osascript", "-", str(lua), str(pid)],
            input=SCRIPT_LOADER,
            check=False,
            capture_output=True,
            text=True,
            timeout=45,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise RuntimeError(f"mGBA script loader failed: {error}") from error
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise RuntimeError(f"mGBA script loader failed: {detail}")


def _atomic_copy(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            dir=destination.parent,
            prefix=f".{destination.name}.",
            suffix=".tmp",
            delete=False,
        ) as temporary:
            temporary_path = Path(temporary.name)
            with source.open("rb") as source_file:
                shutil.copyfileobj(source_file, temporary)
            temporary.flush()
            os.fsync(temporary.fileno())
        temporary_path.chmod(0o644)
        os.replace(temporary_path, destination)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def publish_results(staging: Path, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    for name in EXPECTED_SCREENSHOTS:
        _atomic_copy(staging / name, output / name)
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="ascii",
        dir=output,
        prefix=".mgba-smoke-result.",
        suffix=".tmp",
        delete=False,
    ) as result_file:
        result_path = Path(result_file.name)
        result_file.write("PASS\n")
        result_file.flush()
        os.fsync(result_file.fileno())
    try:
        result_path.chmod(0o644)
        os.replace(result_path, output / "mgba-smoke-result.txt")
    finally:
        result_path.unlink(missing_ok=True)


def _tail(path: Path, line_count: int = 40) -> str:
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return ""
    return "\n".join(lines[-line_count:])


def _wait_for_result(process: subprocess.Popen[bytes], result_path: Path, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if result_path.is_file():
            try:
                return result_path.read_text(encoding="utf-8")
            except OSError as error:
                raise RuntimeError(f"cannot read Lua smoke result: {error}") from error
        return_code = process.poll()
        if return_code is not None:
            raise RuntimeError(
                f"mGBA exited with status {return_code} before producing a result"
            )
        time.sleep(0.1)
    raise RuntimeError(f"timed out after {timeout:.0f}s waiting for Lua smoke result")


def _probe_environment(layout: DebugLayout) -> Mapping[str, str]:
    return {
        "LINE_NOISE_PROBE_VERSION": str(DEBUG_PROBE_VERSION),
        "LINE_NOISE_STATE": f"{layout.state_address:08X}",
        "LINE_NOISE_CALLS_OFFSET": str(layout.calls_offset),
        "LINE_NOISE_CALL_STRIDE": str(layout.call_stride),
        "LINE_NOISE_CALL_STATE_OFFSET": str(layout.call_state_offset),
        "LINE_NOISE_CORDS_OFFSET": str(layout.cords_offset),
        "LINE_NOISE_CORD_STRIDE": str(layout.cord_stride),
        "LINE_NOISE_CORD_LIVE_OFFSET": str(layout.cord_live_offset),
        "LINE_NOISE_CROSSING_COUNT_OFFSET": str(layout.crossing_count_offset),
    }


def _require_executable(path: Path, label: str) -> None:
    if not path.is_file() or not os.access(path, os.X_OK):
        raise RuntimeError(f"{label} is not executable: {path}")


def _require_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise RuntimeError(f"{label} does not exist: {path}")


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--mgba",
        type=Path,
        default=Path(os.environ.get("MGBA", DEFAULT_MGBA)),
        help="mGBA executable (default: MGBA or installed app binary)",
    )
    parser.add_argument(
        "--devkitarm",
        type=Path,
        default=Path(os.environ.get("DEVKITARM", DEFAULT_DEVKITARM)),
        help="devkitARM root (default: DEVKITARM or /opt/devkitpro/devkitARM)",
    )
    parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
    parser.add_argument("--elf", type=Path, default=DEFAULT_ELF)
    parser.add_argument("--lua", type=Path, default=DEFAULT_LUA)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--timeout", type=float, default=90.0)
    return parser.parse_args(argv)


def run_smoke(args: argparse.Namespace) -> None:
    if sys.platform != "darwin":
        raise RuntimeError("native mGBA smoke automation requires macOS")
    if args.timeout <= 0:
        raise RuntimeError("--timeout must be positive")
    nm = args.devkitarm / "bin" / "arm-none-eabi-nm"
    readelf = args.devkitarm / "bin" / "arm-none-eabi-readelf"
    _require_executable(args.mgba, "mGBA")
    _require_executable(nm, "arm-none-eabi-nm")
    _require_executable(readelf, "arm-none-eabi-readelf")
    _require_file(args.rom, "ROM")
    _require_file(args.elf, "ELF")
    _require_file(args.lua, "Lua smoke script")
    layout = load_debug_layout(nm, readelf, args.elf)
    ensure_no_running_mgba()
    ensure_accessibility()

    with tempfile.TemporaryDirectory(prefix="line-noise-mgba-") as temporary:
        sandbox = Path(temporary)
        config = sandbox / "config"
        staging = sandbox / "screenshots"
        saves = sandbox / "saves"
        states = sandbox / "states"
        cheats = sandbox / "cheats"
        result_path = sandbox / "mgba-smoke-result.txt"
        log_path = sandbox / "mgba.log"
        for directory in (config, staging, saves, states, cheats):
            directory.mkdir()

        environment = os.environ.copy()
        environment.update(_probe_environment(layout))
        environment.update(
            {
                "XDG_CONFIG_HOME": str(config),
                "LINE_NOISE_OUTPUT": str(staging),
                "LINE_NOISE_RESULT": str(result_path),
            }
        )
        command = [
            str(args.mgba),
            "-2",
            "-C",
            "mute=1",
            "-C",
            "pauseOnFocusLost=0",
            "-C",
            "autoload=0",
            "-C",
            "autosave=0",
            "-C",
            f"screenshotPath={staging}",
            "-C",
            f"savegamePath={saves}",
            "-C",
            f"savestatePath={states}",
            "-C",
            f"cheatsPath={cheats}",
            str(args.rom),
        ]
        process: subprocess.Popen[bytes] | None = None
        try:
            with log_path.open("wb") as log_file:
                process = subprocess.Popen(
                    command,
                    env=environment,
                    stdout=log_file,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,
                )
                load_lua_script(args.lua, process.pid)
                result = _wait_for_result(process, result_path, args.timeout)
            if result != "PASS\n":
                raise RuntimeError(f"Lua smoke test failed:\n{result.rstrip()}")
            validate_screenshot_set(staging)
            publish_results(staging, args.output)
        except (OSError, RuntimeError, subprocess.SubprocessError) as error:
            diagnostics = _tail(log_path)
            if diagnostics:
                raise RuntimeError(f"{error}\nLast mGBA log lines:\n{diagnostics}") from error
            raise
        finally:
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        run_smoke(args)
    except RuntimeError as error:
        print(f"FAIL mGBA smoke: {error}", file=sys.stderr)
        return 1
    print(
        "PASS mGBA smoke: screens, live routing probe, and reviewed screenshot "
        f"published to {args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
