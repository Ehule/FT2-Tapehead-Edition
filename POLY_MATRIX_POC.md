# Poly Matrix Automatic Loom and Handoff Prototype

This RC1 experiment tests the automatic loom: a pattern supplies a bundle of
musical threads, and the router sends each thread through an available
physical channel ("tunnel").

## Automatic source bundle

Open the Pattern Matrix and middle-click a populated pattern tile. Cursor
position is no longer involved.

Every populated source track in that pattern becomes a thread. Note,
instrument, volume-column, and effect-only tracks all count as populated. A
pattern may supply up to eight threads, and up to four Poly patterns may be
active at once.

Each thread first tries the same-numbered physical track as its tunnel. If that
tunnel already belongs to another Poly spool, routing wraps forward to the next
free physical track. The complete bundle is routed atomically: if all of its
threads cannot be placed, none of them starts.

The source supplies the pattern events. The destination tunnel retains mixer
trim, performance mute, output assignment, and its FasTracks ratio, direction,
and clutch behavior.

## Poly gestures

- Middle-click a free populated tile: start its automatic bundle at row `00`.
- Middle-click its cyan tile again: let every thread finish its current
  revolution, then pull the whole bundle together.
- Shift+Middle-click its cyan tile: pull the whole bundle immediately.
- Ctrl+Shift+Left-click its cyan tile: arm Poly -> Q handoff.

Each active Poly tile carries a tiny `1` through `4` in launch-slot order. The
number remains attached while the tile overlaps Q or waits for a handoff. When
a spool is removed, its number becomes available to the next Poly throw.

Poly -> Q waits until every thread has completed its current revolution. If Q
is already running, the bundle holds at that group boundary until Q's next
safe pattern boundary, then Q takes the clicked pattern at row `00`. If Q is
stopped, Q starts as soon as the Poly bundle reaches its boundary.

Ordinary left-click remains independent and never alters Poly state.

## Q queue markers

The currently playing Q tile keeps its breathing green or semantic exit color.
Waiting Q tiles carry a tiny `1` through `4` on the right edge, showing their
next-up order directly instead of relying only on the theme-derived gradient.
The left edge remains reserved for the stable Poly spool number, so a tile can
show both states at once without ambiguity.

## Q -> Poly gesture

Middle-click the currently playing Q tile to arm Q -> Poly handoff. Q finishes
its current loop, the pattern's populated tracks are routed into Poly at row
`00`, and:

- the next queued Q pattern becomes foreground, if one exists; or
- the ordinary Q transport stops while Poly continues.

Middle-clicking a queued-but-not-yet-playing Q tile still performs a normal
Poly throw because that tile does not yet have a running Q loop boundary.

If a tile is already active in both Q and Poly, middle-click retains the
established Poly pull gesture. The Q color keeps a cyan foot to show the
simultaneous Poly layer.

## Clock and ownership

Poly owns its playback clock. Throwing a spool while FT2 is stopped does not
start Pattern Play, advance the editor row, or read from the pattern displayed
in the editor. Pattern/Song Play and Q can start and stop independently.

A destination channel is exclusively owned by its Poly thread until release,
temporarily replacing ordinary pattern feed on that channel. Graceful,
immediate, and handoff releases all send clean tunnel note-offs.

Pattern-position effects (`Bxx`, `Dxx`, `E6x`, and `EEx`) are isolated from
Poly playback and cannot steer the ordinary Pattern/Song transport. `Fxx`
remains shared because BPM/TPL is the common performance clock.

The complete state is runtime-only. Explicit global Stop or module load clears
both layers, and XM pattern data is never altered.

## Suggested test

1. Create a pattern with data on several tracks, including one effect-only
   control track.
2. Middle-click it without moving the tracker cursor and confirm that all
   populated tracks speak through their routed tunnels.
3. Prepare several destination tracks with visibly different FasTracks ratios
   and throw additional patterns to force routing contention.
4. Ctrl+Shift+Left-click a cyan tile and confirm that its entire bundle reaches
   Q cleanly.
5. Middle-click the active Q tile and confirm that it returns to Poly after the
   Q loop, while the next queued Q item takes foreground if present.
