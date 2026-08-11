#!/usr/bin/env python3
"""Focused source-level contract checks for Pattern Editor block extraction."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
edit = (ROOT / "src/ft2_edit.c").read_text()
keyboard = (ROOT / "src/ft2_keyboard.c").read_text()
config = (ROOT / "src/ft2_config.c").read_text()
undo = (ROOT / "src/ft2_undo.c").read_text()
launcher = (ROOT / "src/ft2_pattern_launcher.c").read_text()
poly = (ROOT / "src/ft2_poly_matrix.c").read_text()

assert 'undoTransactionBegin("Extract block to pattern")' in edit
assert "patternNumRows[destination] = extractedRows" in edit
assert "(y - y1) * MAX_CHANNELS + x" in edit
assert "y2 = MIN(y2, sourceRows)" in edit
assert "pattern[sourcePattern][y * MAX_CHANNELS + x]" in edit
pattern_editor = (ROOT / "src/ft2_pattern_ed.c").read_text()
assert "findUnusedPattern" in edit
assert "song.orders[i] == pattNum" in pattern_editor
assert "copyBlock();" not in edit[edit.index("bool extractBlockToPattern"):edit.index("void pasteBlock")]
assert "tuneType != 0" in edit and "tuneData != 0" in edit

f8 = keyboard[keyboard.index("case SDLK_F8:"):keyboard.index("case SDLK_F9:")]
assert "trackTranspCurInsUp()" in f8
assert "pattTranspCurInsUp()" in f8
assert "blockTranspCurInsUp()" in f8
assert "tapeheadConfig.f8ExtractBlock" in f8
assert "!keyWasRepeated" in f8
assert "editor.curOctave = 6" in f8
assert "handleKeys(SDL_Keycode keycode, SDL_Scancode scanKey, bool keyWasRepeated)" in keyboard
assert "handleKeys(keycode, scancode, keyWasRepeated)" in keyboard

assert "tapeheadConfig.f8ExtractBlock = true" in config
assert '"F8ExtractBlock"' in config
assert "parseBoolValue(value, &tapeheadConfig.f8ExtractBlock)" in config
assert "restorePattern(state)" in undo
for source in (launcher, poly):
    assert "track->tuneType != 0" in source
    assert "track->tuneData != 0" in source

print("Pattern block extraction, F8/config, Undo notification, and Tuning/Drift contracts passed.")
