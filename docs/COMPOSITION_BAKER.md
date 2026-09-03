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

## Performance-capture contract

**What you hear is what you bake, other than fader data.** **The faders remain
the live mix.** Structural performance is flattened; track trim, the master
fader, above-unity boost, and their movements never create XM volume commands,
scaled samples, or automation. Record the audio output when the exact live
fader mix, boost, or resulting distortion must be preserved.

Ordinary mute and Performance Mute are combined into one effective audible
state. Performance Solo, mute-all, reveal, and APC40 actions use those same
states. A transition to silence writes a standard XM note cut; releasing only
one overlapping mute cause does not reveal the track, resurrect its cut note,
or replay events missed while muted. Tracker volume cells, sample-launch
volume, envelopes, and other non-fader musical volume remain ordinary music.

Pattern Matrix Q and Poly streams pass through the resolved replayer path.
Their launches, queued replacements, handoffs, automatic advances, and stops
therefore become the notes, instruments, volume cells, effects, and cuts they
actually produce—not Tapehead tile commands. Independent streams share the
existing collision/spill allocator.

Sample Matrix launches become standard note/instrument events. The automatic
bank-to-instrument mapping is reused, including its C-4-up sample note map.
Q/Poly replacement and stop actions become starts and note cuts, and active
voices are captured at Live Bake start. Empty tiles do nothing. A populated
tile whose mapping is unavailable is omitted without interrupting capture;
the completion accounting distinguishes unavailable positions from launches
actually skipped. Conversion work that can be represented by the existing
mapping is not an error.

Sample Morph is resolved per triggered note, after its independent per-track
selection, so the baked XM uses the exact sample heard instead of blindly
copying the source cell's original sample choice. The original instrument is
reused only when its note map selects that sample at the note that was actually
played. A mapping at some other note is not used as a reason to transpose the
baked note.

When the selected sample is not faithfully reachable at that note, Baker adds
a destination-only private instrument containing an owned copy of the selected
sample and the source instrument's XM envelopes, fadeout, and vibrato settings.
The note remains unchanged, and the copied sample retains its PCM, loops,
volume, panning, relative note, finetune, and bit-depth flags. Compatible
private instruments are deduplicated when a selection recurs. They exist only
in the destination XM: Baker never changes the source instrument, its note map,
or its samples, and cancellation or failure discards all private assets.
Baker-only Sample Morph instruments inherit the selected sample's name, with a
generated source instrument/sample fallback when that name is empty.

Encoder motion itself is not stored as automation and does not retrigger a
voice; it changes only later note triggers. Track selections remain independent
and their current state is honored when either Fast or Live Bake begins.

## Before baking

Stop ordinary Song/Pattern playback before Fast Bake. Deck activity is not a
blanket veto. Live Bake may be armed while Pattern Q/Poly or Sample Q/Poly is
already active and records that starting state.

Keep the source XM. The baker writes a separate file and does not turn the
loaded composition into the baked result.

## Open the Bake Module window

1. Open Disk Op in **Module** mode.
2. Choose the destination directory and filename.
3. Hold **Shift** and click **Save**.
4. Choose **Fast Bake** or **Live**.
5. Choose **Standard XM**, **Tapehead XM**, or **Adaptive XM**.

Standard output receives `-BAKED.xm`. Tapehead output receives
`-BAKED-TAPEHEAD.xm`. Experimental Adaptive output receives
`-BAKED-ADAPTIVE.xm`. Normal Save behavior is unchanged when Shift is not held.

**Adaptive XM** is the separate experimental timing path. It preserves the
Tapehead M/N extension, keeps every event-bearing canonical tick at TPL 1, and
compresses only completely empty tick spans with ordinary XM `F01`–`F1F`
commands. Those synthesized clock commands occupy a dedicated final timing
track, leaving the musical tracks' effect columns free. It never replaces or
changes the existing Standard XM or Tapehead XM paths. Its final pattern uses
the exact captured row count instead of adding a padded tail. FT2 normally
rewrites all-zero XM patterns to 64 rows, so a repeated current TPL command
anchors each otherwise empty adaptive pattern on that same timing track in the
serialized XM without changing its timing.

The window has a compact **Pattern Rows** cycling control with 16, 32, 64, 128, and 256-row choices. Standard XM and Tapehead XM give every generated pattern, including the final one, that length, making each pattern a predictable Deck Matrix loop unit. Adaptive XM uses that length as its pattern maximum but shortens its final pattern to the exact captured endpoint. The nearby approximate Pattern and Maximum durations use the BPM active when the dialog opens; timing commands encountered later remain part of the baked performance. The selection is retained in `tapehead.ini`, with 256 as the compatibility-safe default.

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

### Tick-resolution output

Composition Baker intentionally always uses tick resolution. Every actual
source replayer tick becomes one destination XM row, regardless of whether the
source begins at TPL 1, 3, 6, 12, or changes TPL during playback.
This lets fractional, mixed, and fast ratios—including `5:1`—cross several
private rows without forcing those events into one master-row cell.

Standard XM and Tapehead XM output use **TPL 1**. Source `F01`–`F1F` timing commands have
already shaped the expanded timeline and are removed. BPM commands remain
ordinary XM data.

Adaptive XM begins at TPL 1, preserves those same canonical event ticks at TPL
1, and combines only empty spans using TPL values up to 31. A one-tick empty
reset row restores TPL 1 before every later event row, so continuous effects,
retriggers, BPM changes, strumming, and M/N data retain their canonical Baker
behavior. The timing track is appended after channel compaction. In the XM
limit case where all 32 compacted channels contain music, Adaptive XM retains
all source data and places the clock commands on otherwise empty rows of
channel 1 because a 33rd XM channel cannot be represented.

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
- Linear, unique destination patterns of the selected 16, 32, 64, 128, or 256 rows
- At most 256 order positions and baked patterns: selected rows × 256 captured ticks (4,096 through 65,536)
- Maximum wall-clock duration depends on BPM; at a constant 125 BPM the limit
  is approximately 21:51 (it is not 21:51 at every tempo)
- Original BPM at the beginning; resolved BPM changes remain in the pattern
- TPL 1 for tick-resolution bakes
- Smallest compacted musical channel count; Adaptive XM normally appends one
  dedicated timing track
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
silently discarding music. Capacity overflow refuses to save: it never truncates,
wraps, or overwrites an earlier row. Standard XM and Tapehead XM outputs are
both ordinary `.xm` files.

## Capture boundary

The Baker captures the resolved Song, FastTracks, Pattern Matrix, Sample
Matrix, Sample Morph, mute, and solo event stream. It does not bake
multichannel hardware routing, track/master faders, or mixer gain automation.
The result remains a standard XM event performance rather than an audio render.

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
