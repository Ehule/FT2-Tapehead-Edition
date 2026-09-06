# Tapehead → TapeSister Live Link

Live Link sends Tapehead's post-mixer stereo output directly into TapeSister without
VB-CABLE, an audio-interface loopback, or a second physical-output client. TapeSister
owns the speakers or interface; Tapehead uses a clocked shared-memory destination.

## Start it

1. Open TapeSister and leave its normal output device selected.
2. In Tapehead, open **Config → Audio** and select **TapeSister Live Link** as the
   output device.
3. Open Sister Machine in TapeSister. Its source strip reports **LINK** when the stream
   is available and **WAIT** when it is not.
4. With Sister Machine powered off, Tapehead is heard through TapeSister's ordinary
   program/post-FX path. With Sister Machine powered on, enable **TH SRC** to send it
   through the rolling tape, heads, Fallout, and pedalboard.

Either application may start first. TapeSister checks for a producer once per second,
and reconnects after a Tapehead restart. Selecting another Tapehead output device ends
the link and restores that device as Tapehead's audio owner.

## What is sent

The link carries 32-bit floating-point stereo at Tapehead's configured sample rate.
If Tapehead is configured for multiple logical buses or Mono Outputs, the Live Link
destination deliberately uses Tapehead's established stereo fallback: all tracker and
launcher routes fold to Bus A before transmission. Use native JACK or a multichannel
hardware destination when separate output buses are required.

TapeSister resamples automatically when the applications use different rates. Live Link
uses its own 256-frame software render quantum rather than inheriting Tapehead's
512/1024/2048-frame hardware preference. TapeSister's adaptive reserve targets 25 ms,
with a two-callback safety floor for large output buffers, so scheduler jitter and
independent application clocks do not cause periodic dropouts. Connection, underrun,
overrun, restart, and stop transitions are bounded and faded.

## Capture and processing

Once connected, Tapehead behaves like another live TapeSister instrument source:

- use the ordinary four-slot pedalboard with Sister Machine off;
- enable **TH SRC** with Sister Machine on for the tape heads, Soak/Bleed, Fallout,
  placed pedalboard slots, and feedback structures;
- use TapeSister's main Capture/Overdub workflow to print the performed output into a
  tile;
- select a Sister tap and tile or FILE destination to capture the processed tape path.
- select TapeSister's **TAPEHEAD** capture tap to record the raw linked stereo stream
  directly to a tile, Overdub, or long-form FILE.

TapeSister exposes three independent controls so routing and transport cannot change one
another accidentally. **TH SRC** controls only Sister routing, **TH SONG** toggles Song
Play/Stop, and **TH PATT** toggles Pattern Play/Stop. The two transport buttons receive
Tapehead's real mode state and light only while that mode is playing. Commands are
handled on Tapehead's main thread and cannot repeat from a held click.

The TAPEHEAD mixer fader is independent and persists with TapeSister's configuration,
projects, and Sister presets. Only the newest Tapehead instance selecting Live Link is
the active producer.

## Companion window switching

Press **Ctrl+Tab** in either application to move to the other one. This is an
independent companion control channel, not part of the audio ring, so it also works
while Tapehead uses a physical output and Live Link shows **WAIT**. It never launches a
missing application or changes playback, routing, recording, or interface state.

TapeSister remembers which of its two windows was last active. A performance can
therefore move from Tapehead back to the main TapeSister window, Sister Machine,
Fallout, or the pedalboard exactly as it was left. Tapehead likewise retains an open
Config panel, editor, or performance surface.

## Windows coexistence

Live Link opens no Tapehead hardware output. This avoids the DirectSound/WASAPI/ASIO
device-ownership conflict that motivated the feature. TapeSister is the sole physical
output owner, so its existing Auto/WASAPI policy remains the recommended Windows
starting point. REAPER can still record TapeSister through an interface loopback or
other normal DAW route when desired.

## Release check

- Start TapeSister first, then Tapehead; repeat in the opposite order.
- Confirm **WAIT** changes to **LINK** and audio fades in without a click.
- Restart either program and confirm automatic recovery.
- Test 44.1 kHz → 48 kHz and 96 kHz → 48 kHz.
- Verify ordinary post-FX, Sister Machine, Fallout, pedalboard, tile Capture, and FILE
  capture.
- Verify independent TH SRC, TH SONG, and TH PATT behavior and truthful transport lights.
- Run TapeSister at 256, 512, 1024, and 2048 frames without link underruns.
- Switch Tapehead back to a hardware output and confirm the link fades out cleanly.
- With Live Link both selected and unselected, use Ctrl+Tab in each direction and
  confirm the last active windows and open panels are preserved.
