#!/usr/bin/env python3
"""Compile and run the Tapehead MIDI Dub configuration regression test."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-midi-dub-") as tmp:
        tmp_path = Path(tmp)
        executable = tmp_path / "test_midi_dub_config"
        tapehead_ini = tmp_path / "tapehead.ini"
        tapehead_ini.write_text(
            """[MIDIDub]
Track01=16
Track2=9
Track03=0
Track04=17
Track05=invalid
Track17=4
Track32=1
Track00=7
Track33=7
""",
            encoding="utf-8",
        )

        subprocess.run(
            [
                "gcc",
                "-std=c11",
                "-DNDEBUG",
                "-D_DEFAULT_SOURCE",
                "-ffunction-sections",
                "-fdata-sections",
                f"-I{ROOT / 'vs2026_project/ft2-clone/sdl/include'}",
                f"-I{ROOT / 'src'}",
                str(ROOT / "tests/test_midi_dub_config.c"),
                str(ROOT / "src/ft2_config.c"),
                "-Wl,--gc-sections",
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run(
            [str(executable), str(tmp_path / "FT2.CFG")],
            check=True,
            cwd=ROOT,
        )


if __name__ == "__main__":
    main()
