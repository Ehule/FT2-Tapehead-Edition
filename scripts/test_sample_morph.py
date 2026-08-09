#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="ft2-sample-morph-") as temp_dir:
    output = Path(temp_dir) / "test_sample_morph"
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(ROOT / "src"),
        str(ROOT / "tests/test_sample_morph.c"),
        str(ROOT / "src/ft2_sample_morph.c"),
        "-o", str(output),
    ], check=True)
    subprocess.run([str(output)], check=True)
