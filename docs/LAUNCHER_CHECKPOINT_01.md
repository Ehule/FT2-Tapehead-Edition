# Tapehead Pattern + Sample Launcher — Checkpoint 01

This branch is the first working vertical slice of the combined launcher. It
starts from the consolidated post-HD experimental source tree and does not
touch the frozen RC1 lineage.

## What is working

- Pattern and Sample decks share the XM pattern boundary, BPM and TPL.
- The Pattern deck retains the existing Q queue, four Poly spools,
  next-free-tunnel allocation, handoffs and multichannel routing.
- Alt-click masks or exposes a Pattern tile without changing the XM, its song
  order or its pattern data. Masked tiles remain dimly visible so they can be
  restored.
- The Sample deck loads up to 32 naturally sorted audio files from the current
  Disk Op folder into storage that is independent of the XM instrument pool.
- Sample left-click is the Q transport. It replaces the current Q sample at
  the next XM pattern boundary while playback is running.
- Sample middle-click latches or gracefully pulls one of four independent Poly
  sample voices.
- Sample Q and Poly voices never occupy an XM tracker channel, so neither deck
  can evict the other.
- Samples play at their decoded natural speed and loop over the complete file.
  There is no time stretching and no experimental one-second fade patch.
- Alt-clicking a Sample tile cycles its output. Stereo mode uses logical buses
  A–P; Mono Outputs mode uses the exposed physical destinations.
- `tapehead.ini` can open directly into the launcher with
  `[Launcher] Enabled=true`. HD remains available but defaults off in this
  checkpoint.

## Controls

1. Ctrl-click **Swap Bank** to open the launcher if it is not already open.
2. Ctrl-click the launcher's **Exit / Patt.** or **Exit / Samp.** button to
   switch between the Pattern and Sample decks. Plain click exits the launcher.
3. In Disk Op, select **Sample**, navigate to a folder, then Ctrl+Shift-click
   the **Sample** selector to load that folder into the Sample deck.
4. Left-click a tile for Q playback. Middle-click a tile for Poly playback.
5. Alt-click a Pattern tile to mask/expose it. Alt-click a Sample tile to cycle
   its output destination.

When the XM transport is stopped, a Sample request starts immediately. During
XM playback, Sample requests commit at the same pattern boundary as Pattern
Matrix requests. A fifth Poly request is rejected with the existing
`No more track Lanes available` message.

## Validation

Run:

```bash
python3 scripts/test_all_native.py
```

Checkpoint 01 adds native tests for Sample Q/Poly state and for routing the
independent Sample voice pool to a non-main output bus. The original FasTracks,
Pattern/Poly ownership, JACK and multichannel regression suites remain in the
same runner.

## Intentionally next

The dedicated full-window two-matrix surface is implemented in
`LAUNCHER_CHECKPOINT_02.md`.

- Drag-and-drop Sample tile rearrangement
- Sidecar session save/load
- Capturable cross-deck scenes
- Sample one-shot/loop choice, gain and pan editing
- A dedicated large launcher layout instead of the borrowed FT2 instrument
  switcher surface
- Live Linux/JACK and Windows hardware validation
