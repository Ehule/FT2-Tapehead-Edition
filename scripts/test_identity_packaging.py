#!/usr/bin/env python3
"""Regression checks for Tapehead identity and portable release wiring."""

from __future__ import annotations

import os
import stat
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNTIME_ASSETS = (
    "FT2.CFG",
    "tapehead.ini",
    "palette.pal",
    "fastTracksLogoBadges.bmp",
    "tapeheadSplash.png",
)


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def check_product_identity() -> None:
    cmake = read("CMakeLists.txt")
    assert "project(Tapehead)" in cmake
    assert "add_executable(Tapehead" in cmake
    assert "install(TARGETS Tapehead" in cmake

    for script_name in (
        "make-linux.sh",
        "make-linux-nomidi.sh",
        "make-linux-appimage.sh",
        "make-linux-appimage-nomidi.sh",
    ):
        script = read(script_name)
        assert "ft2-clone.AppDir" not in script
        assert "usr/bin/ft2-clone" not in script
        assert "release/other/ft2-clone" not in script

    project = read("vs2026_project/ft2-clone/ft2-clone.vcxproj")
    assert "<ProjectName>Tapehead</ProjectName>" in project
    assert project.count("<TargetName>Tapehead</TargetName>") == 2
    assert "ft2-clone-win32" not in project
    assert "ft2-clone-win64" not in project

    resource = read("src/ft2-clone.rc")
    assert 'VALUE "ProductName",      "Tapehead"' in resource
    assert 'VALUE "OriginalFilename", "Tapehead.exe"' in resource
    assert "tapehead\\\\tapehead.ico" in resource
    assert "ft2-clone.ico" not in resource

    assert '#define PROG_NAME_STR "Tapehead"' in read("src/ft2_replayer.h")
    assert "Tapehead MIDI Port" in read("src/ft2_midi.c")
    assert "Oh no! Tapehead has crashed" in read("src/ft2_events.c")
    assert "Tapehead v%s" in read("src/ft2_video.c")


def check_repository_cleanup() -> None:
    assert not (ROOT / "tapesister").exists()
    assert (ROOT / "src/ft2_tapesister_exchange.c").is_file()
    assert (ROOT / "src/ft2_tapesister_protocol.c").is_file()

    for obsolete in (
        "release/other/ft2-clone",
        "release/other/audiodev.ini",
        "release/other/mididev.ini",
        "release/other/Freedesktop.org Resources",
    ):
        assert not (ROOT / obsolete).exists()


def check_windows_portable_manifest() -> None:
    stage = read("scripts/stage-windows-portable.ps1")
    package = read("scripts/package-windows-portable.ps1")
    project = read("vs2026_project/ft2-clone/ft2-clone.vcxproj")

    for asset in RUNTIME_ASSETS:
        assert asset in stage
    for filename in (
        "SDL2.dll",
        "LICENSE.txt",
        "LICENSES.txt",
        "problems.txt",
        "README.txt",
        "Tapehead.exe",
    ):
        assert filename in stage

    assert "Tapehead-Windows-$ArchitectureLabel" in package
    assert '"win64"' in stage
    assert '"win64"' in package
    assert "Compress-Archive" in package
    assert project.count("stage-windows-portable.ps1") == 2
    assert project.count('-Destination "$(TargetDir)."') == 2

    for script_name in ("make-linux-appimage.sh", "make-linux-appimage-nomidi.sh"):
        script = read(script_name)
        assert "--custom-apprun Tapehead.AppDir/AppRun" in script
        assert 'cp "$DEFAULTS_DIR/tapeheadSplash.png" "$APPDIR/usr/bin/"' in script
        assert 'cp "$DEFAULTS_DIR/fastTracksLogoBadges.bmp" "$APPDIR/usr/bin/"' in script


def check_appimage_sidecar() -> None:
    app_run = ROOT / "release/linux/AppRun"
    config_source = read("src/ft2_config.c")
    assert 'getenv("TAPEHEAD_PORTABLE_DIR")' in config_source

    with tempfile.TemporaryDirectory(prefix="tapehead-appimage-") as tmp_name:
        tmp = Path(tmp_name)
        app_dir = tmp / "mount"
        defaults = app_dir / "usr/share/tapehead/defaults"
        binary = app_dir / "usr/bin/Tapehead"
        defaults.mkdir(parents=True)
        binary.parent.mkdir(parents=True)

        for asset in RUNTIME_ASSETS:
            (defaults / asset).write_text(f"default {asset}\n", encoding="utf-8")

        result_path = tmp / "portable-path.txt"
        binary.write_text(
            "#!/bin/sh\n"
            "set -eu\n"
            "test -f \"$TAPEHEAD_PORTABLE_DIR/FT2.CFG\"\n"
            "printf '%s' \"$TAPEHEAD_PORTABLE_DIR\" > \"$TAPEHEAD_TEST_RESULT\"\n",
            encoding="utf-8",
        )
        binary.chmod(binary.stat().st_mode | stat.S_IXUSR)

        fake_appimage = tmp / "Tapehead-x86_64.AppImage"
        fake_appimage.touch()
        environment = os.environ.copy()
        environment.update(
            {
                "APPDIR": str(app_dir),
                "APPIMAGE": str(fake_appimage),
                "TAPEHEAD_TEST_RESULT": str(result_path),
            }
        )
        subprocess.run(["sh", str(app_run)], check=True, env=environment)

        data_dir = tmp / "Tapehead-data"
        assert result_path.read_text(encoding="utf-8") == str(data_dir)
        for asset in RUNTIME_ASSETS:
            assert (data_dir / asset).is_file()

        customized = data_dir / "tapehead.ini"
        customized.write_text("user setting\n", encoding="utf-8")
        subprocess.run(["sh", str(app_run)], check=True, env=environment)
        assert customized.read_text(encoding="utf-8") == "user setting\n"


def main() -> None:
    check_product_identity()
    check_repository_cleanup()
    check_windows_portable_manifest()
    check_appimage_sidecar()
    print("Tapehead identity and portable packaging tests passed.")


if __name__ == "__main__":
    main()
