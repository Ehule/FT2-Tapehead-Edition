#!/usr/bin/env python3
"""Compile and run generic MIDI mapping with injected MIDI events."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
	with tempfile.TemporaryDirectory(prefix="ft2-midi-map-") as tmp:
		executable = Path(tmp) / "test_midi_map"
		subprocess.run(
			[
				"cc",
				"-std=c11",
				"-Wall",
				"-Wextra",
				"-Werror",
				"-I",
				str(ROOT / "src"),
				"-I",
				str(ROOT / "vs2026_project" / "ft2-clone" / "sdl" / "include"),
				str(ROOT / "tests" / "test_midi_map.c"),
				str(ROOT / "src" / "ft2_midi_map.c"),
				str(ROOT / "src" / "ft2_tapehead_actions.c"),
				str(ROOT / "src" / "ft2_microtonal.c"),
				"-lm",
				"-o",
				str(executable),
			],
			check=True,
			cwd=ROOT,
		)
		subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
	main()
