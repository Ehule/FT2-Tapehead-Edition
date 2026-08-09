#!/usr/bin/env python3
"""Compile and run the dedicated control-surface MIDI port tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
	with tempfile.TemporaryDirectory(prefix="ft2-midi-surface-") as tmp:
		executable = Path(tmp) / "test_midi_surface"
		subprocess.run(
			[
				"cc",
				"-std=c11",
				"-DHAS_MIDI",
				"-Wall",
				"-Wextra",
				"-Werror",
				"-I",
				str(ROOT / "src"),
				"-I",
				str(ROOT / "vs2026_project" / "ft2-clone" / "sdl" / "include"),
				str(ROOT / "tests" / "test_midi_surface.c"),
				str(ROOT / "src" / "ft2_midi_surface.c"),
				"-o",
				str(executable),
			],
			check=True,
			cwd=ROOT,
		)
		subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
	main()
