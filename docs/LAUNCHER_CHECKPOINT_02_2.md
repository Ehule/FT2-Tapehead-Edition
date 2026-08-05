# Launcher Checkpoint 02.2

Checkpoint 02.2 preserves the full-window launcher interface and redraw fix
from CP02.1 while restoring the configurable MIDI Dub channel map that was
present in the validated `FT2-HDV3-MIDIMap` checkpoint.

## MIDI Dub track map

Edit `[MIDIDub]` in `release/other/tapehead.ini` before starting Tapehead:

```ini
[MIDIDub]
Track01=1
Track02=2
...
Track16=16
Track17=1
...
Track32=16
```

Each value is an outgoing MIDI channel from 1 through 16. Tracks may share the
same channel. Missing, malformed, or out-of-range values keep the safe default
for that track: tracks 1-16 use channels 1-16 and tracks 17-32 repeat them.

The selected map is used for note-on, note-off, timed gate release, and MIDI
panic. It does not change XM data, audio-bus routing, Pattern/Sample launcher
ownership, or launcher controls.

Changes are loaded when Tapehead starts. Restart the program after editing the
map.
