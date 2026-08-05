# Structural checkpoint 01

> **Historical RC1 engineering record.** Later Deck Matrix and Composition
> Baker work builds on this refactor. Use [`README.md`](README.md) for the
> current CP04.6 documentation map.

This checkpoint changes source ownership, not the musical model.

## Checkpoint 01A performance addition

`Shift`-clicking the `INP` button inserts a genuinely new pattern after the
current order position, copies every row and channel from the current pattern,
and preserves its exact pattern length. This gesture ignores IPL; plain `INP`
continues to create a blank pattern using the existing IPL rule.

Undo/Redo treats the inserted order entry and copied pattern as one operation.

## Checkpoint 01B performance addition

Pattern Matrix pads whose pattern number appears anywhere in the song order
receive a subtle theme-aware base tint. Patterns available only to the Matrix
retain their previous appearance. Repeated order entries are automatically
deduplicated because the tint describes pattern membership, not order
occurrences.

The established Matrix transport colors take priority over the base tint:
playing/breathing, queued, return, continue, and stop indications are unchanged.
Order-list edits, Undo/Redo, and module loading refresh the classification
automatically through the position-editor redraw callback.

## Checkpoint 01C visual revision

Song membership now uses the pattern-number text instead of a background tint.
Populated patterns referenced by the song order follow the theme's Pattern Text
color. Populated Matrix-only patterns follow the Mouse color also used by the
pattern editor's channel-header numbers. Empty pads retain their dim text.

Playing, queued, return, continue, and stop states continue to use only their
established cell backgrounds. The pattern-number color remains a persistent
membership cue while those transport states are active.

## Checkpoint 01D Matrix membership refresh

The visible Matrix bank now watches its 32 patterns for populated/empty
transitions. Entering the first event in an unreferenced pattern immediately
changes its number from the empty color to the Matrix-only Mouse color.
Removing the final event immediately restores the empty color. No bank change
or manual redraw is required.

This lightweight check compares only allocation state for the visible bank and
redraws only when a pad changes, so inactive banks are not scanned continuously.
Song-order membership continues to use its separate order-list callback.

## Checkpoint 01E Matrix pattern management

An empty pattern that is still referenced by the song order now uses a dimmed
version of Pattern Text. This distinguishes an intentional empty song section
from both populated song material and completely unused pattern numbers.

Matrix right-click gestures now manage the underlying pattern object:

- `Shift`-right-click asks for confirmation, then clears the pattern and removes
  every occurrence of its number from the song order.
- `Ctrl`-right-click asks for confirmation, then clears only the pattern data
  and preserves every song-order occurrence as an intentional empty section.
- Plain right-click is reserved and performs no action.

Both destructive gestures use one atomic Undo/Redo transaction. Clearing data
does not reset sounding voices, so a note already sustaining into an empty
order position can continue as a drone.

## Checkpoint 01G Sample Editor shortcut repair

The Sample Editor now claims `Ctrl+Shift+E` and `Ctrl+Shift+X` before the global
32-channel FasTracks keyboard handler. This restores the intended extraction
behavior without removing either FasTracks binding:

- With the Sample Editor visible, `Ctrl+Shift+E` extracts cursor-to-end into an
  empty sample slot in the current instrument.
- With the Sample Editor visible, `Ctrl+Shift+X` extracts the selected range
  into an empty sample slot in the current instrument.
- Outside the Sample Editor, those same physical keys retain their FasTracks
  channel 13 and channel 31 toggle behavior.

Key-repeat events are consumed without repeating the extraction, preventing a
held chord from filling multiple sample slots.

## Checkpoint 01I global Z-command timing repair

Global FasTracks commands `Z20-Z2B` are collected in encounter order during an
audio tick and applied only after the complete channel pass. A control lane no
longer changes the transport state midway through left-to-right channel
processing.

This specifically keeps tracks on both sides of `Z23`, `Z24`, or `Z2B`
exactly synchronized. The green phase indicator now remains lit after the
`1:1` reset because the later channels no longer advance one extra private
tick.

## Checkpoint 01J inserted-pattern edit-target repair

When `INP` or `Shift+INP` selects a newly inserted order during playback, that
pattern now becomes the editor's immediate target as well as the replayer's
target. Pattern, Track, and Block Transpose therefore operate on the visible
new pattern instead of the preceding source pattern.

This does not change stock FT2's instrument-memory rule: notes with `00` in the
instrument column are ignored by Current Instrument Transpose and remain
available to All Instruments Transpose.

## Checkpoint 01K input and Matrix state repair

Keyboard shortcuts now use the modifier mask attached to each SDL key event,
so `Ctrl+Grave` cannot fall through into edit-step adjustment when both events
arrive in one queue pass. The INP button similarly captures Shift when the
click begins, making playback-time `Shift+INP` duplication independent of
mouse-up timing. Pattern Matrix membership and populated/empty colors update
directly at their mutation boundaries.

## Checkpoint 01L pattern-context integrity and VIEW Transpose

Manual order-list scrolling and pattern-number changes now publish one atomic
pattern context to the replayer, editor, and editor synchronization shadow.
Obsolete audio/video sync entries are cleared before the new identity becomes
editable, so normal Transpose cannot silently modify a prior pattern during
Pattern Play.

FasTracks state is not reset by this repair. Pattern transports keep their
private row and phase; Song transports continue freewheeling through their
private order list; ratio, clutch, reverse, and direction state remain intact.
Song-mode columns now draw data from each head's actual private source pattern,
keeping the visible tape aligned with the audible one.

The Transpose panel adds a `VIEW` checkbox. When enabled, the button press
snapshots the literal rendered slots before changing notes:

- Track affects the visible slots on the cursor track.
- Pattern and Song affect every horizontally visible canvas track.
- Block affects the intersection of the block mark and visible canvas.
- Enabled FasTracks channels use their private rows and, in Song mode, their
  private source patterns.
- Standard or clutched channels use the master pattern and rendered master rows.
- Rows repeated through wrapping are modified once per click.

Disabling `VIEW` restores the ordinary complete Track, Pattern, Song, and Block
Transpose scopes.

VIEW snapshots a moving render surface rather than promising a simultaneous
picture of every private transport. At extreme ratios, adjacent columns can be
drawn from different private-head moments. Notes that were originally aligned
may therefore be captured and transposed differently. The immutable target
snapshot and wrapped-row deduplication still guarantee that every captured
underlying event is changed once per button press.

## FasTracks

- `ft2_fasttracks.c/.h` owns ratios, per-track state, Pattern/Song traversal,
  clutch behavior, synchronization, control operations, UI snapshots, and the
  audio-thread crossing interface.
- `ft2_fasttracks_core.c/.h` owns the FT2-independent rational clock.
- `ft2_replayer.c` still decides when tracker notes and effects are processed,
  but it no longer owns or reaches into FasTracks state.
- `ft2_pattern_draw.c` takes one complete FasTracks snapshot before drawing a
  frame, so headers and private-row views describe the same transport moment.

## Pattern Matrix

- `ft2_pattern_launcher.c/.h` owns the active pattern, four-item queue, saved
  song position, exit gesture, and pattern-boundary handoff.
- `ft2_pattern_launcher_ui.c/.h` owns the complete borrowed Matrix surface:
  cells, shell, bank-button parent column, captions, page state, breathing
  animation, and mouse hit testing.
- `ft2_replayer.c` asks the launcher for a boundary result instead of editing
  launcher internals.
- `ft2_pattern_ed.c` asks the launcher UI to draw or change state instead of
  sharing its private rectangles and variables.

## Tests

Run both transport suites:

```bash
python3 scripts/test_fasttracks_transport.py
python3 scripts/test_fasttracks_native.py
```

The first suite retains the broader deterministic behavior model. The second
compiles and executes the actual C rational clock used by the replayer.

For the 01A addition, manually test `Shift`-click `INP` once with IPL off and
once with IPL on. In both cases verify that the new pattern has a different
number, identical data and length, and remains independent when edited. Then
verify one-step Undo and Redo of the complete insertion.

For the 01B addition, put one pattern in the order list more than once and keep
another populated pattern outside it. Verify that only the song pattern is
tinted, that adding/removing an order reference updates the tint, and that
playing and queued colors still replace the base tint cleanly.

For the 01C revision, repeat that setup and verify that song-referenced pattern
numbers follow Pattern Text, Matrix-only pattern numbers follow Mouse, and empty
pads remain dim. Change both theme colors and confirm the Matrix updates. Queue
and launch patterns from both categories and verify that their number colors
remain classified while the established cell backgrounds show transport state.

For the 01D refresh, select an unused pattern outside the song order and enter
one event. Verify that its Matrix pad immediately changes to the Mouse color
without changing banks. Populate several adjacent patterns, then clear every
event from one of them and verify that only that pad immediately returns to the
empty color. Repeat both transitions through Undo/Redo.

For the 01F addition, Ctrl-right-click a populated song pattern, cancel once,
then confirm once. Verify that its number becomes dim while its order positions
remain. Undo and Redo the clear. Then Shift-right-click a repeated song pattern,
cancel once, confirm once, and verify that its data and every order occurrence
are removed together. Undo and Redo the full deletion. Repeat Shift-right-click
on a populated Matrix-only pattern and verify that its data is released.

For the 01G repair, open the Sample Editor on an instrument with at least three
empty sample slots. Mark a range and press `Ctrl+Shift+X`; verify that one new
sample appears in the current instrument while the source selection and view
remain unchanged. Place the cursor elsewhere and press `Ctrl+Shift+E`; verify
that one cursor-to-end sample appears. Hold each chord briefly and verify that
key repeat does not create additional samples. Close the Sample Editor and
confirm that `Ctrl+Shift+E` and `Ctrl+Shift+X` still toggle FasTracks channels
13 and 31 respectively.

For the 01J repair, begin song playback, then use plain `INP` and
`Shift+INP`. In each case verify that the inserted order, displayed pattern,
and immediate Pattern/Track/Block Transpose target all use the same new pattern.
Verify that the source pattern remains unchanged after transposing the
`Shift+INP` copy. Repeat while stopped. Finally, generate a Melodic Walk from a
seed with instrument `00` and confirm that Current Instrument Transpose retains
stock FT2 behavior while All Instruments Transpose changes the generated notes.

For the 01L repair, begin Pattern Play and scroll repeatedly through several
distinct order entries. Transpose each visible pattern and confirm that no
other pattern changes, even before the next audio/video refresh. Repeat with
the pattern-number arrows, `Shift+INP`, reverse Pattern transport, and a
freewheeling reverse Song transport.

Then enable `VIEW` in the Transpose panel. At a high ratio such as `5:1`,
transpose the visible Track and Pattern scopes while notes rotate through the
editor. Confirm that visible events change, off-screen events remain untouched,
and a Song-mode channel edits the private pattern displayed under that head.
Disable `VIEW` and confirm that ordinary full-pattern Transpose returns.

## Deliberately deferred

MIDI Dub still sends through its established callback path. Moving MIDI output
to a worker queue changes timing and shutdown behavior, so it belongs in a
separate checkpoint with dedicated Linux and Windows testing.
