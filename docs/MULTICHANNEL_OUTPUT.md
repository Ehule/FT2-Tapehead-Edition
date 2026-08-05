# Multichannel and mono output — CP04.6

Tapehead separates tracker routing from pattern data. Each physical FT2 channel
can feed a logical stereo bus or, in Mono Outputs mode, one physical mono
destination. Poly Matrix threads inherit the trim, performance mute,
FasTracks behavior, and output assignment of the destination tunnel they use.

Routing is runtime-only and is not saved in XM.

## Stereo bus mode

Set the exposed bus count in `tapehead.ini`:

```ini
[Audio]
OutputBuses=2
MonoOutputs=false
```

`OutputBuses` accepts `1`–`16`, representing logical stereo buses A–P and 2–32
physical output channels.

| Gesture over a channel scope | Result |
|---|---|
| `Alt+left-click` | Cycle through the exposed buses |
| `Ctrl+Alt+left-click` | Toggle an additional feed to Bus A |

The scope marker shows the selected bus. `+B`, for example, means the channel
feeds Bus A and Bus B. Audio is rendered once and distributed after tracker
voice/event processing; a multi-destination route does not replay the note.

If the selected SDL device cannot open the requested multichannel layout,
Tapehead opens stereo and folds every logical bus to Bus A without changing the
stored runtime assignments. Song-to-WAV rendering remains stereo.

## Linux JACK/PipeWire-JACK

When Tapehead can load `libjack.so.0`, Config > Audio includes:

```text
Tapehead JACK Virtual Outputs
```

With two stereo buses it creates:

| Bus | JACK ports |
|---|---|
| A | `ft2_tapehead:bus_A_L`, `ft2_tapehead:bus_A_R` |
| B | `ft2_tapehead:bus_B_L`, `ft2_tapehead:bus_B_R` |

Naming continues through Bus P. Use QjackCtl, qpwgraph, Helvum, REAPER, or
another JACK graph client to connect each port pair to recording inputs or
hardware playback ports. Tapehead does not auto-connect them.

The JACK client remains active and emits silence while editor operations pause
the mixer, so graph connections survive sample, instrument, Undo, module, and
editor changes. Final program shutdown closes the client. If initial connection
fails, Tapehead restores the last working audio device.

The native JACK path is Linux-only and uses 32-bit float audio. Ordinary SDL
output remains available on Linux, Windows, and macOS.

## Mono Outputs mode

Enable **Mono** in Config > Audio or set:

```ini
[Audio]
MonoOutputs=true
```

Each scope letter becomes one physical mono output: `A=1`, `B=2`, `C=3`, and
so on. Routes initialize by physical tracker lane and repeat across the exposed
outputs. `Alt+left-click` cycles the available destinations.

Mono delivery uses a dedicated gain path before FT2 stereo panning. Panning
commands and envelopes remain stored in the XM but cannot leak the signal into
an adjacent output while Mono Outputs is active. The output endpoint remains
open when switching modes so established JACK graph connections are preserved.

`Ctrl+Alt+left-click` is reserved in Mono Outputs mode for a possible future
per-track stereo override.

## Performance mute versus stock mute

- `Shift+left-click` a scope toggles Tapehead performance mute. It silences the
  internal sample in the mixer but leaves timing, effects, envelopes, loops,
  and MIDI Dub running.
- Stock FT2 channel mute stops normal channel processing and future MIDI Dub
  triggers as well as sample playback.

Use MIDI Panic if an external instrument is already sustaining a note that
must stop immediately.

## Validation

Run the integrated native suites:

```bash
python3 scripts/test_multichannel_native.py
python3 scripts/test_audio_bus_delivery.py
python3 scripts/test_jack_native.py
```

The historical Pass 1–5 documents record the route from the initial SDL bus
mixer through the hardware-confirmed JACK Bus B repair. They remain useful for
diagnosis, but this file is the current setup reference.
