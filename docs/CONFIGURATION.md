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
