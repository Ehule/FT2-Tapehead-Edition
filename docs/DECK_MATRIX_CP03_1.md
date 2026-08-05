# Deck Matrix CP03.1

> **Historical delta.** These changes are included in CP04.6. Use
> [`DECK_MATRIX.md`](DECK_MATRIX.md) and
> [`DECK_MATRIX_CP03.md`](DECK_MATRIX_CP03.md) for current behavior.

CP03.1 is a focused transport-feedback and sample-import refinement over CP03.

## Transport presentation

- `PLAY PATT` is shortened to `PLAY PAT` so its caption sits comfortably
  inside the same-size bevel as `PLAY SNG`.
- During native Song playback, the `Oxx Pxx` display follows the audible song
  order and pattern instead of remaining on the position where playback began.
- The previous/next order arrows and all five song/deck transport buttons
  briefly use the selected instrument-bank color and inset bevel after a click.
  The flash is visual only and does not delay or alter transport execution.

## Canceling Pattern Q

- Re-click any numbered waiting Q tile to remove that individual entry. Queue
  numbers close up immediately.
- The active Q tile keeps its existing yellow quantized exit when re-clicked
  with no waiting queue. Poly keeps its existing middle-click toggle behavior.
- Canceling a waiting Q entry does not stop the active Q pattern, Poly patterns,
  Sample Q/Poly, or a native Song.

## Folder imports beyond 32 samples

Ctrl+Shift-clicking **Sample** now continues naturally sorted files through
successive Sample Matrix banks instead of truncating at 32. Each group of 16
samples receives one standard XM instrument:

- 1-16 files: one instrument
- 17-32 files: two instruments
- 33-48 files: three instruments across two Matrix banks
- 49-64 files: four instruments across two Matrix banks

The import starts at the Sample bank currently selected on the right side and
continues forward without wrapping. All required instrument destinations are
reserved before decoding or replacing anything. If any affected bank already
contains samples, the confirmation names the complete replacement range.

The Deck Matrix still has eight banks (256 tiles). If a folder exceeds the
remaining capacity from the selected bank through `E0`, Tapehead reports the
limit and asks before importing the portion that fits.
