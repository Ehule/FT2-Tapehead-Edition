#!/usr/bin/env python3
"""Compile and run the live mixer-to-logical-bus delivery regression test."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SDL_INCLUDE = ROOT / "vs2026_project/ft2-clone/sdl/include"


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-audio-bus-delivery-") as tmp_name:
        tmp = Path(tmp_name)
        common = [
            "gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
            "-ffunction-sections", "-fdata-sections", "-I", str(SDL_INCLUDE),
        ]
        sources = [
            (ROOT / "src/ft2_audio.c", "ft2_audio.o", ["-DTAPEHEAD_AUDIO_ROUTING_TEST"]),
            (ROOT / "src/ft2_multichannel.c", "ft2_multichannel.o", []),
            (ROOT / "src/mixer/ft2_mix.c", "ft2_mix.o", []),
            (ROOT / "src/mixer/ft2_silence_mix.c", "ft2_silence_mix.o", []),
            (ROOT / "tests/test_audio_bus_delivery.c", "test.o", ["-DTAPEHEAD_AUDIO_ROUTING_TEST"]),
        ]

        objects: list[str] = []
        for source, object_name, extra_flags in sources:
            output = tmp / object_name
            subprocess.run(
                [*common, *extra_flags, "-c", str(source), "-o", str(output)],
                check=True,
                cwd=ROOT,
            )
            objects.append(str(output))

        executable = tmp / "test_audio_bus_delivery"
        subprocess.run(
            ["gcc", "-Wl,--gc-sections", *objects, "-lm", "-o", str(executable)],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
