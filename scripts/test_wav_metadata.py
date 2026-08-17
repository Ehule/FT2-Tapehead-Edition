#!/usr/bin/env python3
"""Compile and run WAV smpl metadata regression tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-wav-metadata-") as tmp:
        executable = Path(tmp) / "test_wav_metadata"
        subprocess.run(
            [
                "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{ROOT / 'src'}", str(ROOT / "tests/test_wav_metadata.c"),
                "-lm", "-o", str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable)], check=True, cwd=ROOT)
    print("WAV metadata tests passed.")


if __name__ == "__main__":
    main()
