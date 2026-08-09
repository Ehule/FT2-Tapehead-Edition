# APC40 mkII Phase 4.3 live-test correction checklist

This pass is a delta from the user-tested Phase 4.2 source and retains all
Phase 4.2 bank, Shift-status, Q/Poly palette, sequence, mute, cursor, and LED
cleanup behavior.

## Launch controls and Matrix stops

- The physical top Scene Launch button sequences the complete 32-tile bank.
- The four Scene buttons below it sequence visually adjacent rows 1-4.
- The eight top RGB pads are dim purple; an active column launch pulses bright
  purple. Scene LEDs remain on and the active sequence button blinks.
- Shift + a lower Matrix pad never launches an inactive tile. It cancels a
  pending launch or asserts a quantized stop for every active Q and Poly
  instance of that tile.
- Repeated Shift + pad presses cannot cancel an already pending stop.
- The Bank button stops only the selected Pattern/Sample target's currently
  displayed Q or Poly layer.
- Stop All Clips stops Pattern Q, Pattern Poly, Sample Q, and Sample Poly while
  leaving ordinary Song/Pattern transport alone.

## Ordinary transport and FastTracks

- Play toggles ordinary Song mode; Record toggles ordinary Pattern mode.
- Starting either ordinary mode exits Pattern Q, cancels automatic Q sequence
  state, and leaves independent Poly audio intact.
- Active Pattern Q can no longer make the second Play/Record press restart.
- The APC40 mkII has no ordinary transport Stop button; the old phantom
  `Stop=` assignment is removed from its profile.
- Shift + Stop All Clips hard-silences latched jog voices only. Ordinary
  transport, FastTracks, Pattern/Sample Q, and Pattern/Sample Poly continue.
- Tap Tempo toggles every enabled FastTrack between Pattern and Song mode and
  never changes ordinary FT2 playback.
- Tap LED is off for all-Pattern, on for all-Song, and blinking for mixed
  per-track modes.

## Cue Level pattern jog

- Cue Level defaults to `PatternJogRelative`; Crossfader defaults to
  `PatternJogAbsolute`.
- Every relative Cue Level message is queued; detents are never coalesced.
- Clockwise/right hardware messages step backward while visually dragging
  pattern data downward. Counterclockwise/left steps forward.
- While playing, the real master row moves and tick 1 is armed so the new row
  retriggers on the next audio callback.
- While stopped, the editing row moves and auditions note/instrument data
  without executing XM effects.
- `PatternJogAudition=Latched` sustains across blank rows, replaces a channel
  on a new note, obeys pattern note-offs, and accumulates channels.
- `Momentary` releases after a short UI-frame timeout. `Off` is position-only.
- Shift + Stop All Clips sends a musical note-off and hard-clears the jog-owned
  mixer voice, so an infinite forward/ping-pong loop cannot remain stuck.
- Poly-owned destinations are never killed by jog cleanup.
- Crossfader A maps to row `00`, B maps to the final pattern row, and every
  intermediate hardware message is preserved for physical strumming.
- Cue Level can take over relatively from a crossfader row at any time; the
  next crossfader movement reasserts its new absolute position.

## Delivery and regression

- The standalone installer is run from the Phase 4.2 repository root as
  `python3 ~/Downloads/apply_tapehead_phase43_APC40_PERFORMANCE.py`.
- The installer embeds its patch, checks compatibility before editing, updates
  only old Phase 4.2 default mappings, preserves custom mappings and device
  names, and is idempotent.
- Native tests cover active-Q transport toggles, relative-event preservation,
  global FastTracks mode, Shift-pad idempotency, layer stop isolation, latched
  jog behavior, and the full existing Tapehead regression suite.
