# Hybrid per-track transport visuals

PR42 makes the Pattern Editor choose its transport visualization independently for every visible tracker channel.

## Configuration

Add this key under `[Pattern]` in `tapehead.ini`:

```ini
[Pattern]
PerTrackTransportVisuals=true
```

The default is `true`, including for older `tapehead.ini` files where the key is absent. Set it to `false` to retain the pre-PR42 TapeHead transport rendering as closely as possible.

## Rendering rules

With hybrid visuals enabled, a normal master-synchronized track keeps the FastTracker II model: pattern data scrolls through the fixed central row. A track with an independent read position uses stationary pattern data and the existing colored TapeHead playhead. FastTracks uses its private source pattern/row, LEN-only tracks use the same effective LEN resolver as playback, reverse follows the FastTracks source row backward, and Transport Punch/Freeze holds the colored head at the frozen authoritative row.

FastTracks + LEN uses the FastTracks resolved source row and effective private transport domain rather than a second visual counter. If a pattern is taller than the visible tracker area, an independent lane changes stationary pages only when its real read head crosses a viewport page boundary.

## Pattern Jog / strumming

The current APC mappings remain authoritative: `Crossfader=PatternJogAbsolute` and `CueLevel=PatternJogRelative`.

A normal track being strummed keeps the FT2 metaphor: the Pattern Jog row moves the data across the fixed central head. An already-independent track keeps its data stationary and the colored head follows the actual Pattern Jog row during the gesture. A short UI-only activity hold prevents flicker between MIDI messages; it does not delay or change audio.

`[MIDI] PatternJogFastTracks=Ignore|Include` still controls FastTracks participation, not the renderer. `Ignore` leaves excluded FastTracks running on their own private heads. `Include` lets the strum temporarily display the ordinary pattern row that Pattern Jog actually reads on those channels, then the private FastTracks head resumes.

## Editing safety

Hybrid playback rendering never rewrites pattern, song-order, block, or undo data. Recording modes and an active block selection deliberately fall back to the established editor coordinate model so a visual transport transform cannot redirect pattern edits or block operations.
