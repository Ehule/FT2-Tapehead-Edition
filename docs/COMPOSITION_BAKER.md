# Composition Baker — CP04.6

The Composition Baker translates resolved Tapehead timing into a conventional
XM. It runs the real replayer, follows private FasTracks heads and
pattern-programmed transport changes, writes what those heads actually
encountered, and removes the Tapehead control instructions after their musical
result has been realized.

Use it when a standard XM player must reproduce a FasTracks composition or
when a live ratio performance should become editable linear pattern data.

After choosing Fast Bake or Live, choose an output target:

- **Standard XM** deliberately removes Tapehead `Mxx` MicroTune and `Nxx`
  MicroDrift because another XM player cannot reproduce their cent-accurate,
  persistent pitch state or continuous drift faithfully. Notes and other
  fields sharing those cells remain.
- **Tapehead XM** preserves `Mxx` and `Nxx` in the flattened pattern data for
  reopening and continued work in Tapehead Edition. It remains an ordinary
  `.xm` file; no new container format is required.

The completion dialog reports how many microtonal commands were stripped or
preserved; see [`MICROTONAL_PITCH.md`](MICROTONAL_PITCH.md).

## Before baking

Stop ordinary Song/Pattern playback and all Pattern Q, Pattern Poly, Sample Q,
and Sample Poly activity. The baker will refuse to start while those transports
own playback state.

Keep the source XM. The baker writes a separate file and does not turn the
loaded composition into the baked result.

## Open the Bake Module window

1. Open Disk Op in **Module** mode.
2. Choose the destination directory and filename.
3. Hold **Shift** and click **Save**.
4. Choose **Fast Bake** or **Live**.
5. Choose **Standard XM** or **Tapehead XM**.

Standard output receives `-BAKED.xm`. Tapehead output receives
`-BAKED-TAPEHEAD.xm`. Normal Save behavior is unchanged when Shift is not held.

The window also contains **Merge exact duplicate voices**. It is enabled by
default and remembers its most recent setting for the current program run.

## Fast Bake

Fast Bake performs one silent, CPU-speed Song pass beginning at order 0, row 0.
It captures:

- ordinary tracker cells and Song traversal;
- FasTracks Pattern and Song transports;
- ratios, private phase, direction, clutch, synchronization, enable state, and
  master state;
- compatible `Zxx` changes encountered during the pass;
- standard XM instruments and samples.

The loaded module's song data, editor position, channel state, and exact
FasTracks runtime state are restored after the pass.

### Row-resolution and tick-resolution output

An ordinary composition with no active FasTracks transport and no `Zxx` source
language keeps the established row-resolution path and its initial TPL.

If any channel is already in a private FasTracks mode, or a `Zxx` command
appears in a pattern used by the song order, Fast Bake chooses tick resolution
before playback begins. Every replayer tick becomes one destination XM row.
This lets fractional, mixed, and fast ratios—including `5:1`—cross several
private rows without forcing those events into one master-row cell.

Tick-resolution output uses **TPL 1**. Source `F01`–`F1F` timing commands have
already shaped the expanded timeline and are removed. BPM commands remain
ordinary XM data.

## Live Bake

Live Bake records an audible performance:

1. Choose **Live** in the Bake Module window.
2. Tapehead returns to the tracker and arms the recorder.
3. Press **Play Song** or **Play Pattern**, or begin moving the cue encoder or
   crossfader without starting transport.
4. Let the source song loop while changing FasTracks ratios, clutch, direction,
   synchronization, modes, and master controls.
5. Press the ordinary **Stop** button to finish and save.

Each source-song loop appends more linear destination rows. A small source can
therefore become a long XM containing distinct performed variations. MIDI Dub
remains active during the audible capture.

Live Bake always uses tick resolution because FasTracks can be enabled or
changed after recording is armed. For transportless Performance Bake,
wall-clock time at the armed BPM determines gesture spacing and preserves
silence. Every row crossed by one fader sweep is retained in order. Pressing
**Stop** before playback or a manual strum cancels the armed bake without
writing a file.

## Collision allocation and duplicate policy

Each source tracker channel initially targets the same-numbered output channel.
If a resolved event reaches an occupied cell, CP04.6:

1. reuses a free spill channel already owned by that logical source stream;
2. otherwise claims an unused channel from XM's 32-channel pool;
3. keeps the stream associated with that spill channel when possible;
4. refuses the bake only when no channel can represent the event.

All five XM cell fields move together. Instruments and samples are referenced,
not duplicated.

With exact-duplicate merging enabled, identical repeated events from one
logical stream can merge at the same instant. Different source tracks are
merged only when their completed baked channel trajectories are identical for
the entire capture. Disable the option when coincident voices must remain
separate for gain, phase, envelope, or intentional-noise reasons.

Before saving, the baker removes empty spill channels, packs used channels into
one contiguous block, and sets the module to the smallest channel count needed
by the result. The completion message reports relocated and merged event
counts.

## Output structure

- Linear order list beginning at pattern 0
- Conventional 64-row destination patterns
- Short final pattern when capture ends between 64-row boundaries
- Original BPM at the beginning; resolved BPM changes remain in the pattern
- TPL 1 for tick-resolution bakes
- Smallest compacted output channel count
- Standard XM instruments and sample payload
- Tapehead output preserves `Mxx` and `Nxx`; Standard output removes them
- Tapehead `Zxx`, resolved `Bxx`/`Dxx`/`E6x` flow instructions, and resolved
  tick-speed commands removed where appropriate
- Standard output omits Tapehead Sample Matrix metadata; Tapehead output keeps
  the current Tapehead metadata alongside the preserved pattern extensions

## Lossless-first failure rules

No XM is written when:

- all 32 channels are occupied at a required instant;
- a row-resolution event occurs at a sub-row time that cannot be represented
  by XM's `EDx` note delay;
- allocation or capture memory fails;
- the result exceeds XM's 256-pattern limit;
- an offline song never reaches its end before the safety limit;
- Live Bake stops without capturing any rows.

The baker reports the relevant collision, sub-row, or size failure instead of
silently discarding music.

## Current capture boundary

CP04.6 bakes the ordinary Song replayer and FasTracks. It does **not** capture:

- Pattern Matrix Q or Poly performance;
- Sample Matrix Q or Poly performance;
- Deck Matrix cross-deck scenes;
- multichannel bus assignments or performance-mixer automation;
- live MIDI input as new tracker events unless that material already enters
  the captured XM event stream.

Those are future inputs to the same resolved-event pipeline, not promises of
the current checkpoint.

## Proven CP04.6 validation

The checkpoint has been validated with synchronized `1:1`, synchronized `5:1`,
mixed ratios, repeated live loops, and a pathological collision-heavy module.
Inspection confirmed that the baked files were distinct performances, used
conventional TPL 1 timelines, stripped obsolete timing controls, retained
their source sample data, and expanded beyond the source channel count when
collisions required spill allocation.

Run the native allocator and timeline test directly with:

```bash
python3 scripts/test_baker_allocator.py
```

Run the integrated acceptance suite with:

```bash
python3 scripts/test_all_native.py
```

The implementation history is preserved in
[`COMPOSITION_BAKER_CP04_4.md`](COMPOSITION_BAKER_CP04_4.md) and
[`COMPOSITION_BAKER_CP04_5.md`](COMPOSITION_BAKER_CP04_5.md).
