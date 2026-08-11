#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="ft2-sysreq-") as temp_dir:
    output = Path(temp_dir) / "test_sysreq_layout"
    subprocess.run([
        "cc", "-std=c11", "-O2", "-ffunction-sections", "-fdata-sections",
        "-I", str(ROOT / "src"),
        "-I", str(ROOT / "vs2026_project/ft2-clone/sdl/include"),
        str(ROOT / "tests/test_sysreq_layout.c"),
        str(ROOT / "src/ft2_sysreqs.c"),
        "-Wl,--gc-sections", "-o", str(output),
    ], check=True)
    subprocess.run([str(output)], check=True)
