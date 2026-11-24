#!/usr/bin/env python3
"""Validate the LINE NOISE! GBA ROM and its symbol-bearing ELF.

The checks live in uppercut-gba-rom-verify, pinned in tools/requirements.txt. This file only
supplies the cartridge identity, so the Makefile's verify command is unchanged.
"""

from __future__ import annotations

import sys
from typing import Optional, Sequence

try:
    from uppercut_gba_rom_verify import RomConfig
    from uppercut_gba_rom_verify import main as verify_main
except ImportError as error:
    raise SystemExit(
        "uppercut-gba-rom-verify is not installed: pip install -r tools/requirements.txt"
    ) from error

CARTRIDGE = RomConfig(title="LINE NOISE!", game_code="LNSE", maker_code="00")


def main(argv: Optional[Sequence[str]] = None) -> int:
    arguments = list(sys.argv[1:] if argv is None else argv)
    return verify_main(
        [
            "--title",
            CARTRIDGE.title,
            "--game-code",
            CARTRIDGE.game_code,
            "--maker-code",
            CARTRIDGE.maker_code,
            *arguments,
        ]
    )


if __name__ == "__main__":
    raise SystemExit(main())
