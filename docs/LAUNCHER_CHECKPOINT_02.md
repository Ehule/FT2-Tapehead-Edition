# Tapehead Standalone Launcher — Checkpoint 02

> **Historical checkpoint.** The full-window surface later became Deck Matrix
> with native Sample Banks and the Sample Matrix Editor. See
> [`DECK_MATRIX.md`](DECK_MATRIX.md).

Checkpoint 02 keeps the validated Checkpoint 01 audio and transport engine and
adds the first dedicated launcher surface. It is deliberately still compiled
from the Tapehead source tree so XM effects, instruments, MIDI, FasTracks,
Poly ownership and multichannel routing remain available.

## Full-window launcher

- The XM Pattern Matrix and Sample Matrix are visible at the same time.
- Each deck shows 32 large tiles in a four-column/eight-row grid.
- Eight page buttons expose all 256 possible XM patterns.
- Bank buttons use their starting pattern (`00`, `20` ... `E0`) as compact,
  clearly separated labels.
- Pattern tiles retain song-membership, exposure, Q queue, Poly slot and
  graceful-stop states.
- Sample tiles show a shortened filename, output bus and Q/Poly slot state.
- Both matrices keep the Checkpoint 01 ownership rule: neither deck can evict
  the other.
- **Stop All** stops the XM transport, Pattern Poly spools and all five Sample
  Launcher voices.
- **Tracker** returns to the complete Tapehead editor without unloading the XM
  or Sample deck.
- The launcher is composited after FT2's live tracker counters, preventing song
  position, pattern length, BPM and TPL fields from bleeding into the matrices.

## Controls

- Left-click a tile: Q cue.
- Middle-click a tile: latch or pull a Poly loop.
- Alt-click a Pattern tile: mask or expose it.
- Alt-click a Sample tile: cycle its output destination.
- Ctrl+Shift-click an active Pattern Poly tile: hand it to Q at its boundary.
- Shift+middle-click retains the existing immediate Poly pull behavior.
- Ctrl+Alt-click the small launcher's **Exit / Patt.** or **Exit / Samp.**
  button to reopen the full-window launcher after returning to the tracker.

Sample-folder loading still uses the validated Checkpoint 01 route: choose
**Tracker**, open Disk Op in Sample mode, navigate to a folder, and
Ctrl+Shift-click **Sample**. The next interface checkpoint can replace this
round-trip with native Load XM and Load Samples controls.

## Configuration

`release/other/tapehead.ini` contains:

```ini
[Launcher]
Enabled=true
Standalone=true
```

`Standalone=false` preserves the compact switchable Checkpoint 01 deck.

## Intentionally next

- Native Load XM and Load Samples buttons
- Draggable Sample tile ordering
- Sidecar session persistence
- Cross-deck scenes
- Per-sample one-shot/loop, gain and pan controls
- Optional independent executable target once the interface and session model
  are stable
