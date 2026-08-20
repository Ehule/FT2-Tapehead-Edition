# Hybrid per-track transport visuals

PR42 makes the Pattern Editor choose its transport visualization independently for every visible tracker channel.

## Rendering invariant

The mode is selected per track and is not controlled by a machine-local INI
switch. This keeps Linux and Windows behavior identical: an active
FastTracks/LEN/Freeze lane always owns stationary data and a moving playhead,
while a standard lane keeps ordinary FT2 scrolling.

## Rendering rules

With hybrid visuals enabled, a normal master-synchronized track keeps the FastTracker II model: pattern data scrolls through the fixed central row. A track with an independent read position uses stationary pattern data and the existing colored TapeHead playhead. FastTracks uses its private source pattern/row, LEN-only tracks use the same effective LEN resolver as playback, reverse follows the FastTracks source row backward, and Transport Punch/Freeze holds the colored head at the frozen authoritative row.

FastTracks + LEN uses the FastTracks resolved source row and effective private transport domain rather than a second visual counter. If a pattern is taller than the visible tracker area, an independent lane changes stationary pages only when its real read head crosses a viewport page boundary.

## Pattern Jog / strumming

The current APC mappings remain authoritative: `Crossfader=PatternJogAbsolute` and `CueLevel=PatternJogRelative`.

A normal track being strummed keeps the FT2 metaphor: the Pattern Jog row moves the data across the fixed central head. An already-independent track keeps its data stationary and the colored head follows the actual Pattern Jog row during the gesture. A short UI-only activity hold prevents flicker between MIDI messages; it does not delay or change audio.

`[MIDI] PatternJogFastTracks=Ignore|Include` still controls FastTracks participation, not the renderer. `Ignore` leaves excluded FastTracks running on their own private heads. `Include` lets the strum temporarily display the ordinary pattern row that Pattern Jog actually reads on those channels, then the private FastTracks head resumes.

## Editing safety

Hybrid playback rendering never rewrites pattern, song-order, block, or undo
data. While playback is running, the pattern-data body is display-only: mouse
buttons cannot place the cursor, create or clear a block mark, or audition a
row. LEN/control and FasTrack gestures in the channel headers remain active.
When playback is stopped, ordinary cursor placement, block marking, and middle
click auditioning work normally. Recording modes retain the established editor
coordinate model for write safety, and a stored block mark never changes the
transport view.
