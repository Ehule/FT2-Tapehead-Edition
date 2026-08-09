#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

ini_text = (ROOT / "release/other/tapehead.ini").read_text(encoding="utf-8")
config_source = (ROOT / "src/ft2_config.c").read_text(encoding="utf-8")
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
