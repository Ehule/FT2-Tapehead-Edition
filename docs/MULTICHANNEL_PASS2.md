# Multichannel Output — Pass 2

Pass 2 gives Linux Tapehead a native JACK/PipeWire-JACK destination. It removes
the dependency on SDL or a sound card advertising one large multichannel
device: Tapehead creates its own named virtual ports instead.

## Configure the bus count

Set the number of stereo buses in `tapehead.ini` beside the executable:

```ini
[Audio]
OutputBuses=2
```

Restart Tapehead after changing this value. Two buses create four JACK ports:

| Tapehead bus | JACK ports |
|---|---|
| A | `ft2_tapehead:bus_A_L`, `ft2_tapehead:bus_A_R` |
| B | `ft2_tapehead:bus_B_L`, `ft2_tapehead:bus_B_R` |

The same naming continues through Bus P when more buses are configured.

## Select the virtual output

Open **Config > Audio** and select:

`Tapehead JACK Virtual Outputs`

This entry is separate from entries such as `DSP56301 ... Stereo`. Selecting
the DSP/Indigo entry still opens one ordinary SDL stereo stream and folds all
logical buses together when the device cannot accept more channels.

The JACK entry appears whenever Tapehead can load `libjack.so.0`. A JACK or
PipeWire-JACK server must also be running when the entry is selected. If the
connection fails, Tapehead restores the last working audio device instead of
leaving audio disabled.

## Route the ports

Use a JACK/PipeWire patchbay such as qpwgraph, Helvum, or QjackCtl:

- For REAPER, connect each Tapehead port pair to distinct REAPER input ports.
- For a MOTU M6 or another interface, connect each Tapehead port pair directly
  to the desired hardware playback pair.
- For the Indigo DJx, connect Bus A to its first playback pair and Bus B to its
  second pair if PipeWire/JACK exposes both hardware pairs independently.

Tapehead does not auto-connect ports. This is deliberate: the same logical bus
can feed REAPER, hardware, or both without changing the tracker project.

## Track controls

| Gesture over a channel scope | Result |
|---|---|
| `Alt+Left-click` | Cycle the physical FT2 channel through configured buses |
| `Ctrl+Alt+Left-click` | Toggle an additional feed to Bus A |

The scope marker shows the selected bus. A marker such as `+B` means the
channel feeds both Bus A and Bus B. Poly Matrix threads continue to inherit the
destination tunnel's output assignment.

## Compatibility and boundaries

- The JACK path is Linux-only and always uses 32-bit float audio.
- Ordinary SDL output remains available on Linux, Windows, and macOS.
- A stereo SDL device still receives a safe fold-down of every logical bus.
- Song-to-WAV rendering remains stereo.
- Bus assignments remain runtime-only and are not written into XM files.
- Port reconnection after a JACK server restart is not automatic in Pass 2;
  reselect the virtual output device.

The source archive includes `release/other/ft2-clone-multichannel-pass2-nomidi`
as a Linux smoke-test executable. It omits MIDI only because the build
environment used for this checkpoint lacked ALSA development headers. The
normal Linux build scripts retain MIDI on a complete development system.
