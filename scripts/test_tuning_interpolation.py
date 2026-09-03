#!/usr/bin/env python3
"""Compile interpolation math and verify its pattern-editor wiring."""

import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="tapehead-tuning-interpolation-") as tmp:
    binary = Path(tmp) / "test_tuning_interpolation"
    subprocess.run(
        [
            "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-I", str(ROOT / "src"),
            str(ROOT / "tests/test_tuning_interpolation.c"),
            "-o", str(binary),
        ],
        check=True,
        cwd=ROOT,
    )
    subprocess.run([str(binary)], check=True, cwd=ROOT)

interpolation = (ROOT / "src/ft2_interpolation.c").read_text()
keyboard = (ROOT / "src/ft2_keyboard.c").read_text()
header = (ROOT / "src/ft2_interpolation.h").read_text()
docs = (ROOT / "docs/PATTERN_INTERPOLATION.md").read_text()

assert "INTERPOLATE_TUNING" in header
assert "microtonalLaneTypeIsValid(a->tuneType)" in interpolation
assert "n->tuneType != 0 || n->tuneData != 0" in interpolation
assert "n->tuneType = a->tuneType" in interpolation
assert "n->tuneData = value" in interpolation
assert "interpolationLinearByte" in interpolation
t_case = keyboard[keyboard.index("case SDLK_t:"):keyboard.index("case SDLK_u:")]
assert "keyb.leftCtrlPressed && keyb.leftShiftPressed" in t_case
assert "interpolationBegin(INTERPOLATE_TUNING)" in t_case
assert "Ctrl+Shift+T" in docs and "Mxx" in docs and "Nxx" in docs

print("Tuning-lane interpolation wiring checks passed.")
