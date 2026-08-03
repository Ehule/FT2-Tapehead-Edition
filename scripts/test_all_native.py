#!/usr/bin/env python3
"""Run every standalone Tapehead native regression suite."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TEST_SCRIPTS = (
	"test_sample_launcher_native.py",
	"test_fasttracks_native.py",
    "test_fasttracks_transport.py",
    "test_multichannel_native.py",
    "test_audio_bus_delivery.py",
    "test_jack_native.py",
    "test_poly_matrix_native.py",
)


def main() -> None:
    for script_name in TEST_SCRIPTS:
        print(f"\n== {script_name} ==", flush=True)
        subprocess.run(
            [sys.executable, str(ROOT / "scripts" / script_name)],
            check=True,
            cwd=ROOT,
        )

    print("\nAll Tapehead native regression suites passed.")


if __name__ == "__main__":
    main()
