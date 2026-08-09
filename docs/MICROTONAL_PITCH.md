# Microtonal Tune and Microtonal Drift

Tapehead Edition reserves two previously unused XM effect letters for
per-channel microtonal playback state:

| Command | Meaning |
|---|---|
| `Mxx` | Persistent MicroTune center, one cent per hexadecimal step around `80` |
| `Nxx` | Persistent MicroDrift depth, approximately +/- one cent per hexadecimal step |

Examples: `M80` is nominal tuning, `M87` is +7 cents, `M79` is -7 cents,
`M00` is -128 cents, and `MFF` is +127 cents. `N00` disables drift and returns
the wandering component to zero; `N04` enables approximately +/-4 cents of
drift around the current `Mxx` center. The maximum drift depth is +/-255 cents,
so the maximum combined displacement is -383 through +382 cents.

## Effect-namespace audit

The replayer recognizes the standard XM effects below. Tapehead does not
reinterpret any of them.

| Commands | Existing meaning |
|---|---|
| `0`-`9` | Arpeggio; pitch slides; tone portamento; vibrato and combination effects; tremolo; panning; sample offset |
| `A`-`H` | Volume slide; position jump; volume; pattern break; extended effects; speed/BPM; global volume; global-volume slide |
| `K`, `L` | Key off; envelope position |
| `P`, `R`, `T` | Panning slide; multi-retrigger; tremor |
| `X1x`, `X2x` | Extra-fine portamento up/down |
| `Zxx` | Tapehead FastTracks command family |

The implemented extended effects are `E1x/E2x` fine pitch slide,
`E3x` glissando control, `E4x` vibrato waveform, `E5x` sample finetune,
`E6x` pattern loop, `E7x` tremolo waveform, `E9x` retrigger,
`EAx/EBx` fine volume slide, `ECx` note cut, `EDx` note delay, and `EEx`
pattern delay. `E0x`, `E8x`, and `EFx` are no-ops in this replayer.

Before this feature, `I`, `J`, `M`, `N`, `O`, `Q`, `S`, `U`, `V`, `W`, and
`Y` were unused no-op letters. `M` and `N` were selected because they are
adjacent, mnemonic, and do not collide with the standard namespace or the
compatibility-frozen `Zxx` map. `Dxx` remains Pattern Break and `Txx` remains
Tremor.

An effect value from `0` through `Z` that has no handler remains a no-op in
Tapehead. Values above `Z` are sanitized to no effect when a module is loaded.
Other XM players may ignore or remove `Mxx`/`Nxx`; resaving in another tracker
may lose them.

## Pitch and state architecture

Each of the 32 tracker channels owns independent MicroTune, MicroDrift,
target, interpolation time, and pseudo-random state. The ordinary FT2 engine
continues to calculate note periods, pitch slides, tone portamento, vibrato,
arpeggio, instrument auto-vibrato, and MIDI pitch first. Tapehead then scales
the final mixer step by:

`2 ^ ((MicroTune cents + MicroDrift cents) / 1200)`

The adjustment does not rewrite samples or feed a modified period back into
FT2's effect state. A channel tuned -11 cents therefore keeps ordinary vibrato
and portamento centered on its detuned pitch without cumulative rounding.
MicroTune persists across rows and subsequent note triggers. It resets only
when another `Mxx` is encountered or the channel/replayer is reset; `M80` is
the explicit nominal reset.

MicroDrift selects independent deterministic targets inside the requested
depth and interpolates continuously between them. Each target takes 6-14
seconds. Timing is derived from the tracker tick duration and BPM rather than
the audio sample rate. The independent fixed seeds make voices diverge from
one another while making a playback reproducible. The state layout keeps
depth and timing separate so a later drift-rate command can be added without
changing `Nxx`'s cent resolution.

## Strumming

Both APC40 cue-encoder and crossfader strumming converge on the same row
audition path. That path now applies `Mxx`/`Nxx` before triggering a note and
also applies effect-only rows to an already ringing channel. The normal audio
tick continues while Song/Pattern transport is stopped, so looped and
ping-pong strummed samples keep drifting after the gesture has passed their
row. Different tracks retain independent tuning and drift.

This does not add a transport dependency to manual strumming. If Pattern Jog
Audition is configured Off, the jog still does not create or alter audition
voices.

`PatternJogFastTracks=Ignore` excludes private-head channels from both the
audible strum and Live Bake capture. `Include` applies and captures `Mxx/Nxx`
on those channels through the same ordinary-row strum path without changing
their FastTracks assignments.

## Composition Baker and compatibility output

Standard XM has no faithful representation for a persistent one-cent offset
or for continuous sub-semitone wandering. Composition Baker therefore offers
two explicit targets. **Standard XM** strips `Mxx` and `Nxx` while preserving
any note, instrument, volume, or volume-column data in the same cell.
**Tapehead XM** retains the commands in the flattened pattern stream for
reopening in Tapehead Edition. Both targets remain `.xm` files; Tapehead output
does not need a separate container merely to preserve pattern effects.

The completion message reports how many microtonal commands were stripped or
preserved. Standard output never leaves these unknown commands for a different
tracker to interpret.

This first implementation does not approximate MicroTune or MicroDrift with
standard pitch-slide commands: that would be quantized, tempo-sensitive, and
unable to share one XM effect column reliably with the source music. A future
specialized pitch-render pass would be a separate compatibility feature.

## Editor note

The hexadecimal command is the canonical UI. The current pattern editor has
no lightweight cursor-status line for effect descriptions, so this focused
patch does not add hover text or rewrite the editor layout.
