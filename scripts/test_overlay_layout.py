#!/usr/bin/env python3
"""Structural audit for modal, popup, and persistent overlay drawing."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def constant(source: str, name: str) -> int:
	match = re.search(rf"#define\s+{name}\s+(\d+)", source)
	assert match is not None, f"missing {name}"
	return int(match.group(1))


def local_box(source: str, function: str) -> tuple[int, int, int, int]:
	start = source.index(f"static void {function}(void)")
	body = source[start:source.index("\n}", start)]
	values = []
	for name in ("x", "y", "w", "h"):
		match = re.search(rf"const int16_t {name} = (\d+);", body)
		assert match is not None, f"missing {name} in {function}"
		values.append(int(match.group(1)))
	return tuple(values)


def main() -> None:
	edit = (ROOT / "src/ft2_edit.c").read_text()
	video = (ROOT / "src/ft2_video.c").read_text()
	sysreq = (ROOT / "src/ft2_sysreqs.c").read_text()
	sample_tools = (ROOT / "src/ft2_sample_ed_features.c").read_text()
	pattern = (ROOT / "src/ft2_pattern_ed.c").read_text()
	pattern_draw = (ROOT / "src/ft2_pattern_draw.c").read_text()
	main_source = (ROOT / "src/ft2_main.c").read_text()

	# The persistent Instrument Transform overlay must not cross the piano,
	# whose instrument-editor framework starts at logical Y=347. The piano is
	# updated independently during playback and can otherwise punch through it.
	panel_x = constant(edit, "INST_TRANSFORM_PANEL_X")
	panel_y = constant(edit, "INST_TRANSFORM_PANEL_Y")
	panel_w = constant(edit, "INST_TRANSFORM_PANEL_W")
	panel_h = constant(edit, "INST_TRANSFORM_PANEL_H")
	assert panel_x >= 0 and panel_x + panel_w <= 632
	assert panel_y >= 173 and panel_y + panel_h <= 347

	# It is also drawn after all live editor/replayer updates, and closing it
	# rebuilds the covered bottom editor after first disabling the overlay.
	redraw = video[video.index("void handleRedrawing(void)"):
		video.index("static void drawReplayerData(void)",
			video.index("void handleRedrawing(void)"))]
	assert redraw.index("drawReplayerData();") < redraw.index(
		"instrumentTransformDrawPanel();")
	assert redraw.index("handleInstEditorRedrawing();") < redraw.index(
		"instrumentTransformDrawPanel();")
	close_start = edit.index("static void closeInstrumentTransform(bool apply)")
	close_body = edit[close_start:edit.index("\n}\n\nvoid openInstrumentTransformEditor", close_start)]
	assert close_body.index("instrumentTransformActive = false;") < close_body.index(
		"showBottomScreen();")

	# Conventional system requests remain above the instrument piano. Tall
	# multiline requests own both top- and bottom-screen restoration, while the
	# input box rebuilds the bottom editor it covers.
	request_y = constant(sysreq, "SYSTEM_REQUEST_Y")
	request_h = constant(sysreq, "SYSTEM_REQUEST_H")
	assert request_y + request_h <= 347
	assert "showTopScreen(RESTORE_SCREENS);" in sysreq
	assert sysreq.count("showBottomScreen();") >= 2

	# The four legacy sample-tool modal frames stay within the waveform region
	# (above the controls at Y=329), draw after handleRedrawing(), and restore
	# the sample view through their shared close path.
	for function in ("drawResampleBox", "drawEchoBox", "drawMixSampleBox",
		"drawSampleVolumeBox"):
		x, y, w, h = local_box(sample_tools, function)
		assert x >= 0 and x + w <= 632
		assert y >= 173 and y + h <= 329
	assert sample_tools.count("handleRedrawing();") == 4
	assert "writeSample(FORCE_SAMPLE_REDRAW);" in sample_tools
	assert "updateNewSample();" in sample_tools

	# Pattern navigation is a popup inside pattern data. Dismissal requests a
	# pattern redraw, and drawPatternNavPopup() is the last overlay in the
	# pattern writer so live rows cannot paint over it.
	assert pattern.count("pattNavPopupShown = false;") == 2
	assert pattern.count("ui.updatePatternEditor = true;") >= 2
	write_pattern = pattern_draw[pattern_draw.index("void writePattern("):
		pattern_draw.index("\n}", pattern_draw.index("void writePattern("))]
	assert "drawPatternNavPopup();" in write_pattern

	# The standalone Deck/Sample Matrix explicitly redraws after normal live
	# fields, preserving the same topmost-surface rule at full-screen scale.
	main_loop = main_source[main_source.index("while (editor.programRunning)"):
		main_source.index("if (config.cfg_AutoSave)")]
	assert main_loop.index("handleRedrawing();") < main_loop.index(
		"patternLauncherDrawStandalone();")

	print("Overlay layout and restoration audit passed.")


if __name__ == "__main__":
	main()
