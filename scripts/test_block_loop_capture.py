#!/usr/bin/env python3
"""Audit literal Block Loop, quick capture, and destination wiring."""

from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def ordered(text: str, *needles: str) -> None:
    position = -1
    for needle in needles:
        position = text.index(needle, position + 1)


def main() -> None:
    replayer = (ROOT / "src/ft2_replayer.c").read_text()
    keyboard = (ROOT / "src/ft2_keyboard.c").read_text()
    renderer = (ROOT / "src/ft2_wav_renderer.c").read_text()
    capture = (ROOT / "src/ft2_capture.c").read_text()
    exchange = (ROOT / "src/ft2_tapesister_exchange.c").read_text()
    config = (ROOT / "src/ft2_config.c").read_text()
    main_c = (ROOT / "src/ft2_main.c").read_text()
    video = (ROOT / "src/ft2_video.c").read_text()
    pattern_draw = (ROOT / "src/ft2_pattern_draw.c").read_text()
    project = (ROOT / "vs2026_project/ft2-clone/ft2-clone.vcxproj").read_text()

    # Block playback branches before every alternate tracker/deck transport.
    tick = replayer[replayer.index("void tickReplayer(void)"):
                      replayer.index("void resetMusic(void)")]
    ordered(tick, "if (blockLoop.active && songPlaying)", "sampleLauncherTick()",
            "polyMatrixHasAudioWork()", "fastTracksPOCGetSharedBoundary")
    block_tick = replayer[replayer.index("static void tickBlockLoop(void)"):
                            replayer.index("static void getNextPos(void)")]
    assert "rowNotes[i]" in block_tick
    assert "channelStart" in block_tick and "channelEnd" in block_tick
    assert "patternLauncher" not in block_tick
    assert "fastTracksPOC" not in block_tick
    assert "advanceBlockLoop();" in block_tick

    # Bxx/Dxx/E6 and Zxx cannot escape or mutate literal block playback.
    isolate = replayer[replayer.index("static void isolateBlockEvent"):
                       replayer.index("static void advanceBlockLoop")]
    for token in ("0x0B", "0x0D", "0x23", "0x60"):
        assert token in isolate
    for token in ("0x0F", "0x10", "0x11", "0xE0"):
        assert token in isolate

    # Ctrl+L and live Shift+Arrow are claimed before ordinary editor actions.
    ordered(keyboard, "if (keycode == SDLK_l", "if (keycode == SDLK_ESCAPE)",
            "if (handleEditKeys(keycode, scancode))")
    assert "tapeheadBlockLoopResize(rowDelta, channelDelta)" in keyboard
    f8 = keyboard[keyboard.index("case SDLK_F8:"):
                  keyboard.index("case SDLK_F9:")]
    ordered(f8, "keyb.leftAltPressed", "tapeheadBlockLoopIsActive()",
            "tapeheadConfig.f8ExtractBlock")
    assert "tapeheadCaptureQuickBlock()" in f8

    # Offline rendering first discards one production-mixer cycle so the WAV
    # begins with the same carried voice state heard at a live loop seam. It
    # then captures exactly one cycle and resumes audition on the main thread.
    ordered(renderer, "static bool renderBlockPreroll",
            "mixReplayerTickToBuffer(tickSamples, wavRenderBuffer, bitDepth)",
            "tapeheadBlockLoopClearCycleCompleted()",
            "static int32_t renderWavThread")
    ordered(renderer, "if (wavRenderBlockEnabled &&",
            "renderBlockPreroll(bitDepth, &tickSamplesFrac, &cancelled)",
            "while (!renderDone)")
    assert "tapeheadBlockLoopCycleCompleted()" in renderer
    assert "startWavBlockRenderToFile" in renderer
    assert "SDL_AtomicSet(&job->finished, true)" in capture
    assert "tapeheadCapturePoll();" in main_c
    assert "tapeheadBlockLoopStart(&job->blockSpec)" in capture

    # Replacing a transient overlay restores its clean backing frame first,
    # and the selection uses the same playback-centered row as the pattern.
    overlay = video[video.index("void showRecPlusOverlay"):
                    video.index("static void drawRecPlusOverlay")]
    ordered(overlay, "recPlusOverlayFrames > 0",
            "memcpy(video.frameBuffer, recPlusOverlayBackup",
            "memcpy(recPlusOverlayBackup, video.frameBuffer")
    assert "writePatternBlockMark(visualMasterRow" in pattern_draw
    assert "tapeheadBlockLoopIsActive() && paletteIndex == PAL_DESKTOP" in pattern_draw
    assert "video.palette[PAL_BLCKMRK]" in pattern_draw

    # Captures and exchange publication are deliberately separate outputs.
    assert 'capturesName[] = "Captures"' in capture
    assert "makeDirectory(directory)" in capture
    assert '"%s_%03u.wav"' in capture
    assert "SYSREQ_TYPE_RENDER_DESTINATION" in exchange
    assert "tapeheadCaptureRender(&plan" in exchange
    assert "createTransferFolders" in exchange
    assert "captureFolder" in config and '"Capture"' in config
    assert "ft2_capture.c" in project and "ft2_capture.h" in project

    # Every changed translation unit remains valid against the vendored SDL
    # headers used by the Windows project.
    include_sdl = ROOT / "vs2026_project/ft2-clone/sdl/include"
    for source in (
        "ft2_capture.c", "ft2_replayer.c", "ft2_keyboard.c",
        "ft2_wav_renderer.c", "ft2_tapesister_exchange.c",
        "ft2_tapesister_render.c", "ft2_config.c", "ft2_main.c",
        "ft2_sysreqs.c", "ft2_video.c", "ft2_pattern_draw.c",
    ):
        subprocess.run(
            ["gcc", "-std=c11", "-D_DEFAULT_SOURCE", "-DNDEBUG",
             f"-I{include_sdl}", f"-I{ROOT / 'src'}", "-fsyntax-only",
             str(ROOT / "src" / source)],
            check=True,
            cwd=ROOT,
        )

    print("Literal Block Loop and agnostic capture wiring tests passed.")


if __name__ == "__main__":
    main()
