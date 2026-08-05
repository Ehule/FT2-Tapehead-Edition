# Composition Baker CP04.4

This checkpoint extends the Tapehead-to-XM composition baker with a live
looping-performance mode. Its primary purpose remains flattening a composition
that uses Fast Tracks and Z commands into an ordinary XM that no longer
requires Tapehead's alternate transport engine.

## Using it

1. Stop Song, Q, Poly, and Sample Deck playback.
2. Open Disk Op in module mode.
3. Hold **Shift** and click **Save**.
4. Choose **Fast Bake** or **Live** in the Bake Module window.

The proposed output name receives `-BAKED.xm`. Normal unmodified Save behavior
is unchanged.

## Fast Bake

- One complete song pass, beginning at order 0 / row 0
- Silent CPU-speed replay rather than audio-speed rendering
- Ordinary song transport and tracker cells
- Fast Tracks Pattern and Song modes
- Per-track ratios, direction, clutch, sync, and enable state
- Tapehead Z commands, whose results are flattened while the Z commands
  themselves are removed
- Standard XM instruments and samples
- Conventional 64-row destination patterns and linear order list, with a
  shorter final pattern when Stop occurs between 64-row boundaries

The source module, editor position, channel state, and exact Fast Tracks phase
are restored after the pass.

## Live Bake

Live Bake turns the baker into an armed real-time recorder:

1. Choose **Live** in the Bake Module window.
2. Press **Play Song**.
3. Let the song wrap and loop while changing Fast Tracks ratios, clutch,
   direction, synchronization, and other performance controls.
4. Press the ordinary **Stop** button.

Stop closes the destination timeline and writes the captured performance to the
proposed `-BAKED.xm` path. Every source-song loop continues adding destination
rows, so ten source patterns can become a much longer linear XM containing many
different realized variations of those patterns.

Live Bake is audible and runs at normal playback speed. MIDI Dub remains active
during the performance. Pressing Stop before Play Song cancels the armed bake.

## Lossless-first policy

CP04.3 refuses to write a file if multiple resolved events need the same XM
channel/cell or if a sub-row event cannot be represented by XM's `EDx` note
delay. This is deliberate: the first baker must not appear successful after
silently dropping music.

The dialog reports collision and unsupported sub-row counts. Tapehead Z
commands and already-resolved flow commands are removed after they affect the
private transport; if removing one leaves an empty cell, it is not treated as
a musical sub-row event. Later milestones can translate more genuine musical
cases using spare-channel allocation, finer destination timing, or explicit
effect expansion.

## Not in this milestone

- Deck Matrix or Sample Matrix performance capture
- Spare-channel collision allocation
- Opening the baked module automatically
- Progress bar and cancel-during-bake control
- XM-limit splitting

Those are extensions of the same resolved-event pipeline rather than separate
export systems.
