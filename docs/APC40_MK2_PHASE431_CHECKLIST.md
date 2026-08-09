# APC40 mkII Phase 4.3.1 jog follow-up

This small pass applies to the user-tested Phase 4.3 source.

- The nonexistent APC ordinary Stop control and its `Stop=` profile line are
  removed. Generic `TransportStop` remains available for other MIDI devices.
- Stop All Clips without Shift still stops Pattern Q/Poly and Sample Q/Poly
  without stopping ordinary tracker transport.
- Shift + Stop All Clips hard-silences only latched Cue Level/crossfader jog
  voices, including infinite forward and ping-pong sample loops. It leaves Q,
  Poly, FastTracks, and ordinary transport running.
- Crossfader defaults to `PatternJogAbsolute`: A is row `00`, B is the current
  pattern's last row, and the range between maps linearly across the pattern.
- Crossfader position messages are not coalesced, allowing physical sweeps to
  strum the crossed rows. The event queue holds 256 pending messages so a full
  128-value sweep has headroom for simultaneous controls.
- Cue Level retains `PatternJogRelative` and takes over immediately from the
  current row. Moving the crossfader afterward jumps back to its new absolute
  position.
- While stopped, both controls use the configured Latched, Momentary, or Off
  audition policy. While playing, both move the real replayer row and arm a
  tick-zero retrigger without disturbing private FastTracks heads.
- `Crossfader=None` and `CueLevel=None` remain valid opt-out mappings.
