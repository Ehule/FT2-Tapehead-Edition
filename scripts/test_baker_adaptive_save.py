#!/usr/bin/env python3
"""Verify the isolated Adaptive XM option is wired without replacing old paths."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    baker_h = (ROOT / "src/ft2_baker.h").read_text()
    baker = (ROOT / "src/ft2_baker.c").read_text()
    diskop = (ROOT / "src/ft2_diskop.c").read_text()
    sysreqs = (ROOT / "src/ft2_sysreqs.c").read_text()
    docs = (ROOT / "docs/COMPOSITION_BAKER.md").read_text()

    assert "BAKER_OUTPUT_STANDARD_XM = 0" in baker_h
    assert "BAKER_OUTPUT_TAPEHEAD_XM" in baker_h
    assert "BAKER_OUTPUT_ADAPTIVE_XM" in baker_h
    assert '"Standard XM", "Tapehead XM", "Adaptive XM", "Cancel"' in sysreqs
    assert "outputChoice == 3" in diskop
    assert "? BAKER_OUTPUT_ADAPTIVE_XM : outputChoice == 2" in diskop
    assert '"-BAKED-ADAPTIVE"' in diskop

    assert "static bool saveAdaptiveBakeResult" in baker
    assert "bakerAdaptiveXMBuild(" in baker
    assert "bakerAdaptivePatternSetBuild(" in baker
    assert "bakerAdaptivePatternSetAnchorEmptyPatterns(" in baker
    assert "adaptiveChannels = outputChannels < MAX_CHANNELS" in baker
    assert "timingChannel" in baker
    assert "song.numChannels = adaptiveChannels" in baker
    assert "saveXM(bakeFilenameU)" in baker
    assert "memcpy(pattern, savedPatterns" in baker
    assert "song = savedSong" in baker
    assert "bakerAssetsUninstall()" in baker

    # LEN/CONTROL is already rendered into the flattened tick stream. Baked
    # modules must not inherit the source pattern map, and the live module's
    # metadata plus exact FastTracks phase must be restored after saving.
    assert "savedPatternMetadata[MAX_PATTERNS]" in baker
    assert "fastTracksPOCResetAllPatternMetadata();" in baker
    assert "fastTracksPOCSetPatternMetadata(i, &savedPatternMetadata[i]);" in baker
    assert "fastTracksPOCSetRuntimeState(&savedFastTracksRuntime);" in baker

    # The two established output paths remain selected by the old ternary and
    # only the explicit Adaptive target enters the new helper.
    assert "if (bakeOutputTarget == BAKER_OUTPUT_ADAPTIVE_XM)" in baker
    assert "bakeOutputTarget == BAKER_OUTPUT_TAPEHEAD_XM ?" in baker
    assert "saveXM(bakeFilenameU) : saveStandardXM(bakeFilenameU)" in baker
    assert "It never replaces or" in docs
    assert "changes the existing Standard XM" in docs

    print("Adaptive XM save-path wiring checks passed.")


if __name__ == "__main__":
    main()
