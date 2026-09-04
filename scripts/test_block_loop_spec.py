#!/usr/bin/env python3
"""Build and run the Block Loop bounds contract."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="tapehead-block-loop-") as tmp:
        executable = Path(tmp) / "test_block_loop_spec"
        subprocess.run(
            [
                "gcc", "-std=c11", "-D_DEFAULT_SOURCE", "-DNDEBUG",
                "-ffunction-sections", "-fdata-sections",
                f"-I{ROOT / 'vs2026_project/ft2-clone/sdl/include'}",
                f"-I{ROOT / 'src'}",
                str(ROOT / "tests/test_block_loop_spec.c"),
                str(ROOT / "src/ft2_replayer.c"),
                "-Wl,--gc-sections", "-lm", "-o", str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable)], check=True, cwd=ROOT)


if __name__ == "__main__":
    main()
