#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SDL_INCLUDE = ROOT / "vs2026_project" / "ft2-clone" / "sdl" / "include"


def main() -> None:
    events = (ROOT / "src" / "ft2_events.c").read_text()
    main_source = (ROOT / "src" / "ft2_main.c").read_text()
    chord = "event.key.keysym.sym == SDLK_TAB"
    request = "TAPE_COMPANION_TAPESISTER_NAME"
    assert chord in events and request in events
    assert events.index(chord) < events.index("if (editor.busy)")
    for call in ("initCompanionFocus();", "pumpCompanionFocus();",
                 "closeCompanionFocus();"):
        assert call in main_source
    for project_name in ("ft2-clone.vcxproj", "ft2-clone.vcxproj.filters"):
        project = ROOT / "vs2026_project" / "ft2-clone" / project_name
        project_text = project.read_text(encoding="utf-8-sig")
        assert "tape_companion.c" in project_text
        assert "tape_companion.h" in project_text
        ET.parse(project)

    with tempfile.TemporaryDirectory() as temporary:
        binary = Path(temporary) / "test_companion_focus"
        command = [
            "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic",
            "-DTAPE_COMPANION_TEST",
            f"-I{ROOT / 'src'}", f"-I{SDL_INCLUDE}",
            str(ROOT / "tests" / "test_companion_focus.c"),
            str(ROOT / "src" / "tape_companion.c"),
            "-o", str(binary),
        ]
        command.extend(["-luser32"] if os.name == "nt" else ["-lrt"])
        subprocess.run(command, check=True, cwd=ROOT)
        subprocess.run([str(binary)], check=True, cwd=ROOT)

    print("Companion focus integration guards passed")


if __name__ == "__main__":
    main()
