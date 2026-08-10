#!/usr/bin/env python3
"""Focused source regressions for configurable pattern-field colors."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
palette = (ROOT / "src/ft2_palette.c").read_text()
draw = (ROOT / "src/ft2_pattern_draw.c").read_text()
config = (ROOT / "src/ft2_config.c").read_text()
header = (ROOT / "src/ft2_palette.h").read_text()

for name in ("NOTE", "INSTRUMENT", "VOLUME", "TUNING", "EFFECT", "EMPTY"):
    assert f"PAL_PATTERN_{name}" in header

helper = palette[palette.index("bool patternFieldColorsActive"):
                 palette.index("bool paletteListMouseWheel")]
assert "PATTERN_COLOR_ALWAYS" in helper
assert "PATTERN_COLOR_EDIT" in helper
assert "PLAYMODE_EDIT" in helper
assert "PLAYMODE_RECPATT" in helper and "PLAYMODE_RECSONG" in helper
assert "PLAYMODE_SONG" not in helper and "PLAYMODE_PATT" not in helper
assert "PAL_PATTEXT" in helper and "PAL_PATTERN_EMPTY" in helper

cell = draw[draw.index("uint32_t noteColor = patternFieldColor"):
            draw.index("drawAdaptiveCell", draw.index("uint32_t noteColor = patternFieldColor"))]
for field in range(5):
    assert f"patternFieldColor({field}," in cell
assert "drawPtr->tuneType != 0" in cell
assert "drawPtr->efx != 0 || drawPtr->efxData != 0" in cell

assert '"PatternColorMode"' in config
for value in ("edit", "always", "mono"):
    assert f'"{value}"' in config
assert "patternColorMode = PATTERN_COLOR_MONO" in config

assert "paletteListOffset" in palette
assert "paletteListMouseWheel" in palette
assert "cfg_ColorNum < paletteListOffset" in palette
for preset in ("Arctic", "LiTHe dark", "Aurora Borealis", "Rose", "Blues",
               "Dark mode", "Gold", "Violent", "Heavy Metal", "Why colors?",
               "Jungle", "User defined"):
    assert f'"{preset}"' in palette

assert "i >= 6 && !colorFound[i]" in palette
assert "colors[i] = colors[0]" in palette
assert "for (int32_t i = 0; i < 12; i++)" in palette

print("All pattern color modes, fields, palette compatibility, and UI list checks passed.")
