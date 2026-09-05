# Validation Record

Validated on 2026-08-14 using devkitARM r68, libgba 0.5.4, and mGBA 0.10.5.

## Release identity

| Check | Result |
| --- | --- |
| ROM | `game/line-noise.gba` |
| Size | `192,920` bytes, 4-byte aligned |
| SHA-256 | `ca365eb66dd22f720063fc3b58be07ba6e07e2b52eea0cea3592850dc2420f42` |
| Header | `LINE NOISE! / LNSE / 00` |
| Header checksum | `0xD8` |
| ELF | ARM EABI5 with executable `PT_LOAD` entry coverage |

## Automated evidence

- 22/22 host gameplay, mGBA-runner, ROM, and ELF regression tests pass.
- The gameplay probe reaches a coherent dawn and asserts all four objectives.
- The mGBA script traverses nine screens plus four-cord routing.
- Live masks advance through `1`, `3`, `7`, and `15`; crossing counts are
  `0`, `0`, `2`, and `3`.
- Ten screenshots decode as complete 240x160 PNGs and are pairwise distinct.
- The reviewed four-cord golden is
  `464bd7954e7e0277afc300bc38d37d6f75a44f5b5ed0ca1a94b43aa6f472463c`.
- The supplied asset generator/validator passes 956/956 checks in an isolated
  temporary copy, leaving the source pack unchanged.
- The site build, lint, server-rendered HTML tests, cartridge identity, and
  artifact-presence checks pass.

Canonical native captures are under `site/public/screens/`.

## Commands

```sh
DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make -C game verify size
python3 -m unittest discover -s game/tests -p 'test_*.py' -v
python3 tools/run_mgba_smoke.py
python3 tools/validate_assets_isolated.py
(cd site && npm run lint && npm test)
```

The mGBA runner uses the installed application binary directly, isolates save,
state, config, screenshot, result, and log paths, targets the launched process
by PID, and publishes evidence only after all assertions pass.

## Hardware-facing audit

- Bulk graphics loads use true GBA Forced Blank and enable the display only
  after dirty maps and OAM are presented during VBlank.
- VRAM clear DMA uses a fixed IWRAM source; screenblocks and character blocks do
  not overlap.
- OBJ 1D tile usage is 608/1024 tiles; EWRAM and IWRAM remain within budget.
- Audio WAVs are mono unsigned 8-bit PCM at 22050 Hz. DMA 1 feeds FIFO A from an
  aligned EWRAM buffer while Timer 0 clocks playback.
- All eight WAVs are explicit prerequisites of the embedded audio object.

This is emulator- and static-audit evidence, not a claim of physical-cartridge
hardware testing.
