#!/usr/bin/env python3
"""Build and run TapeSister audio-render planning/metadata tests."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="tapehead-tapesister-render-") as tmp:
        executable = Path(tmp) / "test_tapesister_render"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{ROOT / 'src'}",
                str(ROOT / "tests/test_tapesister_render.c"),
                str(ROOT / "src/ft2_tapesister_render.c"),
                "-o",
                str(executable),
            ],
            check=True,
        )
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
