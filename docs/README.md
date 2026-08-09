# FT2 Tapehead Edition documentation

This index separates current CP04.6 instructions from historical checkpoint
records. Start with the current guides unless you are tracing why a subsystem
was built a particular way.

## Current CP04.6 guides

| Document | Use it for |
|---|---|
| [`../README.md`](../README.md) | Project overview, status, first run, and compatibility |
| [`../FEATURES.md`](../FEATURES.md) | Complete feature inventory and shortcuts |
| [`COMPOSITION_BAKER.md`](COMPOSITION_BAKER.md) | Fast Bake, Live Bake, tick-resolution output, and limits |
| [`DECK_MATRIX.md`](DECK_MATRIX.md) | Deck Matrix quick start and links to the exhaustive command table |
| [`DECK_MATRIX_CP03.md`](DECK_MATRIX_CP03.md) | Maintained detailed Pattern/Sample Deck command reference; filename retained for history |
| [`FAST_TRACKS.md`](FAST_TRACKS.md) | Transport model, ratios, controls, and frozen `Zxx` command map |
| [`MICROTONAL_PITCH.md`](MICROTONAL_PITCH.md) | `Mxx` MicroTune, `Nxx` MicroDrift, strumming, and bake compatibility |
| [`MULTICHANNEL_OUTPUT.md`](MULTICHANNEL_OUTPUT.md) | Stereo buses, JACK/PipeWire-JACK, Mono Outputs, and routing gestures |
| [`CONFIGURATION.md`](CONFIGURATION.md) | Every `tapehead.ini` section and startup behavior |
| [`PATTERN_INTERPOLATION.md`](PATTERN_INTERPOLATION.md) | Interpolation and Melodic Walk previews |
| [`SAMPLE_MAP.md`](SAMPLE_MAP.md) | Mapping sample positions into pattern/row addresses |
| [`../UNDO_IMPLEMENTATION_NOTES.md`](../UNDO_IMPLEMENTATION_NOTES.md) | Undo/Redo coverage, limits, and transaction behavior |
| [`../POLY_MATRIX_POC.md`](../POLY_MATRIX_POC.md) | Pattern Poly routing, ownership, and handoffs |
| [`../CHANGELOG.md`](../CHANGELOG.md) | Reverse-chronological development record through CP04.6 |

## Frozen baselines and checkpoint records

These documents describe the build at the named milestone. Later work may have
superseded their limitations, controls, or planned-next lists.

| Document | Historical scope |
|---|---|
| [`RC1_RELEASE_NOTES.md`](RC1_RELEASE_NOTES.md) | Frozen RC1 baseline |
| [`STRUCTURAL_CHECKPOINT_01.md`](STRUCTURAL_CHECKPOINT_01.md) | RC1 refactor and 01A–01L repairs |
| [`POST_HD_CONSOLIDATION.md`](POST_HD_CONSOLIDATION.md) | Experimental post-HD source consolidation |
| [`LAUNCHER_CHECKPOINT_01.md`](LAUNCHER_CHECKPOINT_01.md) | First combined Pattern/Sample launcher engine |
| [`LAUNCHER_CHECKPOINT_02.md`](LAUNCHER_CHECKPOINT_02.md) | First full-window launcher surface |
| [`LAUNCHER_CHECKPOINT_02_2.md`](LAUNCHER_CHECKPOINT_02_2.md) | Restored configurable MIDI Dub map |
| [`DECK_MATRIX_CP03_1.md`](DECK_MATRIX_CP03_1.md) | Transport feedback and multi-bank folder import delta |
| [`COMPOSITION_BAKER_CP04_4.md`](COMPOSITION_BAKER_CP04_4.md) | Live Bake introduction before CP04.6 timing expansion |
| [`COMPOSITION_BAKER_CP04_5.md`](COMPOSITION_BAKER_CP04_5.md) | 32-channel allocator and duplicate policy |
| [`MULTICHANNEL_PASS1.md`](MULTICHANNEL_PASS1.md) | Initial logical bus mixer and SDL output |
| [`MULTICHANNEL_PASS2.md`](MULTICHANNEL_PASS2.md) | Native JACK destination and setup |
| [`MULTICHANNEL_PASS3.md`](MULTICHANNEL_PASS3.md) | First Bus B repair |
| [`MULTICHANNEL_PASS4_DIAGNOSTIC.md`](MULTICHANNEL_PASS4_DIAGNOSTIC.md) | Temporary JACK diagnostic instrumentation |
| [`MULTICHANNEL_PASS5.md`](MULTICHANNEL_PASS5.md) | Hardware-confirmed live routing repair |

## Small developer note

[`LogoConvert.md`](LogoConvert.md) records the runtime logo replacement
workflow. Platform compilation remains in [`../HOW-TO-COMPILE.txt`](../HOW-TO-COMPILE.txt).

## Regression entry point

From the repository root:

```bash
python3 scripts/test_all_native.py
```

The historical documents sometimes name a smaller focused test. The complete
runner is the acceptance gate for the integrated CP04.6 tree.
