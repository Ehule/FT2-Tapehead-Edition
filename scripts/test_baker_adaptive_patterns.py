#!/usr/bin/env python3
"""Compile and run the isolated adaptive-XM pattern packing tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-baker-adaptive-patterns-") as tmp:
        executable = Path(tmp) / "test_baker_adaptive_patterns"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "src"),
                str(ROOT / "tests/test_baker_adaptive_patterns.c"),
                str(ROOT / "src/ft2_baker_adaptive_patterns.c"),
                str(ROOT / "src/ft2_baker_adaptive_xm.c"),
                str(ROOT / "src/ft2_baker_timeline_planner.c"),
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
