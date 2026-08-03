#!/usr/bin/env python3
"""Compile and run the native Tapehead output-routing tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-multichannel-native-") as tmp:
        executable = Path(tmp) / "test_multichannel_routing"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "vs2026_project/ft2-clone/sdl/include"),
                str(ROOT / "tests/test_multichannel_routing.c"),
                str(ROOT / "src/ft2_multichannel.c"),
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
