#!/usr/bin/env python3
"""Compile and run native paired-instrument Sample Bank tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-sample-banks-") as tmp:
        executable = Path(tmp) / "test_sample_launcher_banks"
        subprocess.run(
            [
                "gcc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{ROOT / 'vs2026_project/ft2-clone/sdl/include'}",
                f"-I{ROOT / 'src'}",
                str(ROOT / "tests/test_sample_launcher_banks.c"),
                str(ROOT / "src/ft2_sample_launcher.c"),
                str(ROOT / "src/ft2_sample_launcher_state.c"),
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
