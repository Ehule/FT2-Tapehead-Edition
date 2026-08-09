#!/usr/bin/env python3
"""Compile core tests and verify the Tapehead pitch integration points."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ft2-microtonal-") as tmp:
        executable = Path(tmp) / "test_microtonal"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "src"),
                str(ROOT / "tests/test_microtonal.c"),
                str(ROOT / "src/ft2_microtonal.c"),
                "-lm",
                "-o",
                str(executable),
            ],
            check=True,
            cwd=ROOT,
        )
        subprocess.run([str(executable)], check=True, cwd=ROOT)

    replayer = (ROOT / "src/ft2_replayer.c").read_text()
    audio = (ROOT / "src/ft2_audio.c").read_text()
    strum = (ROOT / "src/ft2_tapehead_actions.c").read_text()
    baker = (ROOT / "src/ft2_baker.c").read_text()
    diskop = (ROOT / "src/ft2_diskop.c").read_text()
    saver = (ROOT / "src/ft2_module_saver.c").read_text()

    assert "microTune,          // M - Tapehead MicroTune" in replayer
    assert "microDrift,         // N - Tapehead MicroDrift" in replayer
    assert "microtonalAdvance(&ch->microtonal, song.BPM)" in replayer
    assert "microtonalScaleDelta(baseDelta" in audio
    assert "applyChannelMicrotonalEffect((uint8_t)ch" in strum
    assert "bakerCaptureManualEvent(ch, event)" in strum
    assert "microtonalEffectIsPitchExtension(flattened.efx)" in baker
    assert "bakeOutputTarget == BAKER_OUTPUT_STANDARD_XM" in baker
    assert "bakePreservedMicrotonalCommands++" in baker
    assert "saveXM(bakeFilenameU) : saveStandardXM(bakeFilenameU)" in baker
    assert "pitch omitted" in baker
    assert "SYSREQ_TYPE_BAKE_OUTPUT" in diskop
    assert '"-BAKED-TAPEHEAD" : "-BAKED"' in diskop
    assert "standardXMSave && microtonalEffectIsPitchExtension(bytes[3])" in saver
    print("Microtonal playback, strum and compatibility-bake hooks verified.")


if __name__ == "__main__":
    main()
