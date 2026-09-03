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
    acknowledgement = (ROOT / "src/ft2_tapesister_ack.c").read_text()
    loader = (ROOT / "src/ft2_sample_loader.c").read_text()
    main_c = (ROOT / "src/ft2_main.c").read_text()
    mouse = (ROOT / "src/ft2_mouse.c").read_text()
    config = (ROOT / "src/ft2_config.c").read_text()
    diskop = (ROOT / "src/ft2_diskop.c").read_text()
    palette = (ROOT / "src/ft2_palette.c").read_text()
    pushbuttons = (ROOT / "src/ft2_pushbuttons.c").read_text()
    textboxes = (ROOT / "src/ft2_textboxes.c").read_text()
    saver = (ROOT / "src/ft2_sample_saver.c").read_text()
    renderer = (ROOT / "src/ft2_wav_renderer.c").read_text()
    sysreqs = (ROOT / "src/ft2_sysreqs.c").read_text()
    xm_saver = (ROOT / "src/ft2_module_saver.c").read_text()
    xm_loader = (ROOT / "src/modloaders/ft2_load_xm.c").read_text()

    # Inbox eligibility, session deferral, manual reopening, and UI ownership.
    assert "unicodeEndsWithPartial(name)" in exchange
    assert 'strcmp(offer.sender, "tapesister")' in exchange
    assert 'strcmp(offer.recipient, "tapehead")' in exchange
    assert "acknowledgementName" in exchange and "pathExists(path)" in exchange
    assert "(!manual && folderIsDeferred(folder))" in exchange
    assert "scanInboxThread" in exchange
    assert 'SDL_CreateThread(scanInboxThread' in exchange
    assert "findPendingOffer(job->root, job->manual" in exchange
    assert "SDL_AtomicSet(&job->finished, true)" in exchange
    assert "SDL_WaitThread(inboxScanThread, NULL)" in exchange
    assert "ui.sysReqShown" in exchange and "sampleLoaderIsBusy()" in exchange
    assert "EXCHANGE_POLL_INTERVAL_MS 1000" in exchange
    assert 'tapeheadPresenceName[]' in exchange
    assert 'tapeSisterPresenceName[]' in exchange
    assert 'refreshTapeheadPresence();' in exchange
    assert 'tapeSisterIsRunning()' in exchange
    assert 'forceNewInstance' in exchange
    assert 'openTapeSisterExchangeFolder()' in exchange
    assert "OCCUPIED" in exchange and "clears all other sample slots" in exchange
    assert "Multi-page TapeSister bank" in exchange
    assert "empty TapeSister tiles do not erase samples" in exchange
    assert "TAPEHEAD_EXCHANGE_LAYOUT_PAGE_INSTRUMENTS" in exchange
    assert "referenced WAV" in exchange and "missing or unreadable" in exchange

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
    create = exchange[exchange.index("static bool createTransferFolders"):
                      exchange.index("static bool publishSource")]
    publish = exchange[exchange.index("static bool publishSource"):
                       exchange.index("static void cleanupRenderPartial")]
    assert "makeDirectory(partialFolder)" in create
    assert "pathExists(finalFolder) || pathExists(partialFolder)" in create
    ordered(publish, "saveWAVSampleDirect(", 'UNICHAR_FOPEN(path, "wb")',
            "UNICHAR_RENAME(partialFolder, finalFolder)")
    assert "cleanupPartial(partialFolder, source)" in publish
    launch = exchange[exchange.index("static bool launchTapeSister"):exchange.index("static bool publishSource")]
    assert "tapeSisterExecutablePath[0] == '\\0'" in launch
    assert "CreateProcessW" in launch and "execl(executable, executable" in launch
    assert "system(" not in exchange and "ShellExecute" not in exchange
    assert "UNC" in exchange and "driveAbsolute" in exchange and "uncAbsolute" in exchange

    # Render exchange reuses the WAV engine and the same atomic publisher. The
    # existing v1 manifest remains immediately consumable; provenance lives in
    # a sidecar that older TapeSister builds can ignore.
    render_publish = exchange[exchange.index("static bool writeRenderTransferFiles"):
                              exchange.index("static void confirmAndPublish")]
    assert 'metadataName[] = "render.tapehead"' in render_publish
    ordered(render_publish, "tapeheadRenderWriteMetadata(",
            '"TAPESISTER_EXCHANGE 1\\n"',
            "UNICHAR_RENAME(job->partialFolder, job->finalFolder)")
    assert '"layout=instrument_samples\\n"' in render_publish
    assert '"count=1\\n"' in render_publish
    assert '"item=1,1,1,%s\\n"' in render_publish
    assert "cleanupRenderPartial(job)" in render_publish
    assert "TAPEHEAD_RENDER_MAX_TAPESISTER_FRAMES" in render_publish
    assert "startWavRenderToFile(file" in render_publish
    assert "TAPEHEAD_RENDER_PATTERN_MIX" in exchange
    assert "TAPEHEAD_RENDER_PATTERN_TRACK" in exchange
    assert "TAPEHEAD_RENDER_SONG_TRACK" in exchange
    assert "TAPEHEAD_RENDER_SONG_MIX" in exchange
    assert '"Send samples", "Render audio"' in sysreqs
    assert '"Pattern mix", "Pattern track", "Song track", "Song mix"' in sysreqs
    assert "wavRenderCompletionCallback completion" in renderer
    assert "dontRenderThisChannel = i != soloChannel" in renderer
    assert "SDL_AtomicCAS(&renderThreadActive" in renderer
    assert "SDL_AtomicSet(&renderThreadActive, false)" in renderer

    # The asynchronous importer requires actual WAV content, decodes the whole
    # batch before allocating Undo, commits under one mixer lock, then acks.
    thread_body = loader[loader.index("static int32_t loadSampleFolderThread"):loader.index("static uint32_t assignFolderInstrumentDestinations")]
    ordered(thread_body, "for (uint32_t i = 0; i < job->fileCount; i++)",
            "decodeFolderSample(", "commitTapeSisterExchange(")
    decode = loader[loader.index("static bool decodeFolderSample"):loader.index("static void freeDecodedFolderSamples")]
    assert "requireWav && format != FORMAT_WAV" in decode
    commit = loader[loader.index("static bool commitTapeSisterExchange"):loader.index("static instr_t *makeLauncherBankInstrument")]
    ordered(commit, "findOrCreateExchangeInstrument(",
            'undoTransactionBegin("Import TapeSister Transfer")',
            "undoTransactionAddInstrument(",
            "undoTransactionPrepareInstrumentAfter(",
            "undoTransactionPreparedInstrumentsFitMemoryLimit()",
            "lockMixerCallback();",
            "freeInstr(destination);", "unlockMixerCallback();",
            "undoTransactionCommit();", "tapeheadExchangeWriteAcknowledgement(")
    assert sum(line.strip() == "lockMixerCallback();" for line in commit.splitlines()) == 1
    assert "undoCancelTransaction();" in commit
    assert "instr_t *newInstruments[MAX_INST]" in commit

    # Version 2 is a sparse patch: it deep-clones each destination before Undo
    # and frees/replaces only listed slots. Version 1 does not enable this flag.
    clone = loader[loader.index("static instr_t *cloneExchangeDestinationInstrument"):loader.index("static instr_t *findOrCreateExchangeInstrument")]
    ordered(clone, "memcpy(instrument, instr[destination]",
            "memset(instrument->smp", "cloneSample(")
    assert "freeFolderInstrument(instrument);" in clone
    assert "freeTmpSample(&instrument->smp[sample]);" in commit
    exchange_load = loader[loader.index("bool loadTapeSisterExchange"):]
    assert "exchangePreserveUnlistedSamples = offer->layout ==" in exchange_load
    assert "TAPEHEAD_EXCHANGE_LAYOUT_PAGE_INSTRUMENTS" in exchange_load
    assert "job->exchangePreserveUnlistedSamples" in commit
    failure = commit[commit.index("allocationError:"):]
    assert "freeInstr(" not in failure
    assert "tapeheadExchangeWriteAcknowledgement(" not in failure

    ordered(acknowledgement, 'UNICHAR_STRCAT(temporaryU, ".tmp")',
            'UNICHAR_FOPEN(temporaryU, "wb")',
            "UNICHAR_RENAME(temporaryU, pathU)")

    # Integration extends, rather than replaces, the established paths.
    assert "tapeSisterExchangeInit();" in main_c
    assert "tapeSisterExchangePoll(false);" in main_c
    assert "tapeSisterExchangeShutdown();" in main_c
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
