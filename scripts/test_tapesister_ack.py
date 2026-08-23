#!/usr/bin/env python3
"""Compile and run TapeSister acknowledgement atomicity checks."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-tapesister-ack-") as tmp:
        executable = Path(tmp) / "test_tapesister_ack"
        subprocess.run(
            [
                "cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "src"),
                str(ROOT / "tests/test_tapesister_ack.c"),
                str(ROOT / "src/ft2_tapesister_ack.c"),
                "-o", str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
