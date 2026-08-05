#!/usr/bin/env python3
"""Compile and run the native Sample Matrix file-browser test."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-matrix-browser-") as tmp:
        temporary = Path(tmp)
        samples = temporary / "samples"
        samples.mkdir()
        (samples / "subfolder").mkdir()
        (samples / "subfolder" / "tom3.iff").touch()
        for name in ("kick10.wav", "kick2.wav", "kick1.wav", "snare.aiff", "notes.txt"):
            (samples / name).touch()

        executable = temporary / "test_sample_matrix_browser"
        subprocess.run(
            [
                "gcc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{ROOT / 'vs2026_project/ft2-clone/sdl/include'}",
                f"-I{ROOT / 'src'}",
                str(ROOT / "tests/test_sample_matrix_browser.c"),
                str(ROOT / "src/ft2_sample_matrix_editor.c"),
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable), str(samples)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
