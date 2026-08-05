# Sample Map

> **Current CP04.6 workflow.** Sample Map affects tracker composition and sample
> placement; it is separate from Deck Matrix tile assignment.

**Sample Map places recorded audio in tracker space, giving each instrument an origin and every point in its waveform a pattern-and-row address.**

Traditional trackers treat a sample mainly as an isolated recording measured in sample offsets, percentages, and loop points. Sample Map adds another view: the waveform can also be read in the same musical coordinates as the song.

The Sample Editor displays the cursor or selection in tracker coordinates:

```text
P00|00
P00|00 - P01|19
```

These addresses show where that audio falls across the song's patterns and rows at the current playback settings. Sample Map does not alter FT2's ordinary sample playback. It translates sample position into tracker space so the waveform and Pattern Editor can describe the same musical location.

## Map origins

Every instrument stores its own map origin. New instruments begin at `P00|00`, but an origin can be assigned to any current Pattern Editor cursor position.

Right-click the Sample Map readout:

- **Set cursor** — use the current song position and row as the instrument's map origin.
- **Reset P00** — return the origin to `P00|00`.
- **Cancel** — close the menu without changing anything.

Opening the Sample Editor never changes an origin automatically. Think of the origin as the instrument's home address: moving it remaps the waveform without modifying the audio.

## Extract + Stamp

Sample Map can turn a selected region into a newly placed instrument in one operation.

```text
Shift+X       Extract selection to a new instrument
Shift+E       Extract from the sample cursor to the end

Alt+Shift+X   Extract selection, preserve its map position, and stamp C-4
Alt+Shift+E   Extract cursor-to-end, preserve its map position, and stamp C-4
```

The Alt+Shift commands create the instrument, copy the correct map origin, place a C-4 note at its pattern-and-row address, and extend the song far enough for the extracted audio to finish playing. Large expansions ask for confirmation first.

## Why it exists

A map may use any useful unit of measurement. In Sample Map, the units are **pattern and row**. The origin is home, each point in the waveform has an address, a selection marks a region, and Extract + Stamp plots a course through the song.

The guiding principle is simple:

> When musical intent is clear, the software should perform the bookkeeping.

You still decide what to extract and where it belongs. Sample Map removes the repetitive work of calculating offsets, creating instruments, remembering positions, inserting notes, and extending the order list. Recorded audio becomes a navigable compositional object inside tracker time rather than an isolated waveform beside it.
