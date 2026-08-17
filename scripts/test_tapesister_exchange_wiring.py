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

    print("TapeSister exchange wiring checks passed.")


if __name__ == "__main__":
    main()
