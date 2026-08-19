#!/usr/bin/env python3
"""Compile and run video damage/scaler equivalence tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-video-damage-") as temp_dir:
        output = Path(temp_dir) / "test_video_damage"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "src"),
                str(ROOT / "tests/test_video_damage.c"),
                "-o",
                str(output),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(output)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
