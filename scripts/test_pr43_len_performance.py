#!/usr/bin/env python3
"""Integration-seam checks for PR43 LEN performance controls and visuals."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
fasttracks_h = (ROOT / "src/ft2_fasttracks.h").read_text()
replayer = (ROOT / "src/ft2_replayer.c").read_text()
keyboard = (ROOT / "src/ft2_keyboard.c").read_text()
events = (ROOT / "src/ft2_events.c").read_text()
actions = (ROOT / "src/ft2_tapehead_actions.c").read_text()
apc = (ROOT / "src/ft2_apc40_mk2.c").read_text()
draw = (ROOT / "src/ft2_pattern_draw.c").read_text()
palette = (ROOT / "src/ft2_palette.c").read_text()
config = (ROOT / "src/ft2_config.c").read_text()

# Offline Baker save/restore includes the runtime clutch with FastTracks phase.
assert "lengthTopologyBypassed" in fasttracks_h
assert "fastTracksPOCGetRuntimeState(&savedFastTracksRuntime)" in (
    ROOT / "src/ft2_baker.c"
).read_text()

# The one native-tested boundary policy owns ordinary wrap/transition decisions.
boundary_call = replayer.index("fastTracksResolveSharedBoundary(")
assert "FAST_TRACKS_SHARED_BOUNDARY_WRAP_PHYSICAL" in replayer[boundary_call:]
assert "fastTracksSharedCycleUsesBlankRow(" in replayer
assert "rowNotes = &pattern[song.pattNum][song.row * MAX_CHANNELS]" in replayer
assert "sharedBlankRow" in replayer
assert "fastTracksPOCLengthTopologyIsActive" in replayer

# Freeze must not map a logical extension/private CONTROL head back through a
# clamped physical editor row.
punch = replayer.index("void tapeheadReplayerBeginTransportPunch(void)")
resume = replayer.index("void tapeheadReplayerResumeTransportPunch", punch)
assert "if (!fastTracksPOCLengthTopologyIsActive(song.pattNum))" in replayer[
    punch:resume
]

# Freeze stops the sequencers, while Live Bake retains elapsed performance time
# so a held freeze does not collapse out of the realized XM timeline.
tick = replayer.index("void tickReplayer(void)")
freeze = replayer.index("tapeheadActionTransportPunchIsFrozen()", tick)
baker = replayer.index("bakerBeginTick();", tick)
assert baker < freeze

# Preserve Ctrl+Shift+Space, then claim the two new chords without repeat.
popup = keyboard.index("openPatternNavPopup();")
length_toggle = keyboard.index("tapeheadActionTrackLengthBypassToggle();")
freeze_down = keyboard.index("tapeheadActionTransportPunchKeyboard(true);")
assert popup < length_toggle < freeze_down
for call in (length_toggle, freeze_down):
    assert "!keyWasRepeated" in keyboard[call - 120:call]
assert keyboard.index("tapeheadActionTransportPunchKeyboard(false);") < keyboard.index(
    "if (editor.editTextFlag || ui.sysReqShown)"
)
assert "SDL_WINDOWEVENT_FOCUS_LOST" in events
assert "tapeheadActionTransportPunchKeyboard(false)" in events

# APC Device Lock keeps one action path: Shift branches in the shared action,
# and the physical LED preserves normal FastTracks feedback while reporting the
# runtime LEN bypass with its brighter state.
assert '"DeviceLock", "NoteOn.1.63", "FastTrackResetAll"' in apc
reset = actions.index("bool tapeheadActionFastTrackResetAll(void)")
assert "shiftModifierHeld" in actions[reset:reset + 500]
assert "tapeheadActionTrackLengthBypassToggle" in actions[reset:reset + 500]
assert "fastTracksPOCLengthTopologyIsBypassed()" in apc
assert "? 2 : fastTracksPOCAnyEnabled()" in apc

# CONTROL remains highest priority; hybrid color is palette/config managed and
# dormant classification is dimmed while LEN authority is bypassed.
hybrid = draw.index("PAL_FASTTRACKS_LENGTH_PLAYHEAD")
control = draw.index("PAL_CONTROL_PLAYHEAD", hybrid)
dim = draw.index("dimPatternColor(playheadColor)", control)
assert hybrid < control < dim
assert '"FastTracksLengthPlayhead"' in palette
assert '"FastTracksLengthPlayheadColor"' in config
assert "FT+LEN Head" in palette

print("PR43 LEN performance integration seams passed.")
