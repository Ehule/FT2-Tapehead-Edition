# Phase 1C manual checklist

1. Launch `tapesister` and confirm the six factory recipes appear.
2. Select and hear every recipe; confirm their identities are plainly distinct.
3. Play single notes and chords from both computer-keyboard rows.
4. Change base octave with `[` and `]` and check pitch movement and limits.
5. Compare one-shot and gated behavior using `Ctrl+G`; confirm plain `G` remains a note
   and listen for start/release clicks.
6. Click, drag away from, and release keys on the onscreen two-octave keyboard.
7. Switch recipes while notes are active and confirm old notes remain valid.
8. Resize the window and verify nearest pixels, aspect ratio, and letterboxing.
9. Launch with `--palette-file PATH` and with `--palette dark`.
10. Launch with `--recipe PATH`, then with a missing/invalid path.
11. Force audio-device failure and verify the UI stays open with a useful status.
12. Use Space/Stop All, close the window, and check for clicks, stuck notes,
    hangs, or shutdown errors.
13. Trigger OVERLOAD with a loud chord; confirm it clears after roughly 750ms
    of clean audio and reappears if another overload occurs.
