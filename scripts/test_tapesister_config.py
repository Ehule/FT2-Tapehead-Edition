#!/usr/bin/env python3
"""Compile and run TapeSister path configuration regression tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-tapesister-config-") as tmp:
        tmp_path = Path(tmp)
        executable = tmp_path / "test_tapesister_config"
        tapehead_ini = tmp_path / "tapehead.ini"
        subprocess.run(
            [
                "gcc", "-std=c11", "-DNDEBUG", "-D_DEFAULT_SOURCE",
                "-ffunction-sections", "-fdata-sections",
                f"-I{ROOT / 'vs2026_project/ft2-clone/sdl/include'}",
                f"-I{ROOT / 'src'}",
                str(ROOT / "tests/test_tapesister_config.c"),
                str(ROOT / "src/ft2_config.c"),
                str(ROOT / "src/ft2_midi_map.c"),
                "-Wl,--gc-sections", "-o", str(executable),
            ],
            check=True,
            cwd=ROOT,
        )

        long_exchange = "/tmp/" + "/".join(["exchange-segment"] * 100)
        long_executable = "/opt/" + "/".join(["application-segment"] * 90) + "/TapeSister"
        capture_folder = str(tmp_path / "Captures")
        tapehead_ini.write_text(
            "[TapeSister]\n"
            f"ExchangePath={long_exchange}\n"
            f"ExecutablePath={long_executable}\n"
            "CustomKey=keep-me\n"
            "[Capture]\n"
            f"Folder={capture_folder}\n"
            "[Other]\nCustomSetting=still-here\n",
            encoding="utf-8",
        )
        subprocess.run(
            [str(executable), str(tmp_path / "FT2.CFG"), long_exchange,
             long_executable, capture_folder],
            check=True,
            cwd=ROOT,
        )
        written = tapehead_ini.read_text(encoding="utf-8")
        assert written.count("[TapeSister]") == 1
        assert f"ExchangePath={long_exchange}" in written
        assert f"ExecutablePath={long_executable}" in written
        assert "CustomKey=keep-me" in written
        assert "CustomSetting=still-here" in written
        assert f"Folder={capture_folder}" in written

        tapehead_ini.write_text(
            "[TapeSister]\nExchangePath=/tmp/shared\nExecutablePath=\n"
            "[Capture]\nFolder=\n",
            encoding="utf-8",
        )
        subprocess.run(
            [str(executable), str(tmp_path / "FT2.CFG"), "/tmp/shared", "", ""],
            check=True,
            cwd=ROOT,
        )

    print("TapeSister configuration tests passed.")


if __name__ == "__main__":
    main()
