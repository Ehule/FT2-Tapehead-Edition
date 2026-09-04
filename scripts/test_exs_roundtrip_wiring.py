#!/usr/bin/env python3
"""Guard EXS export/replacement integration and atomicity wiring."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def require(text: str, token: str, source: str) -> None:
    if token not in text:
        raise AssertionError(f"{source} is missing {token!r}")


def main() -> None:
    loader = (ROOT / "src/ft2_sample_loader.c").read_text()
    diskop = (ROOT / "src/ft2_diskop.c").read_text()
    mouse = (ROOT / "src/ft2_mouse.c").read_text()
    saver = (ROOT / "src/ft2_sample_saver.c").read_text()
    guide = (ROOT / "docs/EXS_SAMPLE_EXPORT.md").read_text()
    project = (ROOT / "vs2026_project/ft2-clone/ft2-clone.vcxproj").read_text()

    for field in (
        "InstrumentIndex=%d",
        "SampleIndex=%d",
        "File=%s",
        "LengthFrames=%d",
        "C4Frequency=%d",
        "Flags=%u",
    ):
        require(saver, field, "EXS exporter")

    require(saver, "exsStripKnownSampleExtension(name);", "EXS filename normalization")

    for token in (
        "SAMPLE_FOLDER_IMPORT_EXS",
        "prepareEXSSample",
        "confirmEXSRoundTrip",
        'undoTransactionBegin("Replace Samples from EXS")',
        "undoTransactionPrepareInstrumentAfter",
        "undoTransactionPreparedInstrumentsFitMemoryLimit",
        "lockMixerCallback();",
    ):
        require(loader, token, "EXS replacement loader")
    assert loader.index("confirmEXSRoundTrip") < loader.index(
        'undoTransactionBegin("Replace Samples from EXS")'
    )
    assert loader.index("undoTransactionPreparedInstrumentsFitMemoryLimit") < loader.index(
        "lockMixerCallback();", loader.index('undoTransactionBegin("Replace Samples from EXS")')
    )

    require(diskop, "replaceSamplesFromEXSFolder", "Disk Op")
    require(diskop, "keycode == SDLK_r && keyb.leftCtrlPressed", "Disk Op shortcut")
    require(diskop, "_wfullpath(fullPath, strU, PATH_MAX+1)", "Windows folder deletion")
    require(diskop, "fullPath[pathLength+1] = L'\\0';", "Windows path-list termination")
    require(diskop, "editor.sampleSaveMode == SMP_SAVE_MODE_EXS", "EXS directory-name retention")
    require(mouse, "replaceSamplesFromEXSFolder();", "EXS mouse gesture")
    require(guide, "One **Undo**", "EXS guide")
    require(project, "ft2_exs_manifest.c", "Visual Studio project")
    require(project, "ft2_exs_manifest.h", "Visual Studio project")

    print("EXS round-trip wiring checks passed.")


if __name__ == "__main__":
    main()
