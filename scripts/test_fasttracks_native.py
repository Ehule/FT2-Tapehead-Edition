#!/usr/bin/env python3
"""Compile and run the native FasTracks rational-clock parity tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-fasttracks-native-") as tmp:
        executable = Path(tmp) / "test_fasttracks_core"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                str(ROOT / "tests/test_fasttracks_core.c"),
                str(ROOT / "src/ft2_fasttracks_core.c"),
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
