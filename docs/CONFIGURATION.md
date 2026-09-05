# `tapehead.ini` configuration — CP04.6

Tapehead reads `tapehead.ini` at startup from beside the executable. The source
template lives at [`../release/other/tapehead.ini`](../release/other/tapehead.ini).
Restart after editing values that do not also have a live UI control.

Invalid or missing values fall back to safe built-in defaults.

## TapeSister exchange

```ini
[TapeSister]
ExchangePath=
ExecutablePath=
```

- `ExchangePath` is the shared directory configured as TapeSister's **FT2
  Exchange Path**. Tapehead checks it at startup, about once per second while
  the UI is idle, and when **Check inbox** is requested manually. Sample sends
  and explicit **Render audio → TapeSister** transfers are published into this
  same directory. Ordinary WAV captures never use the exchange inbox.
- `ExecutablePath` is the full path to the TapeSister executable. After
  publishing, Tapehead reuses a live TapeSister detected through the exchange
  folder; otherwise it starts this path directly without a command shell.
  **Publish + New** forces another instance. This key may be blank—publication
  still succeeds and TapeSister can be opened manually.

Both values use 4096-byte configuration storage instead of the legacy
80-character sample-directory fields. On Windows, absolute paths beyond the
ordinary `MAX_PATH` boundary are passed to wide-character file APIs using an
extended path prefix.

Both values can be changed live in **Configuration → Layout**:

- Click **Exchange** or **Program** to edit the scrolling path field directly.
- Double-click **Exchange** to open Tapehead's browser and select the current
  shared folder.
- Double-click **Program** to open the browser, select the TapeSister
  executable, then press **Select**.

Leaving either text field or accepting a browser selection writes the values
back to `tapehead.ini`; no restart is required. Manual edits to `tapehead.ini`
made outside the application are still read on the next launch.

See [`FT2_EXCHANGE.md`](FT2_EXCHANGE.md) for confirmation behavior, the exact
version-1 manifest, and a round-trip test checklist.

The same **Exchange** directory also owns the universal `palette.pal`. Tapehead
loads it at startup and its Layout palette **Load**/**Save** buttons use it.
With no Exchange path, the file lives beside `FT2.CFG`. See
[`UNIVERSAL_PALETTE.md`](UNIVERSAL_PALETTE.md) for legacy fallback and the
complete reciprocal schema.

## Audio captures

```ini
[Capture]
Folder=
```

- `Folder` is the destination for ordinary, application-agnostic WAV renders.
  When blank, Tapehead creates `Captures` beside `FT2.CFG` the first time a
  capture is requested. This keeps Windows portable captures inside the
  extracted Tapehead folder and Linux AppImage captures inside
  `Tapehead-data`.
- Set `Folder` to an absolute path to choose another capture library. Tapehead
  creates the final folder when possible, refuses to overwrite existing WAVs,
  and adds a three-digit sequence number to every filename.
- The capture folder is intentionally separate from the TapeSister exchange
  folder. Only an explicit **TapeSister** destination creates a manifest and
  publishes into that transactional inbox.
- Plain F7 during Block Loop writes live, seam-quantized performance captures
  here as `Song_BlockPerformance_###.wav`. They use the active audio-device
  rate and current WAV bit depth; F8 and dialog renders continue to use the
  WAV renderer's selected format.

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

The presentation layer tracks changed 16×16 logical tiles. Static regions are
kept in the streaming texture; only changed regions are rebuilt by the 2×/3×
filter and uploaded. Large redraws automatically fall back to one full-frame
update, and minimized windows suspend texture work until restored. This is
automatic and does not require another INI setting.

## Pattern LEN and colors

```ini
[Pattern]
PatternColorMode=edit
FastTracksUseTrackLengths=true
TrackLengthControlMax=256
```

LEN is one song/module-wide setting per tracker track. For example, setting
track 1 to 25 makes that lane 25 rows in every Pattern- and Song-mode source.
LEN takes precedence over a shorter source pattern: a 20-row pattern supplies
five safe blank extension rows. A longer 40-row pattern still lets that track
wrap at 25. The longest explicit LEN establishes the extended pattern domain;
`LEN OFF` follows that domain. CONTROL selection is also song-wide and remains
active as playback moves through the order list.

`FastTracksUseTrackLengths=true` makes private FastTracks heads use those LEN
domains. Set it to `false` to make FastTracks ignore LEN while ordinary LEN and
CONTROL behavior remains available.

`TrackLengthControlMax` accepts `1`–`256` and affects only interactive input:
the LEN header mouse wheel and Shift + APC40 Track Control encoders. The
default exposes all 256 rows; a lower ceiling gives the absolute hardware more
precision. Existing module LEN values above the ceiling remain valid until
edited.

The scrollable **Configuration → Layout** palette includes `Wave Select` and
seven transport entries in addition to the pattern-field colors: `LEN Head`, `FT Head`,
`CONTROL Head`, `FT Sync LED`, `FT Phase LED`, `FT Song Badge`, and `FT+LEN
Head`. Saving the configuration writes their corresponding `*Color=#RRGGBB`
keys under `[Pattern]`.

The Layout **Load**/**Save** controls read and write the complete 26-color
shared `palette.pal`. `WaveSelection` drives Tapehead's translucent Sample
Editor range tint; the six remaining TapeSister-only fields are retained.
Nineteen tiny TapeSister swatches form a source-only eyedropper strip beside
Tapehead's 20-color selector. Legacy `tapehead.pal` and
`[TapeheadPalette]` files still load without being rewritten.

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
F8ExtractBlock=true
```

- `DiskOpBackspaceParent`: makes Backspace navigate to the parent folder while
  Disk Op is open.
- `PatternBackspacePullUp`: makes Backspace delete the current note row and
  pull later notes upward.
- `F8ExtractBlock`: defaults to `true`, including when the key is missing or
  malformed. Plain F8 then copies the selected Pattern Editor rectangle to the
  lowest unused, unreferenced pattern slot. Set it to `false` to restore F8's
  legacy duplicate octave-6 behavior. Shift/Ctrl/Alt+F8 continue to transpose
  the current instrument upward in the track/pattern/block, respectively.

Extraction is copy-only: it leaves the source, cursor, selection, clipboard,
song position, and order list unchanged. Rows are rebased to row `00`, selected
tracks keep their channel positions, and all other tracks remain empty. The
new 1–256-row pattern is Matrix-only. Ctrl+Z removes it and Redo recreates the
same pattern number, row count, XM events, and Tuning/Drift data.

## Audio

```ini
[Audio]
Backend=Auto
OutputBuses=1
MonoOutputs=false
```

- `Backend`: Windows audio backend policy. `Auto` is the recommended default
  and lets SDL choose its preferred available backend. `WASAPI` and
  `DirectSound` explicitly select those drivers before SDL initializes audio.
  DirectSound remains available for older-device compatibility. The setting is
  ignored on other platforms.
- `OutputBuses`: number of logical stereo buses, accepted range `1`–`16`.
  Linux's **Tapehead JACK Virtual Outputs** exposes two named ports per bus.
  Multichannel-capable SDL devices can expose the same 2–32 channel layout;
  unsupported devices fold safely to stereo.
- `MonoOutputs`: routes each tracker lane to one physical mono destination
  before FT2 stereo panning. The Config > Audio checkbox can change this mode
  while Tapehead is running.

The Audio output list also contains **TapeSister Live Link**. It is a clocked
shared-memory stereo destination, not a hardware device. While selected,
TapeSister is the sole physical-output owner and Tapehead's logical buses use
the normal stereo fallback. See [`LIVE_LINK.md`](LIVE_LINK.md).

Routing assignments are runtime performance state and are not stored in XM.
See [`MULTICHANNEL_OUTPUT.md`](MULTICHANNEL_OUTPUT.md).

Tapehead reports the active backend, selected device, negotiated sample rate,
format, channel count, and buffer size to standard error and in the
Ctrl+Shift+F diagnostic overlay. If an explicitly selected device cannot open
at startup, Tapehead asks before switching to the system default. It never
silently reroutes an explicit selection. A disconnected output is identified in
the window title and is reopened when the configured device becomes available.

## MIDI performance control

```ini
[MIDI]
PerformanceControl=false
Profile=None
ControlInput=
ControlOutput=
APC40RGBBrightness=100
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

`APC40RGBBrightness` controls only the APC40 mkII's 40 RGB Matrix and upper
column-sequence pads. It accepts integers from `0` (off) through `100` (the
original palette appearance). Values outside that range are clamped, while a
missing or malformed value safely uses `100`. Because the controller exposes
discrete palette variants, adjacent settings can select the same intensity.
Fixed-color track, transport, Scene Launch, and global LEDs are unaffected.

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

The APC Left/Right buttons can replace edit-cursor movement with song-wide LEN
CONTROL selection:

```ini
ControlTrackLeftStart=1
ControlTrackRightStart=8
ControlTrackNavigationWrap=true
```

The two start values accept tracks `1..8` and are used only when CONTROL is
currently off (or outside the APC's eight strips). A shorter module clamps them
to its final active track. CONTROL selection is valid even when that track's
LEN is OFF. When wrapping is disabled, Left/Right stop at the active boundary.

Mapping keys use `Message.MIDIChannel.Number`. MIDI channels, tracker tracks,
Matrix banks, and Matrix slots are one-based in the file. Inputs are `NoteOn`
(or the `Note` alias) and `CC`.

| Note action | Argument |
| --- | --- |
| `TrackSelect`, `TrackMuteToggle`, `TrackPerformanceMuteToggle` | Tracker track `1..32` |
| `PerformanceUnmuteAll`, `PerformanceUnmuteNext`, `PerformanceMutePrevious`, `UnmuteAll` | None |
| `FastTrackToggle`, `FastTrackRatioNext`, `FastTrackRatioPrevious`, `FastTrackRatioReset`, `FastTrackReverseToggle`, `FastTrackClutchToggle` | Tracker track `1..32` |
| `FastTrackMasterToggle`, `FastTrackGlobalModeToggle`, `FastTrackResetAll` | None |
| `TrackLengthControlPrevious`, `TrackLengthControlNext` | None |
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
While `ShiftModifier` is held, a `FastTrackRatio:n` CC instead maps zero to
`LEN OFF` and `1..127` across `1..TrackLengthControlMax` for track `n`.
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
In Toggle mode, the pedal and Shift+Space share one latch, so either control can
unfreeze the other. Plain Space clears the latch as it stops playback, and a
new playback starts unfrozen. Hold mode remains momentary while the pedal is
physically depressed.

Note On with velocity zero is treated as Note Off. Button actions run on the
press edge only, so a release cannot toggle a track a second time. CC values
map linearly from `0..127` to the configured Tapehead trim ceiling. Repeated
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
Set `TrackTrimMaxPercent` in `[MIDI]` to an integer from `100` through `200`.
`100` caps every per-track trim at unity (256), `150` caps it at 384, and
`200` preserves the full boost range (512). Missing or malformed values use
`200`; numerical values outside the range are clamped. APC track faders use
their complete physical travel to reach the configured ceiling, and scope
mouse-wheel trim uses the same limit. This does not affect the master fader,
which always stops at unity (256).

Each visible tracker scope includes a fixed 0–200% trim-position strip with a
unity notch. It displays stored channel trim—not live audio amplitude or a
clipping measurement. Red denotes above-unity boost/headroom in use.

Set `TrackTrimDisplayWidth` in `[MIDI]` to choose the strip width in logical
pixels. Values `1`–`8` resize its background, colored lines, and unity notch;
`0` hides the indicator. Values outside `0`–`8` are clamped, while a missing
or malformed value retains the default width of `2`. Logical pixels preserve
the same proportions under normal and HD scaling.
