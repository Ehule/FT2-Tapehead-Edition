# Multichannel Output — Pass 1

> Historical checkpoint: Pass 2 supersedes the SDL/JACK experiment below with
> a selectable native backend. See `MULTICHANNEL_PASS2.md` for current setup.

Tapehead now has sixteen fixed-capacity logical stereo buses (`A` through `P`).
The number exposed to the selected SDL audio device is set in `tapehead.ini`:

```ini
[Audio]
OutputBuses=2
```

`1` preserves ordinary stereo. Values from `2` through `16` request 4 through
32 interleaved output channels. If the selected SDL backend or audio device
cannot open the requested layout, Tapehead opens stereo and folds every logical
bus to Bus A without changing the per-track assignments.

## Scope controls

| Gesture over a channel scope | Result |
|---|---|
| `Alt+Left-click` | Cycle the physical FT2 channel through buses A-P (limited to `OutputBuses`) |
| `Ctrl+Alt+Left-click` | Toggle an additional feed to Bus A while retaining the selected auxiliary bus |

The scope shows the selected bus letter at the lower right. `+B`, for example,
means the channel feeds both Bus A and Bus B.

Assignments belong to physical FT2 channels. A Poly Matrix thread therefore
inherits the destination tunnel's output assignment alongside its trim,
performance mute, and FasTracks behavior.

## Signal behavior

- Notes, envelopes, effects, global commands, and Poly events are processed
  once.
- A multi-bus assignment duplicates the completed channel voice in the mixer;
  it does not replay tracker events.
- FT2 panning remains stereo inside every logical bus.
- Song-to-WAV rendering deliberately folds all buses to stereo for compatibility.
- Output routing is runtime-only in this pass and is not written into XM files.

## Linux / REAPER test

A backend must expose the requested channel layout. For JACK/PipeWire-JACK, try:

```sh
SDL_AUDIODRIVER=jack ./ft2-clone
```

Set `OutputBuses=2`, route one tracker channel to A and another to B, then
connect Tapehead's first four output channels to separate REAPER inputs or
hardware destinations.

The original Pass 1 source archive included
`release/other/ft2-clone-multichannel-pass1-nomidi` as a Linux smoke-test
build. It contains the multichannel changes but omits
MIDI because the build environment did not provide the ALSA development
headers. Use the normal build scripts on a Tapehead development machine for a
complete MIDI-enabled executable.

## Current boundary

This pass implements the shared internal bus mixer, direct multichannel device
output, per-track routing, stereo fallback, and Poly tunnel inheritance. It
does not yet add a graphical bus matrix, persistent per-song routing metadata,
ASIO-specific device code, or multibus stem export.
