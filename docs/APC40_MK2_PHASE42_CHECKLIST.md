# APC40 mkII Phase 4.2 correction checklist

This checklist records the complete live-test correction pass built on the
user-tested Phase 4.1 source tree. It is intentionally separate from the
finished key map so implementation and acceptance testing cannot silently lose
an earlier agreement.

## LED state and shutdown

- Treat each RGB pad as one physical LED state, not as independent steady,
  pulse, and blink LEDs.
- Clear the obsolete animation channel before changing a pad to another
  animation or to a steady color.
- Clear every APC feedback LED before program exit, profile close, or output
  disconnect, then return the controller to Generic mode.
- A power cycle must no longer be needed to remove animations left by
  Tapehead.

## FastTracks, banks, mutes, and cursor

- Track Select 1-8 toggles per-track FastTracks Reverse.
- With no Shift, Track Select LEDs show Reverse state.
- While Shift is held, those same LEDs temporarily show per-track FastTracks
  Song Mode. Activator LEDs must not be used for this status.
- Activator 1-8 directly selects Pattern Bank 1-8.
- Shift + Activator 1-8 directly selects Sample Bank 1-8.
- Activator LEDs normally show the selected Pattern bank and temporarily show
  the selected Sample bank while Shift is held.
- Bank Left/Right steps every enabled FastTracks ratio down/up one clamped
  degree. Shift + Bank Left/Right moves the currently selected Matrix to its
  previous/next bank.
- Master toggles all eight Performance Mutes: if they are not all muted it
  mutes all; if they are all muted it unmutes all.
- Cursor Left/Right uses native Shift+Tab/Tab channel stepping and always lands
  on the note field.
- Cursor Up/Down moves one row only while stopped and does nothing during
  playback.

## Session and transport

- Session reliably changes the grid input layer without stopping existing Q
  or Poly activity.
- Session off selects Q and uses a green/yellow grid palette.
- Session on selects Poly and uses a blue/cyan grid palette.
- Switching Session immediately redraws all 40 RGB pads. Activity on the
  non-selected layer remains visible at reduced emphasis; simultaneous Q and
  Poly state remains distinct.
- Play toggles ordinary Song playback and its LED reflects Song playback.
- Record toggles ordinary Pattern playback and its LED reflects Pattern
  playback.
- Starting one ordinary mode replaces the other. Pressing its lit button stops
  ordinary playback without destroying independent Poly/Sample work.
- Main Stop stops ordinary playback and all Matrix activity.
- Stop All Clips stops both Matrix engines without becoming the ordinary song
  transport stop.
- Bank stops only the currently selected Matrix deck.

## Automatic Q sequences

- Grid Top Pad 1-8 starts a top-to-bottom Q sequence for its column.
- Scene Launch 1-4 starts a left-to-right Q sequence for the corresponding
  Matrix row.
- Scene Launch 5 starts a row-order Q sequence for the complete 32-tile bank.
- Empty/unavailable tiles are skipped.
- A sequence replaces the previous automatic sequence after the currently
  playing Q item finishes; it does not expand the ordinary four-entry queue.
- Pressing another row/column/whole-bank control replaces the remaining list.
- Pressing the active sequence control again cancels its remaining list.
- Pressing an individual Matrix tile cancels the automatic sequence and its
  queued next item before performing the manual Q or Poly action.
- Poly activity remains independent.
- Active automatic controls pulse, the current item uses native Q feedback,
  and every future sequence tile is visible without leaving stale animations.

## Matrix mixer

- Cue Level is a smoothed master volume for Q and Poly Matrix audio only.
- Crossfader A keeps Q at unity and fades Poly to silence.
- Crossfader center keeps Q and Poly at unity.
- Crossfader B keeps Poly at unity and fades Q to silence.
- Changes use short ramps to avoid zipper noise and never alter ordinary Song
  playback.
- Both controls remain editable in `[APC40MK2_MAP]`; setting either action to
  `None` disables that physical control.

## Deliverables and regression

- Update built-in defaults, generated `tapehead.ini`, shipped `tapehead.ini`,
  the editable action parser, and the printable key map together.
- Preserve user-edited mappings when the installer is rerun.
- Cover mappings, Shift-status display helpers, RGB transitions and cleanup,
  transport toggles, independent banks, sequence replacement/cancellation,
  mixer curves, and playback-safe cursor behavior in native tests.
