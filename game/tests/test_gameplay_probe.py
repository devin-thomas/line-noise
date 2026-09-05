from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]


class GameplayProbeTests(unittest.TestCase):
    def test_gameplay_state_machine(self) -> None:
        with tempfile.TemporaryDirectory(prefix="line-noise-probe-") as temp_dir:
            executable = Path(temp_dir) / "gameplay-probe"
            command = [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-pedantic",
                "-DLN_GAME_DEBUG",
                f"-I{PROJECT_ROOT / 'tests' / 'host'}",
                f"-I{PROJECT_ROOT / 'include'}",
                str(PROJECT_ROOT / "source" / "game.c"),
                str(PROJECT_ROOT / "source" / "content.c"),
                str(PROJECT_ROOT / "tests" / "host" / "stubs.c"),
                str(PROJECT_ROOT / "tests" / "gameplay_probe.c"),
                "-o",
                str(executable),
            ]
            compiled = subprocess.run(
                command,
                cwd=PROJECT_ROOT,
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                compiled.returncode,
                0,
                msg=compiled.stdout + compiled.stderr,
            )
            completed = subprocess.run(
                [str(executable)],
                cwd=PROJECT_ROOT,
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                completed.returncode,
                0,
                msg=completed.stdout + completed.stderr,
            )

        self.assertIn("gameplay probe: PASS", completed.stdout)


if __name__ == "__main__":
    unittest.main()
