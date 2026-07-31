#!/usr/bin/env python3
"""Compile and run the native Poly Matrix routing/clock tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-poly-matrix-native-") as tmp:
        core_executable = Path(tmp) / "test_poly_matrix_core"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "vs2026_project/ft2-clone/sdl/include"),
                str(ROOT / "tests/test_poly_matrix_core.c"),
                str(ROOT / "src/ft2_poly_matrix.c"),
                "-o",
                str(core_executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(core_executable)], check=True, cwd=ROOT)

        boundary_executable = Path(tmp) / "test_pattern_launcher_poly_boundary"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "vs2026_project/ft2-clone/sdl/include"),
                str(ROOT / "tests/test_pattern_launcher_poly_boundary.c"),
                str(ROOT / "src/ft2_pattern_launcher.c"),
                "-o",
                str(boundary_executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(boundary_executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
