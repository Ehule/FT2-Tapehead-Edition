#!/usr/bin/env python3
"""Compile and run Tapehead application-wide undo transaction tests."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="ft2-undo-") as td:
    binary = Path(td) / "test_undo_transactions"
    subprocess.run([
        "gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        f"-I{ROOT / 'vs2026_project/ft2-clone/sdl/include'}",
        f"-I{ROOT / 'src'}",
        str(ROOT / "tests/test_undo_transactions.c"),
        str(ROOT / "src/ft2_undo.c"),
        "-o", str(binary),
    ], check=True, cwd=ROOT)
    subprocess.run([str(binary)], check=True, cwd=ROOT)
