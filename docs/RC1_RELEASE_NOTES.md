# FT2 Tapehead Edition RC1

> **Frozen historical baseline.** RC1 remains a valid recovery point, but the
> current documented experimental checkpoint is CP04.6. See
> [`README.md`](README.md) for the current documentation map.

RC1 freezes Structural Checkpoint 01L as the first release-candidate baseline
on `workmode-structural-refactor`.

## What defines this release

- FasTracks is separated into a named 32-channel transport subsystem with
  rational ratios, Pattern/Song traversal, reverse, phase, sync, and clutch.
- Pattern Matrix transport and drawing are isolated from the stock replayer.
- Global `Z20-Z2B` commands apply atomically after the complete channel pass.
- `Shift+INP` creates an independent copy of the current pattern and length.
- Pattern creation, order navigation, Pattern Play, and normal Transpose share
  one immediate pattern context instead of waiting for a later FT2 sync event.
- The Transpose panel includes VIEW, a deliberate rendered-window editing mode.
- Silent Record uses `Ctrl+Grave`, and keyboard/mouse modifiers are captured
  from the actual input gesture rather than a stale frame snapshot.
- Pattern Matrix colors update immediately as pattern data and song membership
  change.

## VIEW Transpose

With VIEW off, Track, Pattern, Song, and Block retain their complete-data
Transpose scopes.

With VIEW on, a Transpose click freezes the literal event slots currently
rendered in the pattern editor:

- Track addresses the cursor track.
- Pattern and Song address all horizontally visible tracks.
- Block intersects the current block with the visible canvas.
- FasTracks Pattern heads contribute their private rows.
- FasTracks Song heads contribute both their private rows and private patterns.
- Standard and clutched tracks contribute the master source.
- Wrapped appearances of one underlying event are deduplicated.

At high ratios—especially `5:1`—the editor behaves like a spinning canvas.
Adjacent notes can be sampled from different private-head moments. A pair that
began in unison may therefore be split, with one note captured by VIEW and the
other left untouched. This is intentional. The Transpose operation still
applies exactly once to every event in its immutable snapshot.

## FT2 idiosyncrasies preserved

FasTracks makes FT2's distinction between what is heard, what is drawn, and what
is edited visible. RC1 keeps those identities synchronized for ordinary
navigation and editing, but preserves their deliberate separation for private
transports and VIEW.

Other preserved behaviors include:

- Current Instrument Transpose ignores notes whose instrument column is `00`;
  All Instruments Transpose includes them.
- Reverse changes traversal, not stored pattern data.
- In reverse, note-off placement can produce drones because events are still
  processed forward in time as the private source position moves backward.
- Song-mode FasTracks heads freewheel through their private order positions
  while Pattern Matrix performs the master layer.
- Global effects and Tapehead `Zxx` controls can still execute from muted
  channels, following FT2's separation of voice muting and global control data.
- Tapehead performance mute silences only FT2's mixer output. MIDI Dub remains
  active, allowing the tracker to sequence an external destination such as
  SunVox without layering FT2's samples. Stock channel mute stops normal
  channel processing and further MIDI note triggers as well as internal sample
  playback. Use MIDI Panic if an external instrument is already sustaining a
  note that must be stopped immediately.

These are not treated as automatic defects. They are part of the mechanical,
performance-oriented model unless a reproducible state-integrity failure makes
the displayed command and the active transport disagree.

## RC1 validation

The candidate passed:

- the deterministic FasTracks transport suite;
- the native C rational-clock suite;
- the focused 119-check runtime audit for pattern-context, reverse, and VIEW
  behavior;
- strict source compilation and a headless Linux startup smoke test;
- runtime asset, archive-path, executable-mode, Git, and macOS symlink checks;
- manual composition testing of Melodic Walk, repeated `Shift+INP`, normal and
  VIEW Transpose, Pattern/Song transport, reverse, and extreme ratios.

The Linux executable included in the RC1 source package was rebuilt from the
candidate source. Windows and macOS binaries should be built from the RC1 tag
on their native toolchains.

## Compatibility

FasTracks state remains runtime-only and does not change the XM file format.
Stock FT2-compatible players preserve the XM pattern data and `Zxx` effect
bytes but ignore Tapehead's private transport interpretation.
