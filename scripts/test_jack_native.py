#!/usr/bin/env python3
"""Build a fake JACK server and exercise Tapehead's native JACK backend."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SDL_INCLUDE = ROOT / "vs2026_project/ft2-clone/sdl/include"


def sdl_build_flags() -> tuple[list[str], list[str]]:
    if shutil.which("pkg-config") is not None:
        pkg_config = subprocess.run(
            ["pkg-config", "--cflags", "--libs", "sdl2"],
            check=False,
            capture_output=True,
            text=True,
        )
        if pkg_config.returncode == 0:
            flags = pkg_config.stdout.split()
            compile_flags = [flag for flag in flags if flag.startswith("-I")]
            link_flags = [flag for flag in flags if not flag.startswith("-I")]
            return compile_flags, link_flags

    candidates = [
        *Path("/lib").glob("*/libSDL2-2.0.so.0"),
        *Path("/usr/lib").glob("*/libSDL2-2.0.so.0"),
    ]
    if not candidates:
        raise RuntimeError("SDL2 development files or runtime library not found")

    return ["-I", str(SDL_INCLUDE)], [str(candidates[0])]


def main() -> None:
    sdl_compile_flags, sdl_link_flags = sdl_build_flags()

    with tempfile.TemporaryDirectory(prefix="ft2-jack-native-") as tmp_name:
        tmp = Path(tmp_name)
        fake_jack = tmp / "libjack.so.0"
        executable = tmp / "test_jack_backend"

        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-shared",
                "-fPIC",
                str(ROOT / "tests/fake_jack.c"),
                "-o",
                str(fake_jack),
            ],
            check=True,
            cwd=ROOT,
        )

        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                *sdl_compile_flags,
                str(ROOT / "tests/test_jack_backend.c"),
                str(ROOT / "src/ft2_jack.c"),
                *sdl_link_flags,
                "-ldl",
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )

        environment = os.environ.copy()
        environment["LD_LIBRARY_PATH"] = str(tmp)
        subprocess.run([str(executable)], check=True, cwd=ROOT, env=environment)


if __name__ == "__main__":
    main()
