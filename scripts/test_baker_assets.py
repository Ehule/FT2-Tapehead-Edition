#!/usr/bin/env python3
"""Compile and run Sample Morph Baker private-asset regressions."""

from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="ft2-baker-assets-") as tmp:
    executable = Path(tmp) / "test_baker_assets"
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(ROOT / "src"),
        str(ROOT / "tests/test_baker_assets.c"),
        str(ROOT / "src/ft2_baker_assets.c"),
        "-o", str(executable),
    ], check=True, cwd=ROOT)
    subprocess.run([str(executable)], check=True, cwd=ROOT)
