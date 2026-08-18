#!/usr/bin/env python3
"""Compile and run native per-track LEN/CONTROL metadata tests."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


with tempfile.TemporaryDirectory(prefix="ft2-track-length-") as td:
    binary = Path(td) / "test_track_length_control"
    subprocess.run(
        [
            "cc",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-ffunction-sections",
            "-fdata-sections",
            f"-I{ROOT / 'vs2026_project/ft2-clone/sdl/include'}",
            f"-I{ROOT / 'src'}",
            str(ROOT / "tests/test_track_length_control.c"),
            str(ROOT / "src/ft2_fasttracks.c"),
            str(ROOT / "src/ft2_fasttracks_core.c"),
            "-Wl,--gc-sections",
            "-o",
            str(binary),
        ],
        check=True,
        cwd=ROOT,
    )
    subprocess.run([str(binary)], check=True, cwd=ROOT)


# Guard the integration seams that cannot be isolated from the full replayer
# in a small native executable.
replayer = (ROOT / "src/ft2_replayer.c").read_text()
saver = (ROOT / "src/ft2_module_saver.c").read_text()
loader = (ROOT / "src/modloaders/ft2_load_xm.c").read_text()
mouse = (ROOT / "src/ft2_mouse.c").read_text()
trim = (ROOT / "src/ft2_trim.c").read_text()
undo = (ROOT / "src/ft2_undo.c").read_text()
config = (ROOT / "src/ft2_config.c").read_text()
fasttracks = (ROOT / "src/ft2_fasttracks.c").read_text()
pattern_draw = (ROOT / "src/ft2_pattern_draw.c").read_text()

assert "fastTracksPOCWriteXMExtension(f)" in saver
assert "!standardXMSave" in saver
assert "fastTracksPOCReadXMExtension(f, filesize)" in loader
assert "fastTracksPatternMetadata_t fastTracksMetadata" in undo
assert "fastTracksPOCSetPatternMetadata" in undo
assert "fastTracksPOCResetAllPatternMetadata();" in trim
assert "oldPatternMetadata" in trim
assert '"Set track length"' in mouse
assert '"Set control track"' in mouse
assert "pattCoord->upperRowsY + 10" in mouse
assert "positionJump || patternBreak || speedOrTempo || extendedTransport" in replayer
assert "processMasterTransportEffect(ch, masterNote)" in replayer
assert "fastTracksPOCUsesTrackLengths() &&" in replayer
assert "getControlVisualRow" in replayer
assert "song.curReplayerRow = (uint8_t)getControlVisualRow" in replayer
assert '"FT uses LEN"' in config
assert '"FastTracksUseTrackLengths"' in config
assert "fastTracksPOCGetFastTrackLength" in fasttracks
assert "lengthHeaderY + 8" in pattern_draw
assert "displayedRow >= fastTracksPOCGetEffectiveTrackLength" in pattern_draw
assert "drawTrackPlayheadOutline" in pattern_draw
assert "drawDirectHLine" in pattern_draw
assert "drawPlayhead = displayedRow == fastTrack->sourceRow" in pattern_draw
assert "fastTrack->sourceRow + (i - pattCoord->numUpperRows)" not in pattern_draw
assert "panelWidth - 15" in pattern_draw
assert "panelWidth - 11" in mouse
