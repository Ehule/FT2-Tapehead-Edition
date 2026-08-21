#!/usr/bin/env python3
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory() as temp_dir:
    executable = pathlib.Path(temp_dir) / "test_diskop_browser"
    subprocess.run(
        [
            "cc",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            str(ROOT / "src"),
            str(ROOT / "src/ft2_diskop_browser.c"),
            str(ROOT / "tests/test_diskop_browser.c"),
            "-o",
            str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)
