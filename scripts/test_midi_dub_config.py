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
            """[MIDI]
PerformanceControl=true
Profile=APC40MK2
ControlInput=APC40 mkII
ControlOutput=APC40 mkII MIDI Out
PatternJogFastTracks=Include
PatternJogAudition=ManualPingPong
TransportFreezeAudio=Cut
TransportFreezePedalMode=Hold
TransportFreezeNavigation=Audition
TransportFreezeResume=Retrigger
APC40RGBBrightness=57

[MIDI_MAP]
NoteOn.1.48=TrackPerformanceMuteToggle:1
CC.1.7=TrackTrim:1
NoteOn.17.48=PerformanceUnmuteAll

[MIDIDub]
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
				str(ROOT / "src/ft2_midi_map.c"),
                "-Wl,--gc-sections",
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run(
            [str(executable), str(tmp_path / "FT2.CFG"), "57"],
            check=True,
            cwd=ROOT,
        )

        # Missing and malformed values retain the safe default; valid signed
        # integers are clamped to the documented range.
        baseline = tapehead_ini.read_text(encoding="utf-8")
        for value, expected in ((None, 100), ("nope", 100), ("-12", 0), ("345", 100)):
            replacement = "" if value is None else f"APC40RGBBrightness={value}\n"
            tapehead_ini.write_text(
                baseline.replace("APC40RGBBrightness=57\n", replacement),
                encoding="utf-8",
            )
            subprocess.run(
                [str(executable), str(tmp_path / "FT2.CFG"), str(expected)],
                check=True,
                cwd=ROOT,
            )


if __name__ == "__main__":
    main()
