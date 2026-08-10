#!/usr/bin/env python3
"""Regression checks for every adaptive tuning-lane pattern geometry."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
draw = (ROOT / "src/ft2_pattern_draw.c").read_text()
editor = (ROOT / "src/ft2_pattern_ed.c").read_text()
tables = (ROOT / "src/ft2_tables.c").read_text()

assert "columnModeTab[12] = { 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 3 }" in draw
assert "columnModeTab[ui.numChannelsShown-1]" in draw
assert "config.ptnShowVolColumn != 0" in draw

body = draw[draw.index("pattLayouts[2][4]"):draw.index("static const uint8_t sharpNote1Char_small")]
rows = re.findall(r"\{\{([^}]+)\}, \{([^}]+)\}, FONT_TYPE([345]), (\d+), (\d+)\}", body)
assert len(rows) == 8, "volume-hidden and volume-visible tables need four modes each"
channel_widths = (144, 96, 72, 48, 144, 96, 72, 72)
for row_index, (x_text, width_text, _font, _char_width, _note_size) in enumerate(rows):
    positions = [int(value) for value in x_text.split(",")]
    widths = [int(value) for value in width_text.split(",")]
    assert len(positions) == len(widths) == 11
    volume_visible = row_index >= 4
    enabled = range(11) if volume_visible else (0, 1, 2, 5, 6, 7, 8, 9, 10)
    enabled = list(enabled)
    assert all(widths[field] > 0 for field in enabled)
    assert all(positions[a] < positions[b] for a, b in zip(enabled, enabled[1:]))
    assert max(positions[field] + widths[field] for field in enabled) <= channel_widths[row_index]

empty_tuning = draw[draw.index("if (n->tuneType == 0)"):draw.index("if (n->efx == 0", draw.index("if (n->tuneType == 0)"))]
assert "n->tuneData" not in empty_tuning.split("else", 1)[0]
assert "for (int32_t i = 5; i <= 7; i++)" in empty_tuning
assert "if (n->efx == 0 && n->efxData == 0)" in draw
assert "for (int32_t i = 8; i <= 10; i++)" in draw
assert "layout->x[cursor.object]" in draw and "layout->width[cursor.object]" in draw
assert "object == CURSOR_VOL1 || object == CURSOR_VOL2" in draw
assert "cursor.object = patternXToCursorObject(channelX)" in editor
assert "maxVisibleChans1[config.ptnMaxChannels] : maxVisibleChans2[config.ptnMaxChannels]" in editor
assert "maxVisibleChans1[4] = { 4, 6, 8, 8 }" in tables
assert "maxVisibleChans2[4] = { 4, 6, 8, 12 }" in tables
print("All adaptive pattern layouts and tuning-lane geometry verified.")
