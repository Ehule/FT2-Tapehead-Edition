#!/usr/bin/env python3
"""Compile and run Windows-audio policy regression tests."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SDL_INCLUDE = ROOT / "vs2026_project/ft2-clone/sdl/include"


def build_open_policy_test(tmp: Path) -> None:
    common = [
        "gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
        "-DTAPEHEAD_AUDIO_HARDENING_TEST", "-ffunction-sections",
        "-fdata-sections", "-I", str(SDL_INCLUDE), "-I", str(ROOT / "src"),
    ]
    audio_object = tmp / "ft2_audio.o"
    test_object = tmp / "test_audio_hardening.o"
    subprocess.run(
        [*common, "-c", str(ROOT / "src/ft2_audio.c"), "-o", str(audio_object)],
        check=True, cwd=ROOT,
    )
    subprocess.run(
        [*common, "-c", str(ROOT / "tests/test_audio_hardening.c"),
         "-o", str(test_object)],
        check=True, cwd=ROOT,
    )
    executable = tmp / "test_audio_hardening"
    subprocess.run(
        ["gcc", "-Wl,--gc-sections", str(audio_object), str(test_object),
         "-lm", "-o", str(executable)],
        check=True, cwd=ROOT,
    )
    subprocess.run([str(executable)], check=True, cwd=ROOT)


def build_backend_config_test(tmp: Path) -> Path:
    executable = tmp / "test_audio_backend_config"
    subprocess.run(
        [
            "gcc", "-std=c11", "-DNDEBUG", "-D_DEFAULT_SOURCE",
            "-ffunction-sections", "-fdata-sections", "-I", str(SDL_INCLUDE),
            "-I", str(ROOT / "src"),
            str(ROOT / "tests/test_audio_backend_config.c"),
            str(ROOT / "src/ft2_config.c"),
            str(ROOT / "src/ft2_midi_map.c"),
            "-Wl,--gc-sections", "-o", str(executable),
        ],
        check=True, cwd=ROOT,
    )
    return executable


def check_backend_config(tmp: Path, executable: Path) -> None:
    tapehead_ini = tmp / "tapehead.ini"
    ft2_cfg = tmp / "FT2.CFG"
    cases = (
        ("[Audio]\nBackend=Auto\n", 0),
        ("[Audio]\nBackend=WASAPI\n", 1),
        ("[Audio]\nBackend=directsound\n", 2),
        ("[Audio]\nBackend=invalid\n", 0),
        ("[Audio]\nOutputBuses=4\n", 0),
    )
    for contents, expected in cases:
        tapehead_ini.write_text(contents, encoding="utf-8")
        subprocess.run(
            [str(executable), str(ft2_cfg), str(expected)],
            check=True, cwd=ROOT,
        )

    tapehead_ini.unlink()
    subprocess.run(
        [str(executable), str(ft2_cfg), "0"], check=True, cwd=ROOT,
    )
    generated = tapehead_ini.read_text(encoding="utf-8")
    assert "[Audio]" in generated
    assert "Backend=Auto" in generated


def check_startup_and_event_wiring() -> None:
    main_source = (ROOT / "src/ft2_main.c").read_text(encoding="utf-8")
    event_source = (ROOT / "src/ft2_events.c").read_text(encoding="utf-8")
    shipped_config = (ROOT / "release/other/tapehead.ini").read_text(
        encoding="utf-8"
    )

    startup_call = main_source.index(
        "configureWindowsAudioBackendBeforeSDL(startupAudioBackend)"
    )
    sdl_init = main_source.index("SDL_Init(sdlInitFlags)")
    assert startup_call < sdl_init
    assert "disableWasapi" not in main_source
    assert event_source.index("event.type == SDL_AUDIODEVICEADDED") < \
        event_source.index("if (editor.busy)", event_source.index("SDL_PollEvent"))
    assert "Backend=Auto" in shipped_config


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="tapehead-audio-hardening-") as name:
        tmp = Path(name)
        build_open_policy_test(tmp)
        config_test = build_backend_config_test(tmp)
        check_backend_config(tmp, config_test)
        check_startup_and_event_wiring()

    print("Tapehead audio hardening tests passed.")


if __name__ == "__main__":
    main()
