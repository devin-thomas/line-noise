# LINE NOISE!

Playable Game Boy Advance vertical slice of Uppercut Labs' real-time
social-circuit strategy game.

## Build

The verified Mac toolchain lives under `/opt/devkitpro`.

```sh
DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make
```

This creates `line-noise.gba`, `line-noise.elf`, and `line-noise.map`. Open the
ROM with the registered emulator:

```sh
open line-noise.gba
```

## Controls

- D-pad: move between calls, crossings, bridge pegs, and City Memory.
- A: pick up, connect, unplug, confirm, or flip a crossing.
- B: hold to monitor a call; tap to repeat the last clear phrase.
- L: rehang the selected live cord over another peg.
- R: place or remove the one available ground clamp.
- Select: inspect City Memory.
- Start: pause and open objectives or accessibility settings.

Finite Final Week and Operator School shifts run to dawn. Open Lines is untimed
and exposes a manual `END SHIFT` action in the pause menu.

## Validate

From the repository root:

```sh
DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM make -C game verify size
python3 -m unittest discover -s game/tests -p 'test_*.py' -v
python3 tools/run_mgba_smoke.py
python3 tools/validate_assets_isolated.py
```

The host gameplay probe covers the authored coherent-dawn route and key state
regressions. The mGBA runner derives its probe layout from the current ELF,
drives the emulator frame by frame, rejects stale, blank, malformed, duplicate,
or unreviewed captures, and atomically publishes ten native screenshots.

## Scope

The supplied design pack contains one fully authored shift, `Platform Three`.
This build implements that production vertical slice with the core patch,
monitor, peg, over/under, clamp, crosstalk, knowledge, callback, report,
epilogue, and playback systems. The larger ten-shift campaign, eighteen
challenges, and 72-citizen pool remain production targets, not shipped content.
