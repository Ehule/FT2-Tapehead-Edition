#!/usr/bin/env python3
"""Compile and run the EXS v1 round-trip manifest contract tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-exs-manifest-") as tmp:
        executable = Path(tmp) / "test_exs_manifest"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "src"),
                str(ROOT / "tests/test_exs_manifest.c"),
                str(ROOT / "src/ft2_exs_manifest.c"),
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
