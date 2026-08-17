#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

source = (ROOT / "src/ft2_sysreqs.c").read_text()
restore = source[source.index("/* Multiline dialogs can grow upward"):
                 source.index("SDL_EventState(SDL_DROPFILE, SDL_ENABLE);")]
assert "!ui.extendedPatternEditor && systemRequestOverlapsTopScreen(y)" in restore
assert "showTopScreen(RESTORE_SCREENS);" in restore
assert restore.index("showTopScreen(RESTORE_SCREENS);") < restore.index("showBottomScreen();")

with tempfile.TemporaryDirectory(prefix="ft2-sysreq-") as temp_dir:
    output = Path(temp_dir) / "test_sysreq_layout"
    subprocess.run([
        "cc", "-std=c11", "-O2", "-ffunction-sections", "-fdata-sections",
        "-I", str(ROOT / "src"),
        "-I", str(ROOT / "vs2026_project/ft2-clone/sdl/include"),
        str(ROOT / "tests/test_sysreq_layout.c"),
        str(ROOT / "src/ft2_sysreqs.c"),
        "-Wl,--gc-sections", "-o", str(output),
    ], check=True)
    subprocess.run([str(output)], check=True)
