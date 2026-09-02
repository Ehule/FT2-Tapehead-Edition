# Pattern Interpolation / Melodic Walk

> **Current CP04.6 workflow.** These operations participate in module Undo/Redo
> when their previews are committed.

Tapehead Edition provides four pattern operations:

- `Ctrl+Shift+V` — interpolate volume-column values.
- `Ctrl+Shift+B` — interpolate compatible effect values (`8xx` panning and `Cxx` volume).
- `Ctrl+Shift+T` — interpolate dedicated-lane `Mxx` microtuning or `Nxx`
  drift-depth values.
- `Ctrl+Shift+M` — paint a scale-based melodic walk from selected anchor notes.

Press `Enter` to commit a preview or `Escape` to cancel it. While a preview is active, `Space` and `Right Ctrl` retain their normal playback functions so the temporary result can be auditioned before committing. The Fast Tracks logo also remains usable, including its Ctrl- and Shift-modified actions.

Tuning interpolation requires matching `M` or matching `N` commands at the
top and bottom of every selected track. Interior Tuning/Drift cells must be
empty. The two-digit values are linearly filled in both directions, while the
ordinary effect column remains untouched. Mixed `M`/`N` endpoints are rejected
because cents offset and drift depth describe different quantities.

## Melodic Walk modes

The selected block may span one or many tracks.

### One anchor in the entire selection

The single seed note is copied to the same row in every selected track. Each track then receives the same scale walk from that row to the bottom of the selection.

### Multiple one-note tracks

The first note found in each populated track becomes that track's seed. Its walk begins on its own row and continues to the bottom of the selection. Empty selected tracks remain empty.

### Two anchors on a track

The first and last notes in that track are preserved as endpoints. Interior notes walk through the selected scale between them. This works for both single-track and multi-track selections.

A track with more than two anchor notes is rejected to avoid overwriting intentional material. Two explicit endpoint instruments must match; an omitted endpoint instrument inherits the other one.

## Scale and direction preview

While the note preview is active, choose a scale:

1. Chromatic
2. Major
3. Natural minor
4. Major pentatonic
5. Minor pentatonic
6. Dorian
7. Mixolydian
8. Freygish (Phrygian dominant / Ahava Rabbah)
9. Harmonic minor
0. Repeat

The first press chooses the scale and previews an ascending open-ended walk. Press the same number again to toggle open-ended walks between ascending and descending. Choosing a different number selects that scale and resets open-ended walks to ascending.

Two-anchor walks follow the direction of their explicit destination note. The endpoint itself is never replaced, even when it is outside the selected scale.

Repeat is a special one-note mode rather than a pitched scale. When the selected block contains exactly one note, that note is copied at the current spacing to every selected track. The source row anchors the rhythm, which extends both upward and downward through the selection.

The current edit-row skip controls spacing. A skip of 1 paints every row; larger values leave intentional rhythmic gaps. Starting note interpolation with a step of 0 leaves the pattern unchanged and displays a non-zero-step warning.

### Live spacing control

While a Melodic Walk preview is active:

- `` ` `` increases the temporary step length by one row.
- **Shift + grave/tilde** decreases the temporary step length by one row.
- Decreasing from step length `1` to `0` cancels the preview and restores the original pattern.

Every spacing change restores the preview snapshot and regenerates the complete Melodic Walk using the currently selected scale and direction. Playback continues uninterrupted. The temporary spacing begins at the current edit-step value but does not modify that global setting. During preview, the edit-step display temporarily shows the Melodic Walk spacing (1–16); committing or canceling restores the normal editor value.
