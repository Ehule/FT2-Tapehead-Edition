# FT2 Tapehead Edition

> **Compose like a tracker. Think like a tape machine.**

FT2 Tapehead Edition is a performance- and composition-oriented fork of
[ft2-clone](https://github.com/8bitbubsy/ft2-clone), which recreates the
FastTracker II workflow. Tapehead keeps the familiar tracker at its center and
adds alternate per-track transports, live pattern and sample decks, MIDI and
multichannel routing, non-destructive performance controls, and tools for
turning performed timing back into an ordinary XM.

## Project status

- Current documented checkpoint: **CP04.6**
- Git branch: **`baker-experimental`**
- Frozen tag: **`cp04.6`**

CP04.6 is the first proven tick-resolution Composition Baker checkpoint. Its
Fast Bake and Live Bake paths have produced conventional XM files from
synchronized, mixed-ratio, high-ratio, and deliberately pathological Fast
Tracks performances. The checkpoint also contains the full Deck Matrix and
Sample Matrix Editor developed after RC1.

RC1 remains the older frozen baseline. Documents bearing RC1, structural
checkpoint, launcher checkpoint, Deck Matrix CP, or multichannel pass numbers
describe the history of a subsystem at that milestone; they are not the best
place to learn the complete current build.

Start here:

- [Documentation index](docs/README.md)
- [Complete feature overview](FEATURES.md)
- [Composition Baker](docs/COMPOSITION_BAKER.md)
- [Deck Matrix](docs/DECK_MATRIX.md)
- [FasTracks and the frozen `Zxx` map](docs/FAST_TRACKS.md)
- [Configuration reference](docs/CONFIGURATION.md)
- [Changelog](CHANGELOG.md)

## What makes Tapehead different

### FasTracks

Every XM channel can have a private Pattern or Song transport while all tracks
continue to share the module's BPM and tick clock. Tracks can run at rational
ratios from `1:2` through `5:1`, reverse, freewheel, synchronize, and use
momentary or latched clutch behavior. The XM `Zxx` effect carries a frozen set
of pattern-programmable FasTracks commands.

### Deck Matrix

The Deck Matrix places Pattern and Sample decks side by side over the same live
module. Pattern Q and four Pattern Poly spools can coexist with Sample Q and
four Sample Poly voices. The Sample side provides eight banks of 32 tiles and
a visual editor that can import disk files or assign existing module samples
without duplicating their audio.

### Composition Baker

Hold **Shift** while clicking module **Save** to flatten a Tapehead performance
into a conventional XM:

- **Fast Bake** silently resolves one complete song pass.
- **Live** records repeated song loops while FasTracks controls are performed
  in real time, then finishes when ordinary **Stop** is pressed.

When FasTracks timing is present, CP04.6 expands each replayer tick into an XM
row and saves at TPL 1. A 32-channel allocator preserves simultaneous events,
uses spill channels only when needed, optionally merges exact duplicate voices,
and compacts the final channel layout. Tapehead control commands are removed
after their musical result has been realized.

### Tracker workflow

Tapehead also adds Silent Record, `REC+`, Inherit Pattern Length, Insert New
Pattern, independent pattern duplication, module-wide Undo/Redo, Sample Map,
sample extraction shortcuts, previewable interpolation and Melodic Walk,
Pattern Matrix navigation, VIEW Transpose, per-track trim and performance mute,
configurable MIDI Dub routing, and native JACK/PipeWire-JACK output buses.

## First run

The example configuration is [`release/other/tapehead.ini`](release/other/tapehead.ini).
It is read at startup from beside the executable.

Important defaults in this checkpoint:

- Deck Matrix enabled and opened as the full-window standalone surface
- HD presentation disabled
- one stereo output bus
- mono output disabled
- MIDI tracks 1–16 mapped to channels 1–16, repeated for tracks 17–32
- 32 MB Undo history ceiling

Select **TRACKER** in Deck Matrix to return to the ordinary editor. See the
[Deck Matrix guide](docs/DECK_MATRIX.md) for its mouse gestures and transport
boundaries.

## Building and testing

Platform-specific compilation instructions remain in
[`HOW-TO-COMPILE.txt`](HOW-TO-COMPILE.txt). Linux convenience scripts are
provided at the repository root.

Run every standalone native regression suite with:

```bash
python3 scripts/test_all_native.py
```

At CP04.6 the complete suite covers the baker allocator and timeline, Sample
Launcher state and banks, Sample Matrix browser, FasTracks core and transport,
multichannel delivery, JACK backend, Pattern/Poly ownership, and MIDI Dub
configuration.

## XM compatibility

Tapehead continues to edit and save standard XM module data. Most Tapehead
performance state is runtime-only, and ordinary XM players ignore the custom
meaning of `Zxx`. The Sample Matrix can append a small Tapehead metadata block
for tile references; compatible XM software still reads the standard module
payload.

Use the Composition Baker when a conventional player must reproduce resolved
FasTracks timing. Baking does not currently capture Deck Matrix or Sample
Matrix performance.

## Philosophy

The goal is not to redesign FastTracker II. The goal is to keep its speed,
look, and directness while making room for nonlinear timing, performance, and
composition methods that still feel mechanically connected to a tracker.

Every major Tapehead system is intended to remain optional. With the new
layers idle, the program should still feel like FT2. With them active, it can
behave like a collection of tape heads, switchers, clocks, and live routing
paths sharing one old machine.

## Credits

- Original FastTracker II by Triton Productions
- ft2-clone by 8bitbubsy
- Tapehead Edition concept, project direction, and testing by Ehule
- Development, debugging, regression work, and documentation assistance with
  OpenAI ChatGPT

FT2 Tapehead Edition inherits the upstream licensing terms included in
[`LICENSE`](LICENSE) and [`LICENSES.txt`](LICENSES.txt).
