#!/usr/bin/env python3
"""Verify TapeSister runtime integration and atomic ordering invariants."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def ordered(text: str, *needles: str) -> None:
    position = -1
    for needle in needles:
        next_position = text.find(needle, position + 1)
        assert next_position >= 0, needle
        assert next_position > position, needle
        position = next_position


def main() -> None:
    exchange = (ROOT / "src/ft2_tapesister_exchange.c").read_text()
    loader = (ROOT / "src/ft2_sample_loader.c").read_text()
    main_c = (ROOT / "src/ft2_main.c").read_text()
    mouse = (ROOT / "src/ft2_mouse.c").read_text()
    config = (ROOT / "src/ft2_config.c").read_text()
    diskop = (ROOT / "src/ft2_diskop.c").read_text()
    palette = (ROOT / "src/ft2_palette.c").read_text()
    pushbuttons = (ROOT / "src/ft2_pushbuttons.c").read_text()
    textboxes = (ROOT / "src/ft2_textboxes.c").read_text()
    saver = (ROOT / "src/ft2_sample_saver.c").read_text()
    xm_saver = (ROOT / "src/ft2_module_saver.c").read_text()
    xm_loader = (ROOT / "src/modloaders/ft2_load_xm.c").read_text()

    # Inbox eligibility, session deferral, manual reopening, and UI ownership.
    assert "unicodeEndsWithPartial(name)" in exchange
    assert 'strcmp(offer.sender, "tapesister")' in exchange
    assert 'strcmp(offer.recipient, "tapehead")' in exchange
    assert "acknowledgementName" in exchange and "pathExists(path)" in exchange
    assert "(!manual && folderIsDeferred(folder))" in exchange
    assert "findPendingOffer(manualRequest" in exchange
    assert "ui.sysReqShown" in exchange and "sampleLoaderIsBusy()" in exchange
    assert "EXCHANGE_POLL_INTERVAL_MS 1000" in exchange
    assert 'tapeheadPresenceName[]' in exchange
    assert 'tapeSisterPresenceName[]' in exchange
    assert 'refreshTapeheadPresence();' in exchange
    assert 'tapeSisterIsRunning()' in exchange
    assert 'forceNewInstance' in exchange
    assert 'openTapeSisterExchangeFolder()' in exchange
    assert "OCCUPIED" in exchange and "clears all other sample slots" in exchange

    # Both send layouts and deterministic source mappings are explicit.
    assert "collectCurrentInstrument" in exchange
    assert "TAPEHEAD_EXCHANGE_LAYOUT_INSTRUMENT_SAMPLES" in exchange
    assert "tapeSisterTile = sample + 1" in exchange
    assert "collectInstrumentRange" in exchange
    assert "TAPEHEAD_EXCHANGE_LAYOUT_SEPARATE_INSTRUMENTS" in exchange
    assert "tapeSisterTile = index + 1" in exchange
    assert "break;" in exchange[exchange.index("collectInstrumentRange"):exchange.index("cleanupPartial")]

    # Publication creates only a new .partial folder, writes the manifest after
    # every WAV, then performs one final rename. Blank launch paths are valid.
    publish = exchange[exchange.index("static bool publishSource"):exchange.index("static void confirmAndPublish")]
    ordered(publish, "makeDirectory(partialFolder)", "saveWAVSampleDirect(",
            'UNICHAR_FOPEN(path, "wb")', "UNICHAR_RENAME(partialFolder, finalFolder)")
    assert "pathExists(finalFolder) || pathExists(partialFolder)" in publish
    assert "cleanupPartial(partialFolder, source)" in publish
    launch = exchange[exchange.index("static bool launchTapeSister"):exchange.index("static bool publishSource")]
    assert "tapeSisterExecutablePath[0] == '\\0'" in launch
    assert "CreateProcessW" in launch and "execl(executable, executable" in launch
    assert "system(" not in exchange and "ShellExecute" not in exchange
    assert "UNC" in exchange and "driveAbsolute" in exchange and "uncAbsolute" in exchange

    # The asynchronous importer decodes the whole batch before allocating Undo,
    # commits all replacement instruments under one mixer lock, then acks.
    thread_body = loader[loader.index("static int32_t loadSampleFolderThread"):loader.index("static uint32_t assignFolderInstrumentDestinations")]
    ordered(thread_body, "for (uint32_t i = 0; i < job->fileCount; i++)",
            "decodeFolderSample(", "commitTapeSisterExchange(")
    commit = loader[loader.index("static bool commitTapeSisterExchange"):loader.index("static instr_t *makeLauncherBankInstrument")]
    ordered(commit, "findOrCreateExchangeInstrument(",
            'undoTransactionBegin("Import TapeSister Transfer")',
            "undoTransactionAddInstrument(", "lockMixerCallback();",
            "freeInstr(destination);", "unlockMixerCallback();",
            "undoTransactionCommit();", "writeExchangeAcknowledgement(")
    assert sum(line.strip() == "lockMixerCallback();" for line in commit.splitlines()) == 1
    assert "undoCancelTransaction();" in commit

    # Integration extends, rather than replaces, the established paths.
    assert "tapeSisterExchangeInit();" in main_c
    assert "tapeSisterExchangePoll(false);" in main_c
    assert "tapeSisterExchangeOpenMenu();" in mouse
    assert "saveWAVSampleDirect" in saver
    assert "SAMPLE_REVERSE_LOOP" in saver
    assert "SAMPLE_REVERSE_LOOP" in xm_saver and "& ~SAMPLE_REVERSE_LOOP" in xm_saver
    assert "SAMPLE_REVERSE_LOOP" in xm_loader and "& ~SAMPLE_REVERSE_LOOP" in xm_loader

    # Config > Layout exposes full scrolling paths and a dedicated, non-loading
    # Disk Op selector mode. Preset/PAT controls are compacted above the fields.
    assert "TB_CONF_TAPESISTER_EXCHANGE" in textboxes
    assert "TB_CONF_TAPESISTER_EXECUTABLE" in textboxes
    assert "TAPEHEAD_CONFIG_PATH_CAPACITY - 1" in textboxes
    assert "SDL_GetTicks()" in textboxes and "openTapeSisterPathBrowser" in textboxes
    assert "hideTextBox(TB_CONF_TAPESISTER_EXCHANGE)" in config
    assert "drawTapeSisterSwatches();" in palette
    ordered(palette, "showTextBox(TB_CONF_TAPESISTER_EXCHANGE)",
            "drawTextBox(TB_CONF_TAPESISTER_EXCHANGE)")
    ordered(palette, "showTextBox(TB_CONF_TAPESISTER_EXECUTABLE)",
            "drawTextBox(TB_CONF_TAPESISTER_EXECUTABLE)")
    assert "saveTapeSisterConfigPaths();" in textboxes
    assert "openTapeSisterExchangeFolder" in diskop
    assert "TAPESISTER_PATH_BROWSER_EXCHANGE" in diskop
    assert "TAPESISTER_PATH_BROWSER_EXECUTABLE" in diskop
    assert 'pushButtons[PB_DISKOP_SAVE].caption = "Select"' in diskop
    selector_branch = diskop[diskop.index("if (mode == MOUSE_MODE_NORMAL && tapeSisterPathBrowser"):
                             diskop.index("// remove file selection")]
    assert "openFile(" not in selector_branch
    assert '{ 475,  88, 154, 16' in pushbuttons
    assert '{ 475, 105, 154, 16' in pushbuttons
    assert 'textOutShadow(400,  92' in palette
    assert 'textOutShadow(400, 109' in palette

    print("TapeSister exchange wiring checks passed.")


if __name__ == "__main__":
    main()
