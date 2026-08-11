#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

ini_text = (ROOT / "release/other/tapehead.ini").read_text(encoding="utf-8")
config_source = (ROOT / "src/ft2_config.c").read_text(encoding="utf-8")
apc_source = (ROOT / "src/ft2_apc40_mk2.c").read_text(encoding="utf-8")
assert "\n[APC40MK2_MAP]\n" in ini_text
assert "\n+[APC40MK2_MAP]\n" not in ini_text
for key in ("GridTopPad01", "Activator08", "SceneLaunch1", "SceneLaunch5", "Play", "Record",
            "TapTempo", "Crossfader", "CueLevel"):
    assert f"\n{key}=" in ini_text
assert "PatternJogAudition=" in ini_text
assert "PatternJogFastTracks=" in ini_text

# Shipped/generated defaults live in code; the runtime INI is intentionally
# user-editable and must not make the regression suite fail when customized.
assert 'fputs("SceneLaunch1=MatrixSequenceBank\\n", f);' in config_source
assert '"SceneLaunch%d=MatrixSequenceRow:%d\\n", i + 1, i' in config_source
assert 'fputs("Play=TransportPlaySongToggle\\n", f);' in config_source
assert 'Stop=TransportStop' not in config_source
assert 'fputs("Shift=ShiftModifier\\nTapTempo=FastTrackGlobalModeToggle\\n", f);' in config_source
assert 'fputs("Crossfader=PatternJogAbsolute\\n", f);' in config_source
assert 'fputs("CueLevel=PatternJogRelative\\n", f);' in config_source
assert 'fputs("Footswitch=TransportPunch\\n\\n", f);' in config_source
assert 'fputs("PatternJogAudition=Latched\\n", f);' in config_source
assert 'fputs("PatternJogFastTracks=Ignore\\n", f);' in config_source
assert 'fputs("TransportFreezeAudio=Sustain\\n", f);' in config_source
assert 'fputs("TransportFreezePedalMode=Toggle\\n", f);' in config_source
assert 'fputs("TransportFreezeNavigation=Silent\\n", f);' in config_source
assert 'fputs("TransportFreezeResume=Next\\n\\n", f);' in config_source
assert "RecordArm01=FastTrackClutchToggle:1" in ini_text
assert "Footswitch=TransportPunch" in ini_text

# Startup must gate all feedback on the one successful Mode 2 send. The clear
# and authoritative refresh follow it, while ordinary refresh contains no
# introduction builder call and no writes to controls without LEDs.
open_body = apc_source.split("void tapeheadAPC40Mk2Open(void)", 1)[1].split(
    "void tapeheadAPC40Mk2Close(void)", 1)[0]
refresh_body = apc_source.split("void tapeheadAPC40Mk2Refresh(void)", 1)[1]
assert open_body.index("tapeheadMidiSurfaceOutputIsOpen") < open_body.index(
    "tapeheadAPC40Mk2BuildIntroduction") < open_body.index("tapeheadMidiSurfaceSend")
assert "feedbackActive = length > 0 && tapeheadMidiSurfaceSend" in open_body
assert open_body.index("clearFeedbackSurface();") < open_body.index(
    "tapeheadAPC40Mk2Refresh();")
assert "tapeheadAPC40Mk2BuildIntroduction" not in refresh_body
assert "sendNote(0, 0x41, fastTracksPOCMasterIsEnabled());" in apc_source
for unavailable_led in ("0x63", "0x64", "0x65"):
    assert f"sendNote(0, {unavailable_led}" not in refresh_body

with tempfile.TemporaryDirectory(prefix="ft2-apc40-") as temp_dir:
    output = Path(temp_dir) / "test_apc40_mk2"
    command = [
        "cc", "-std=c11", "-O2", "-DHAS_MIDI",
        "-ffunction-sections", "-fdata-sections",
        "-I", str(ROOT / "src"),
        "-I", str(ROOT / "vs2026_project/ft2-clone/sdl/include"),
        str(ROOT / "tests/test_apc40_mk2.c"),
        str(ROOT / "src/ft2_apc40_mk2.c"),
        str(ROOT / "src/ft2_midi_map.c"),
        "-Wl,--gc-sections", "-o", str(output),
    ]
    subprocess.run(command, check=True)
    subprocess.run([str(output)], check=True)
