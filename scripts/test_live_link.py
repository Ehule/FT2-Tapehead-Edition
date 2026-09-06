#!/usr/bin/env python3
"""Compile and run the cross-process Live Link transport contract."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    live_clock = (ROOT / "src/ft2_live_link.c").read_text()
    audio = (ROOT / "src/ft2_audio.c").read_text()
    selector = (ROOT / "src/ft2_audioselector.c").read_text()
    assert "TAPEHEAD_LIVE_LINK_DEVICE_NAME" in selector
    assert "TAPEHEAD_LIVE_LINK_RENDER_QUANTUM_FRAMES" in audio
    assert "openedSamples = TAPEHEAD_LIVE_LINK_RENDER_QUANTUM_FRAMES" in audio
    assert "renderAudioFrames(sampleFrames, 1)" in audio
    assert "tapeheadLiveLinkPause(true)" in audio
    assert "SDL_AtomicSet(&liveLinkPaused, pause ? 1 : 0);" in live_clock
    assert "tapeLinkWriterSetTransportState" in live_clock
    pause_sync = live_clock.index("if (pause && liveLinkMutex != NULL)")
    assert live_clock.index("SDL_LockMutex(liveLinkMutex);", pause_sync) > pause_sync
    callback_lock = live_clock.index("SDL_LockMutex(liveLinkMutex);")
    assert live_clock.index("if (SDL_AtomicGet(&liveLinkPaused))", callback_lock) > callback_lock
    with tempfile.TemporaryDirectory(prefix="tapehead-live-link-") as temp:
        binary = Path(temp) / "test_live_link"
        subprocess.run(
            [
                "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-Isrc", "scripts/test_live_link.c", "src/tape_link.c",
                "-lm", "-o", str(binary),
            ],
            cwd=ROOT,
            check=True,
        )
        subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
