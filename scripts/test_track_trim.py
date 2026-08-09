#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
config = (ROOT / "src/ft2_config.c").read_text()
actions = (ROOT / "src/ft2_tapehead_actions.c").read_text()
midi = (ROOT / "src/ft2_midi_map.c").read_text()
scopes = (ROOT / "src/scopes/ft2_scopes.c").read_text()

assert "tapeheadConfig.trackTrimMaxPercent = 200;" in config
assert '"TrackTrimMaxPercent"' in config
assert "percent < 100 ? 100 : percent > 200 ? 200 : percent" in config
assert "tapeheadConfig.trackTrimDisplayWidth = 2;" in config
assert '"TrackTrimDisplayWidth"' in config
assert "width < 0 ? 0 : width > 8 ? 8 : width" in config
assert "tapeheadTrackTrimClamp(trim, ceiling)" in actions
assert "tapeheadTrackTrimMapMidi(event.value" in midi
assert "tapeheadActionMasterVolumeSet((event.value * 256 + 63) / 127)" in midi
assert "if (volume > 256) volume = 256;" in actions
assert "if (!ui.scopesShown)\n\t\treturn;" in scopes
assert "drawTrackTrimIndicator" in scopes
assert "if (width == 0)" in scopes
assert "fillRect(x, top, width, height" in scopes
assert "hLine(x, unityY, width" in scopes

with tempfile.TemporaryDirectory(prefix="ft2-track-trim-") as tmp:
    exe = Path(tmp) / "test_track_trim"
    subprocess.run([
        "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-I", str(ROOT / "src"), str(ROOT / "tests/test_track_trim.c"),
        str(ROOT / "src/ft2_track_trim.c"), "-o", str(exe)
    ], check=True)
    subprocess.run([str(exe)], check=True)
