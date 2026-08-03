#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="ft2-sample-launcher-") as td:
    binary = Path(td) / "test_sample_launcher_state"
    subprocess.run([
        "cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
        "-I", str(ROOT / "src"),
        str(ROOT / "tests" / "test_sample_launcher_state.c"),
        str(ROOT / "src" / "ft2_sample_launcher_state.c"),
        "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
