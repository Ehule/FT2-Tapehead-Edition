#!/usr/bin/env python3
"""Structural checks for Composition Baker system-request geometry."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
	source = (ROOT / "src" / "ft2_sysreqs.c").read_text()
	assert "drawWindowAt(wlen, dialogHeight, y);" in source
	assert "drawWindow(wlen, dialogHeight);" not in source
	assert 'textWidth("Pattern Rows:") + 8' in source
	assert "p->y = y + 36;" in source and "p->h = 16;" in source
	assert "c->y = y + (bakerOptions ? 84" in source
	assert "bakerOptions ? 86" in source
	assert "y + 59" in source and "y + 72" in source
	assert "drawPushButton(patternRowsButtonID);" in source
	assert "releasedButton == patternRowsButtonID" in source
	assert "returnVal = releasedButton + 1;" in source
	assert "mouse.x >= x + 94" not in source

	# Native 640x400 placement: title, text, selector, estimates, checkbox and
	# terminating buttons all remain strictly inside the 119-pixel frame.
	dialog_y, dialog_height = 249 - 52, 119
	controls = ((4, 7), (24, 7), (36, 16), (59, 7), (72, 7), (84, 12), (101, 16))
	assert all(dialog_y < dialog_y + offset and
		dialog_y + offset + height < dialog_y + dialog_height
		for offset, height in controls)

	print("Composition Baker dialog layout tests passed.")


if __name__ == "__main__":
	main()
