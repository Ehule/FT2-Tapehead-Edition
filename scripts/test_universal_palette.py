#!/usr/bin/env python3
"""Compile and run the reciprocal TapeSister/Tapehead palette tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="tapehead-universal-palette-") as tmp:
        binary = Path(tmp) / "test_universal_palette"
        subprocess.run(
            [
                "cc",
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Wpedantic",
                "-Werror",
                "-I",
                str(ROOT / "src"),
                str(ROOT / "tests" / "test_universal_palette.c"),
                str(ROOT / "src" / "ft2_universal_palette.c"),
                "-o",
                str(binary),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run(
            [str(binary), str(ROOT / "release" / "other" / "palette.pal"), tmp],
            check=True,
            cwd=ROOT,
        )


if __name__ == "__main__":
    main()
