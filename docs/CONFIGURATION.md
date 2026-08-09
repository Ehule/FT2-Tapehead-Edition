# `tapehead.ini` configuration — CP04.6

Tapehead reads `tapehead.ini` at startup from beside the executable. The source
template lives at [`../release/other/tapehead.ini`](../release/other/tapehead.ini).
Restart after editing values that do not also have a live UI control.

Invalid or missing values fall back to safe built-in defaults.

## Video

```ini
[Video]
HDMode=false
HDScale=3
HDStyle=crisp
```

- `HDMode`: enables the experimental high-resolution presentation layer.
- `HDScale`: `2` or `3`; the window is reduced safely if the display cannot fit.
- `HDStyle`: `crisp`, `sharp`, or `round`. `sharp` is accepted as an alias for
  `crisp`; `round` restores the softer earlier filter.

The logical FT2 layout and mouse coordinates remain 632×400.

## Launcher

```ini
[Launcher]
Enabled=true
Standalone=true
```

- `Enabled`: enables the combined Pattern/Sample launcher.
- `Standalone`: opens the dedicated full-window Deck Matrix with both decks
  visible. When false, the compact switchable launcher surface is used.

## Keyboard

```ini
[Keyboard]
DiskOpBackspaceParent=false
PatternBackspacePullUp=false
```

- `DiskOpBackspaceParent`: makes Backspace navigate to the parent folder while
  Disk Op is open.
- `PatternBackspacePullUp`: makes Backspace delete the current note row and
  pull later notes upward.

## Audio

```ini
[Audio]
OutputBuses=1
MonoOutputs=false
```

- `OutputBuses`: number of logical stereo buses, accepted range `1`–`16`.
  Linux's **Tapehead JACK Virtual Outputs** exposes two named ports per bus.
  Multichannel-capable SDL devices can expose the same 2–32 channel layout;
  unsupported devices fold safely to stereo.
- `MonoOutputs`: routes each tracker lane to one physical mono destination
  before FT2 stereo panning. The Config > Audio checkbox can change this mode
  while Tapehead is running.

Routing assignments are runtime performance state and are not stored in XM.
See [`MULTICHANNEL_OUTPUT.md`](MULTICHANNEL_OUTPUT.md).

## MIDI performance control

```ini
[MIDI]
PerformanceControl=false
Profile=None
ControlInput=
ControlOutput=
PatternJogAudition=Latched
PatternJogFastTracks=Ignore

[MIDI_MAP]
NoteOn.1.48=TrackPerformanceMuteToggle:1
NoteOn.1.49=PerformanceUnmuteNext
NoteOn.1.50=PerformanceMutePrevious
NoteOn.1.51=MatrixModeToggle
NoteOn.1.52=MatrixBankNext
NoteOn.1.53=MatrixSlotTrigger:1
CC.1.7=TrackTrim:1
CC.1.48=FastTrackRatio:1
```

`PerformanceControl` enables the generic controller map. `ControlInput` and
`ControlOutput` select dedicated RtMidi ports for the surface. They do not
replace the musical keyboard chosen in Config > MIDI Input, and the output is
separate from MIDI Dub. This allows a keyboard and control surface to operate
at the same time.

Device selection is name-based rather than port-number-based. An exact
case-insensitive name is preferred; a unique partial name such as
`APC40 mkII` is also accepted so volatile ALSA client/port-number suffixes do
not need to be stored. An empty, missing, or ambiguous name leaves only that
surface port closed. It does not disable the keyboard, MIDI Dub, or the other
surface direction.

`Profile=None` keeps the hardware-neutral map. `Profile=APC40MK2` adds the
built-in Akai layout for any inputs not overridden in `[MIDI_MAP]`, sends the
official Alternate Ableton/Mode 2 introduction message, and enables state-led
feedback. See [`APC40_MK2.md`](APC40_MK2.md) for the complete physical map.

`PatternJogAudition=Latched` makes a CC mapped to `PatternJogRelative` or
`PatternJogAbsolute` sustain notes encountered while ordinary playback is
stopped until the pattern supplies a note-off or Shift + Stop All Clips
hard-silences the jog voices.
`Momentary` is a forward one-shot regardless of strum direction.
`ManualPingPong` plays forward or backward one-shots according to the gesture.
Both ignore sample loop flags and decay to the natural sample boundary. `Off`
moves the row without sound.

`PatternJogFastTracks=Ignore` preserves the original isolated-strum effect:
channels assigned to a FastTracks private transport are neither auditioned nor
captured by cue-encoder/crossfader Live Bake gestures. `Include` makes both
controls read the visible ordinary row on those channels too. It does not
change their FastTracks mode; their private heads resume on subsequent
FastTracks events. The same setting governs what Live Bake records, so the
Tapehead bake matches the strummed performance.

Mapping keys use `Message.MIDIChannel.Number`. MIDI channels, tracker tracks,
Matrix banks, and Matrix slots are one-based in the file. Inputs are `NoteOn`
(or the `Note` alias) and `CC`.

| Note action | Argument |
| --- | --- |
| `TrackSelect`, `TrackMuteToggle`, `TrackPerformanceMuteToggle` | Tracker track `1..32` |
| `PerformanceUnmuteAll`, `PerformanceUnmuteNext`, `PerformanceMutePrevious`, `UnmuteAll` | None |
| `FastTrackToggle`, `FastTrackRatioNext`, `FastTrackRatioPrevious`, `FastTrackRatioReset`, `FastTrackReverseToggle`, `FastTrackClutchToggle` | Tracker track `1..32` |
| `FastTrackMasterToggle`, `FastTrackGlobalModeToggle`, `FastTrackResetAll` | None |
| `MatrixModePattern`, `MatrixModeSample`, `MatrixModeToggle` | None |
| `MatrixBankSelect`, `MatrixLayerBankSelect` | Bank `1..8`; layered form uses Pattern normally and Sample while Shift is held |
| `MatrixBankNext`, `MatrixBankPrevious` | None |
| `MatrixSlotTrigger` | Local slot `1..32` |
| `MatrixSequenceRow`, `MatrixSequenceColumn` | Row `1..4` / column `1..8` |
| `MatrixSequenceBank` | None |
| `TransportPlaySong`, `TransportPlayPattern`, `TransportPlaySongToggle`, `TransportPlayPatternToggle`, `TransportStop`, `TransportStopSong`, `TransportStopDeck`, `TransportStopAll` | None |

CC actions are `TrackTrim:1..32`, `FastTrackRatio:1..32`, `TempoRelative`,
`PatternJogRelative`, `PatternJogAbsolute`, `MatrixMasterVolume`, and
`MatrixCrossfader`, plus the binary `TransportPunch`. Trim maps
`0..127` to `0..200%`. FastTrack ratio maps the same CC range across the 17
musical ratios from `1/2` through `5/1`, including the centered `1/1` value.
Matrix Master maps `0..127` to silence..unity for Q and Poly together.
Crossfader A isolates Q, the center keeps Q and Poly at unity, and B isolates
Poly; short audio ramps smooth both controls.

Transport Punch is configured independently:

```ini
TransportFreezeAudio=Sustain
TransportFreezePedalMode=Toggle
TransportFreezeNavigation=Silent
TransportFreezeResume=Next
```

`Sustain`/`Cut` chooses whether voices survive the punch-out; `Toggle`/`Hold`
chooses the pedal gesture; `Silent`/`Audition` chooses whether frozen song-order
jumps sound row `00`. `Next` continues after the row that was already heard
when the pedal froze time; `Retrigger` deliberately strikes that row again on
resume. A later silent relocation always makes its destination pending, while
an auditioned or strummed destination is consumed and resumes on the next row.

Note On with velocity zero is treated as Note Off. Button actions run on the
press edge only, so a release cannot toggle a track a second time. CC values
map linearly from `0..127` to Tapehead trim `0..512` (`0..200%`). Repeated
pending absolute CC values are coalesced before the main thread applies them.
Relative Tempo and both Pattern Jog message types are never coalesced, so every
encoder detent and crossfader position reaches the action queue.

Pattern and Sample Matrix banks remain independent. Matrix mode selects which
system receives bank and slot actions; switching back restores that system's
previous bank. `PerformanceUnmuteNext` scans low-to-high and skips ordinary
mutes. `PerformanceMutePrevious` reverses only the reveal-next history.

Only messages arriving through `ControlInput` are considered by the map.
Unmapped surface messages are ignored and never become tracker notes. Musical
keyboard messages continue through FT2's normal note-entry path and cannot
accidentally trigger mapped controller actions.

The generic map remains hardware-neutral. The APC profile is a thin default
mapping and feedback layer over these same Tapehead actions.

## MIDI Dub

```ini
[MIDIDub]
Track01=1
...
Track16=16
Track17=1
...
Track32=16
```

Each track value is an outgoing MIDI channel from `1` through `16`. Tracks may
share a channel. The default maps tracks 1–16 to channels 1–16 and repeats that
map for tracks 17–32. The mapping is used for note-on, note-off, timed gate
release, and MIDI panic.

## Undo

```ini
[Undo]
UndoMemoryMB=32
```

Accepted range: `4`–`1024` MB. Memory is allocated only when history entries
are recorded; the ceiling is not reserved at startup. Tapehead retains up to
128 transactions and evicts the oldest entries first when the memory ceiling
is reached.

## Hardware-neutral baseline

The checked-in template intentionally uses one stereo bus, disables Mono
Outputs and HD mode, and contains no machine-specific audio or MIDI device
selection. Files such as `audiodev.ini` and `mididev.ini` are local runtime
state and should not be treated as portable project configuration.
