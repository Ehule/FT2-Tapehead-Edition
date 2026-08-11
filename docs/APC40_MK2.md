# APC40 mkII — Tapehead Phase 4.3 key map

Tapehead uses the APC40 mkII in Akai Alternate Ableton Mode (Mode 2). FT2 owns
the feedback LEDs, while the musical MIDI keyboard and MIDI Dub remain
separate devices.

Enable it beside the executable in `tapehead.ini`:

```ini
[MIDI]
PerformanceControl=true
Profile=APC40MK2
ControlInput=APC40 mkII
ControlOutput=APC40 mkII
```

Every physical assignment below is editable in `[APC40MK2_MAP]`. Restart FT2
after editing. `None` safely consumes and disables an input; it is especially
useful if the crossfader is too easy to bump.

## Grid and automatic Q sequences

The lower four rows mirror the onscreen 4×8 Matrix. The upper row performs
vertical Q sequences:

| Physical row | Plain press | INI keys | Raw notes |
| --- | --- | --- | --- |
| Top RGB row | Sequence that column from top to bottom | `GridTopPad01`–`08` | 32–39 |
| Matrix row 1 | Trigger slots 1–8 | `GridSlot01`–`08` | 24–31 |
| Matrix row 2 | Trigger slots 9–16 | `GridSlot09`–`16` | 16–23 |
| Matrix row 3 | Trigger slots 17–24 | `GridSlot17`–`24` | 8–15 |
| Matrix row 4 | Trigger slots 25–32 | `GridSlot25`–`32` | 0–7 |

The physical top Scene Launch button sequences the complete 32-tile bank in
row order. The four buttons below it sequence their visually adjacent Matrix
rows from left to right. Empty cells are skipped. The upper RGB rail stays dim
purple and pulses bright purple while its column sequence is active. The five
fixed-color Scene LEDs stay lit; the active sequence control blinks.

An automatic sequence has its own list instead of enlarging the ordinary
four-entry Q queue. A new row, column, or bank request replaces the remaining
automatic list after the current Q item. Press its active control again to
cancel the remainder. Pressing an individual tile cancels the automatic list
and queued next item before performing the manual Q or Poly action. Poly
activity is never stopped by a Q sequence.

`Session` changes what individual Matrix pads control without stopping
anything already playing:

- Session off: Q input, green/yellow whole-grid palette.
- Session on: Poly input, blue/cyan whole-grid palette.
- The non-selected layer remains visible at reduced emphasis.
- A tile participating in both layers alternates the two state colors.
- Shift + any lower Matrix pad schedules that tile to stop at its next complete
  Q and/or Poly loop boundary. A pending launch is cancelled, an inactive tile
  never launches, and repeated Shift presses cannot cancel the stop request.

## Track strips 1–8

| APC control | Plain press/move | Shift layer | Feedback |
| --- | --- | --- | --- |
| Track Control encoder | Set FastTracks ratio | — | 17 distinct ring positions |
| Track fader | Track trim, 0–configured ceiling | — | Hardware position |
| Record Arm ○ | Toggle that track's FastTracks clutch | Unassigned | On = track clutch engaged |
| Solo | Non-destructive Performance Solo; second press restores the previous Performance Mute arrangement | — | Audible solo track |
| Activator 1–8 | Select Pattern Bank 1–8 | Select Sample Bank 1–8 | Normally shows Pattern bank; while Shift is held shows Sample bank |
| Track Select | Toggle per-track FastTracks Reverse | Toggle per-track FastTracks Pattern/Song Mode | Normally shows Reverse; while Shift is held shows Song Mode |
| Clip Stop | Toggle Performance Mute | — | On = red scope X / muted |
| Crossfader A/B button | Enable/disable FastTracks for that track | — | Dim = selected while master bypassed; bright = active |

Track faders control the complete tracker channel and can boost above unity.
They are deliberately excluded from Composition Baker data, as are the master
fader and mouse-wheel trim changes. A zero fader does not remove structural
events from a bake. **What you hear is what you bake, other than fader data.
The faders remain the live mix.** Record audio to preserve exact fader gain,
above-unity boost, and distortion. Performance Mute, Solo, mute-all, and reveal
are structural controls and are captured as effective silence and XM note cuts.
`TrackTrimMaxPercent=100` makes the top of their physical travel unity;
`TrackTrimMaxPercent=200` preserves the full 0–200% boost range. The complete
fader travel is always remapped to the selected ceiling. The narrow green,
yellow, and red strip in each visible scope is the stored trim position—not a
live amplitude or clipping meter. Its fixed midpoint notch is unity, and red
only means above-unity headroom is in use.
Set `[MIDI]` `TrackTrimDisplayWidth=0` to hide this indicator, or use `1`–`8`
to select its width in logical pixels (default `2`). The output-bus marker
moves left as needed, and normal and HD displays retain equivalent scaling.

The master fader remains independent: its complete range is always 0–256
(silence through unity), and it can never boost above unity.

## Sample Morph and FastTracks block

| APC control | Plain action | Shift action | Feedback |
| --- | --- | --- | --- |
| Device encoders 1–8 | Select a populated sample for that track's instrument | — | Ring shows selected populated sample |
| Device Left / Right | Move all Sample Morph selections −/+ one populated sample | — | On while Sample Morph is armed |
| Device On/Off | Arm/safe Sample Morph | — | On = armed |
| Bank Left / Right | Move every selected FastTracks ratio −/+ one clamped ratio degree | Previous/next bank for the currently selected Matrix | On while FastTracks tracks exist |
| Device Lock | Reset selected FastTracks ratios to 1:1 and resynchronize | — | On while FastTracks tracks exist |
| Clip/Device View | Global FastTracks transmission clutch | — | On = clutch latched |
| Detail View | Global FastTracks master | — | On = FastTracks audible |

Sample Morph starts safe after launch and XM load. Empty sample slots are
skipped, one-sample instruments cannot change, and only the next note event
uses a new selection. Already-playing voices are untouched. Switching Device
On/Off off restores the native instrument sample map on the next note without
rewriting the XM.

Composition Baker resolves Sample Morph at each subsequent note trigger and
writes the exact encoder-selected sample into the standard XM result. It reuses
the source instrument only when that note already maps faithfully; otherwise it
creates and deduplicates a destination-only private instrument with an owned
sample copy and the source XM playback settings. The played note and pitch are
not changed, the loaded source instrument is never modified, and encoder
movement by itself is not written as automation.

## Pattern jog / bowed audition

`Cue Level=PatternJogRelative` uses the APC's relative encoder as a small jog
wheel. Clockwise messages drag the pattern downward and step backward through
rows; counterclockwise messages step forward. Every detent is preserved.

`Crossfader=PatternJogAbsolute` treats the fader as a long horizontal jog:
A is row `00`, B is the current pattern's final row, and positions between map
linearly across the pattern. Crossfader messages are preserved so a physical
sweep strums every intervening row once and in order. The Cue Level encoder can immediately
continue relatively from the crossfader's row; moving the crossfader again
traverses back to the absolute row represented by its new physical position.

During playback the jog moves the real master row and resets its tick, so the
crossed row retriggers and normal playback continues from the new location.
With ordinary playback stopped, it moves the editing row and auditions its
notes without executing effect commands. The default latched mode treats the
pattern as a non-clocked instrument: blank rows sustain, new notes replace that
channel, note-offs release it, and different channels can accumulate into a
drone. Shift + Stop All Clips hard-silences the accumulated jog voices without
stopping Q, Poly, FastTracks, or ordinary transport.

Set `PatternJogAudition=Latched`, `Momentary`, `ManualPingPong`, or `Off` in
`[MIDI]`. `Momentary` plays every crossed note forward as a one-shot regardless
of hand direction. `ManualPingPong` uses forward one-shots on a forward sweep
and reverse one-shots on a backward sweep. Both one-shot modes ignore the
sample's loop flags, let its natural decay reach the sample boundary, and are
replaced normally by the next note on that track. `Off` performs position-only
jogging. `CueLevel=None` disables the wheel and
`Crossfader=None` disables the absolute strum. Q/Poly trim and crossfading are
deferred to separate output routing and an external mixer.

`PatternJogFastTracks=Ignore` leaves FastTracks-assigned channels out of both
strum controls, creating an isolated ordinary-track performance layer.
`PatternJogFastTracks=Include` strums the visible ordinary row on those
channels as well, while leaving their private transport assignments intact.

## Matrix, transport, and timing

| APC control | Tapehead action | LED meaning |
| --- | --- | --- |
| Pan | Select Pattern Matrix | On = Pattern selected |
| Sends | Select Sample Matrix | On = Sample selected |
| User | Open/close standalone Deck Matrix | On = Deck Matrix visible |
| Session | Toggle Q/Poly input layer | Off = Q; on = Poly |
| Bank near Shift | Stop the selected Pattern/Sample target's displayed Q or Poly layer | On = selected layer has work |
| Stop All Clips | Finish an armed/active Live or Performance Bake; otherwise stop both Matrix engines while ordinary song continues | Momentary |
| Shift + Stop All Clips | Hard-silence latched jog voices only; Matrix, FastTracks, and ordinary transport continue | Momentary |
| Play | Toggle ordinary Song playback | On only during ordinary Song playback |
| Record | Toggle ordinary Pattern playback | On only during ordinary Pattern playback |
| Tap Tempo | Toggle all enabled FastTracks between Pattern and Song mode | No addressable LED; hold Shift to reveal each track's mode on Track Select |
| Tempo encoder | BPM ±1 per relative step | Hardware position |
| Nudge − / + | Speed/TPL −/+ 1 | No addressable LEDs |
| Metronome | Set all selected FastTracks tracks reverse/forward together | On = all selected tracks reversed |
| Master button | If any track is audible, Performance Mute all eight; if all are muted, unmute all | Solid = all, blink = some |
| Master fader | FT2 master volume, 0–256 | Hardware position |
| Crossfader | Absolute pattern strum: A = row 00, B = final row | Hardware position |
| Bank Select Left / Right | Native Shift+Tab / Tab track navigation, landing on note field | — |
| Bank Select Up / Down | Previous/next Song Order position, stopping at the boundaries | Jump backward/forward by the current edit-step length |
| Footswitch | Transport Punch: freeze/unfreeze every automatic transport while leaving the mixer alive | Configurable Toggle or Hold behavior |

Transport Punch makes the human the flywheel. While frozen, ordinary Song or
Pattern playback, FastTracks, Q, Poly, and Sample Matrix scheduling stop
advancing, but looped voices, natural one-shot decays, and external effect tails
remain audible. Cue Level and the crossfader remain active for manual strumming.
Up/Down can relocate through actual song-order positions, including repeated
entries that contain the same pattern.

`TransportFreezeNavigation=Silent` makes those order jumps secret and processes
the destination row when the pedal resumes. `Audition` plays the destination
row immediately; that row is then consumed, so resume continues with the next
row instead of striking it twice. `TransportFreezeAudio=Sustain` preserves
voices on entry, while `Cut` silences them. `TransportFreezePedalMode=Toggle`
uses successive downstrokes; `Hold` freezes only while the pedal is depressed.
`TransportFreezeResume=Next` treats the originally frozen row as already heard;
`Retrigger` deliberately plays it again when transport punches back in.

Automatic playhead display sync is suspended while punched out. The pattern
editor therefore follows Cue Level, crossfader, and Up/Down movements directly
instead of repainting the last scheduled transport row over the manual one.

## LED language and cleanup

| Tapehead state | Q-selected feedback | Poly-selected feedback |
| --- | --- | --- |
| Available Pattern / loaded Sample | Dim green | Dim blue |
| Empty or masked/unavailable tile | Off | Off |
| Q active | Pulsing green | Dim green |
| Q queued / automatic future item | Blinking yellow | Dim yellow |
| Return / Continue / Stop | Yellow / orange / blinking red | Dimmed equivalent |
| Poly active | Dim teal | Cyan |
| Poly start / stop pending | Dim pulse / blink | Pulsing teal / blinking dark cyan |
| Same tile in Q and Poly | Alternating Q and Poly colors | Alternating Poly and Q colors |
| Upper column launch rail | Dim purple; active column pulses bright purple | Same fixed launch color |

Off grid tiles cannot be launched or queued in Q or Poly. This applies to
empty tiles and masked Pattern tiles; use Performance Mute when silence is the
intended performance gesture.

Each physical RGB pad has one authoritative steady/pulse/blink state. Tapehead
clears obsolete animation channels on every transition and explicitly clears
the complete APC feedback surface before exit or profile close, so old pad
animations cannot survive after the program closes.

The `[MIDI]` setting `APC40RGBBrightness=0..100` scales all 32 lower Matrix
pads and the eight upper column-sequence pads through hue-matched hardware
palette variants. `100` preserves the original colors exactly; `0` turns
those pads off without disabling their input. Intermediate values progressively
soften bright queued yellow and the green, cyan/blue, purple, orange, and red
families where the discrete APC palette permits. The setting does not affect
fixed-color track, transport, Scene Launch, or global-button LEDs. Missing or
invalid values default to `100`, and out-of-range integers are clamped.

Ordinary FT2 mute remains separate from Performance Mute. Ordinary mute uses
the white scope X; Performance Mute uses Tapehead's red scope X.

## Raw MIDI reference

All values are decimal and channels are one-based in `tapehead.ini`.

| Control family | Raw message |
| --- | --- |
| Clip grid | `NoteOn.1.0`–`NoteOn.1.39` |
| Record Arm / Solo / Activator / Track Select / Clip Stop | Notes 48 / 49 / 50 / 51 / 52 on channels 1–8 |
| Crossfader A/B buttons | Note 66 on channels 1–8 |
| Track faders | CC 7 on channels 1–8 |
| Device knobs / Track Control knobs | CC 16–23 / 48–55 on channel 1 |
| Device L/R, Bank L/R, On/Off, Lock, Clip/Device, Detail | Notes 58–65 on channel 1 |
| Master, Stop All, Scene 1–5 | Notes 80–86 on channel 1 |
| Pan, Sends, User, Metronome | Notes 87–90 on channel 1 |
| Play / Record | Notes 91 / 93 on channel 1; note 92 is unassigned |
| Up, Down, Right, Left, Shift | Notes 94–98 on channel 1 |
| Tap, Nudge−, Nudge+, Session, Bank | Notes 99–103 on channel 1 |
| Tempo / Master fader / Crossfader / Cue | CC 13 / 14 / 15 / 47 on channel 1 |
| Footswitch | CC 64 on channel 1 |

## Editable action vocabulary

```text
TrackPerformanceMuteToggle:n  TrackPerformanceSoloToggle:n
TrackMuteToggle:n             TrackRecordArm:n
TrackTrim:n                   TrackSelect:n
FastTrackToggle:n             FastTrackRatio:n
FastTrackRatioNext:n          FastTrackRatioPrevious:n
FastTrackRatioReset:n         FastTrackReverseToggle:n
FastTrackClutchToggle:n       FastTrackDirectionOrSongMode:n
FastTrackMasterToggle         FastTrackTransmissionClutchToggle
FastTrackGlobalReverseToggle  FastTrackGlobalModeToggle
FastTrackResetAll
FastTrackRatioAllNext         FastTrackRatioAllPrevious
FastTrackRatioAllOrMatrixBankNext
FastTrackRatioAllOrMatrixBankPrevious
SampleMorphArmToggle          SampleMorphSelect:n
SampleMorphAllNext            SampleMorphAllPrevious
PerformanceMuteAll            PerformanceUnmuteAll
PerformanceMuteMaster         UnmuteAll
MatrixModePattern             MatrixModeSample
MatrixModeToggle              MatrixGridModeToggle
MatrixVisibilityToggle        MatrixBankSelect:n
MatrixLayerBankSelect:n       MatrixBankNext
MatrixBankPrevious            MatrixSlotTrigger:n
MatrixSequenceRow:n           MatrixSequenceColumn:n
MatrixSequenceBank            PatternJogRelative
PatternJogAbsolute
TransportPunch
MatrixMasterVolume            MatrixCrossfader
TransportPlaySongToggle       TransportStop
TransportPlayPatternToggle    TransportPlaySong
TransportPlayPattern          TransportPlaySelectedMode
TransportModeToggle           TransportStopSong
TransportStopSelectedDeck     TransportStopDeck
TransportStopAll              MasterVolume
TempoRelative                 SpeedDown / SpeedUp
CursorLeft / CursorRight      CursorUp / CursorDown
SongOrderPrevious / SongOrderNext
ShiftModifier                 None
```

Arguments are one-based. Invalid control names, actions, tracks, banks, rows,
columns, or slots are ignored safely at startup.
