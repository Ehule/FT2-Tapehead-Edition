#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
draw = (ROOT / "src/ft2_pattern_draw.c").read_text()
actions = (ROOT / "src/ft2_tapehead_actions.c").read_text()
policy = (ROOT / "src/ft2_transport_visuals.h").read_text()

def independent(feature: bool, running: bool, fast: bool, length: bool, frozen: bool) -> bool:
    return feature and running and (fast or length or frozen)

# Core per-track policy combinations.
assert not independent(True, True, False, False, False)  # normal FT2
assert independent(True, True, True, False, False)       # FastTracks
assert independent(True, True, False, True, False)       # LEN
assert independent(True, True, True, True, False)        # FastTracks + LEN
assert independent(True, True, False, False, True)       # Freeze
assert not independent(False, True, True, True, True)    # compatibility switch
assert not independent(True, False, True, True, True)    # stopped editor

# The renderer must make the choice inside the channel loop, not globally.
loop = draw.index("for (int32_t j = 0; j < numChannels")
choice = draw.index("tapeheadTrackUsesIndependentTransportVisual", loop)
assert choice > loop
assert "const int32_t visualMasterRow = hybridVisuals ? song.row : currRow;" in draw
assert "const bool trackLengthVisualActive = lengthTopologyActive" in draw
assert "songPlaying, fastTrackVisible, trackLengthVisualActive" in draw
assert "songPlaying, fastTrackVisible, lengthTopologyActive" not in draw
assert "fastTrack->sourceRow" in draw
assert "fastTracksPOCResolveMasterSourceRow" in draw
assert "fastTracksPOCGetFastTrackLength" in draw
assert "tapeheadActionTransportPunchIsFrozen" in draw

# UI-derived channel state must be validated before the fixed FastTracks
# snapshot (and the later pattern/LEN accessors) are indexed.
channel_calc = draw.index("const int32_t absoluteChannel = ui.channelOffset + j;", loop)
channel_guard = draw.index("absoluteChannel >= MAX_CHANNELS", channel_calc)
snapshot_access = draw.index("&fastTracksSnapshot.tracks[absoluteChannel]", channel_calc)
assert channel_calc < channel_guard < snapshot_access
assert "absoluteChannel >= song.numChannels" in draw[channel_calc:snapshot_access]

# FastTracks ratios are formatted with ':' but FT2's tiny font only renders
# alphanumerics. PR42's Pattern Editor scoped wrapper must draw the two colon
# pixels without changing the legacy global tiny-text implementation.
assert 'snprintf(ratioText, sizeof (ratioText), "%u:%u", numerator, denominator);' in draw
assert "tapeheadTextOutTinyWithColon" in policy
assert "if (str[i] != ':')" in policy
assert "((yPos + 2) * SCREEN_W) + colonX" in policy
assert "((yPos + 4) * SCREEN_W) + colonX" in policy
assert "#define textOutTiny tapeheadTextOutTinyWithColon" in policy

# Strum visual state is UI-only and participation continues to use the existing
# PatternJogFastTracks ownership rule.
assert "PATTERN_JOG_VISUAL_HOLD_MS 120" in actions
assert "tapeheadActionPatternJogTrackParticipates" in actions
assert "channelAcceptsPatternJog" in actions
assert "tapeheadActionPatternJogGetVisualPosition" in draw
assert "jogOverridesPrivateHead" in draw

# The option defaults enabled when absent and can be disabled explicitly.
assert '"PerTrackTransportVisuals"' in policy
assert "cached = 1" in policy
assert '"false"' in policy and '"off"' in policy and '"0"' in policy

# Repeated tapehead.ini sections/keys intentionally use the repository's
# last-value-wins convention. The parser must therefore keep scanning after a
# recognized PerTrackTransportVisuals assignment instead of breaking early.
key_check = policy.index('if (_stricmp(key, "PerTrackTransportVisuals"))')
fclose = policy.index("fclose(f);", key_check)
assignment_scan = policy[key_check:fclose]
assert "last-value-wins" in assignment_scan
assert "break;" not in assignment_scan

# Long independent patterns use deterministic viewport pages derived from the
# authoritative playback row rather than accumulating a duplicate visual row.
def page_start(row: int, rows: int) -> int:
    return 0 if row <= 0 or rows <= 0 else (row // rows) * rows

assert page_start(0, 17) == 0
assert page_start(16, 17) == 0
assert page_start(17, 17) == 17
assert page_start(255, 17) == 255

print("hybrid transport visual policy checks passed")
