#!/usr/bin/env python3
"""Compile and run the Tapehead splash configuration regression test."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-splash-config-") as tmp:
        tmp_path = Path(tmp)
        executable = tmp_path / "test_splash_config"
        tapehead_ini = tmp_path / "tapehead.ini"

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
                str(ROOT / "tests/test_splash_config.c"),
                str(ROOT / "src/ft2_config.c"),
                str(ROOT / "src/ft2_midi_map.c"),
                "-Wl,--gc-sections",
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )

        cases = (
            ("[Video]\nshowSplashScreen=true\n", "true", "true", "256", "1", "8", "true"),
            ("[Video]\nshowSplashScreen=false\n", "false", "true", "256", "1", "8", "true"),
            ("[Video]\nshowSplashScreen=off\n", "false", "true", "256", "1", "8", "true"),
            ("[Video]\nshowSplashScreen=yes\n", "true", "true", "256", "1", "8", "true"),
            ("[Video]\nHDMode=false\n", "true", "true", "256", "1", "8", "true"),
            (
                "[Video]\nshowSplashScreen=true\n"
                "[Pattern]\nFastTracksUseTrackLengths=false\n"
                "TrackLengthControlMax=64\n",
                "true",
                "false",
                "64",
                "1",
                "8",
                "true",
            ),
            ("[Pattern]\nTrackLengthControlMax=999\n", "true", "true", "256", "1", "8", "true"),
            ("[Pattern]\nTrackLengthControlMax=0\n", "true", "true", "256", "1", "8", "true"),
            (
                "[MIDI]\nControlTrackLeftStart=3\n"
                "ControlTrackRightStart=6\n"
                "ControlTrackNavigationWrap=false\n",
                "true",
                "true",
                "256",
                "3",
                "6",
                "false",
            ),
            (
                "[MIDI]\nControlTrackLeftStart=0\n"
                "ControlTrackRightStart=9\n"
                "ControlTrackNavigationWrap=maybe\n",
                "true",
                "true",
                "256",
                "1",
                "8",
                "true",
            ),
        )
        for (
            contents,
            expected,
            expected_fasttracks_len,
            expected_max,
            expected_left,
            expected_right,
            expected_wrap,
        ) in cases:
            tapehead_ini.write_text(contents, encoding="utf-8")
            subprocess.run(
                [
                    str(executable),
                    str(tmp_path / "FT2.CFG"),
                    expected,
                    expected_fasttracks_len,
                    expected_max,
                    expected_left,
                    expected_right,
                    expected_wrap,
                ],
                check=True,
                cwd=ROOT,
            )
            written = tapehead_ini.read_text(encoding="utf-8")
            assert (
                f"FastTracksUseTrackLengths={expected_fasttracks_len}" in written
            )
            assert f"TrackLengthControlMax={expected_max}" in written

    print("Splash configuration tests passed.")


if __name__ == "__main__":
    main()
