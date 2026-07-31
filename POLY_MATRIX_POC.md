# Poly Matrix

Poly Matrix is an experimental post-RC1 performance layer inside the Pattern
Matrix. It treats a pattern as a spool of musical threads and automatically
routes those threads through available physical channels, or "tunnels."

Poly Matrix is runtime-only and non-destructive. It does not rewrite pattern
data or add Tapehead-specific metadata to an XM file.

## Loom vocabulary

| Term | Meaning |
|---|---|
| Spool | One middle-clicked pattern and all of its populated source tracks |
| Thread | One populated source track within that pattern |
| Tunnel | The physical FT2 channel through which a thread is played |
| Q | The ordinary left-click Pattern Matrix foreground and queue transport |

The source thread supplies notes, instruments, volume-column data, and effects.
The destination tunnel supplies its mixer trim, performance mute, output
assignment, and FasTracks ratio, direction, and clutch behavior.

## Automatic routing

Open the Pattern Matrix and middle-click a populated pattern tile. The tracker
cursor position is not involved.

Every populated source track in the pattern becomes a thread. Tracks containing
only instrument, volume-column, or effect data still count. One spool may
contain up to eight threads, and up to four spools may be active at once.

Each thread first requests the same-numbered physical channel. If that tunnel
already belongs to another Poly spool, the router searches forward and wraps
around for the next free channel. This makes prepared FasTracks channels act
like differently configured tunnels while preserving an element of routing
chance.

Routing is atomic. Every thread must receive a tunnel or the spool does not
start. Empty patterns, patterns with more than eight populated tracks, a fifth
simultaneous spool, and bundles without enough free tunnels are rejected
without changing the active performance.

## Mouse controls

| Gesture | Result |
|---|---|
| Middle-click a free populated tile | Start its complete Poly bundle at row `00` |
| Middle-click an active cyan Poly tile | Arm graceful removal after every thread completes its current revolution |
| `Shift+Middle-click` an active cyan Poly tile | Pull the complete bundle immediately |
| `Ctrl+Shift+Left-click` an active cyan Poly tile | Arm a seamless Poly-to-Q handoff |
| Left-click any tile | Use Q normally without altering Poly |
| Middle-click the currently playing Q tile | Arm a seamless Q-to-Poly handoff |

Ordinary left-click never stops, restarts, transfers, or resynchronizes an
active Poly spool. A pattern may therefore be active in Q and Poly at the same
time.

## Visual state

Poly and Q use opposite tile edges so both layers remain legible together:

| Marker | Meaning |
|---|---|
| Left `1`-`4` | Stable Poly spool slot |
| Right `1`-`4` | Waiting Q position; `1` is next |
| Breathing green tile | Q is playing now |
| Cyan tile | Poly spool is active |
| Dark cyan tile | Graceful Poly removal is pending |
| Green tile with cyan foot | The same pattern is active in Q and Poly |

Poly numbers remain attached to their spools instead of being renumbered during
a performance. When a spool is removed, its freed number is reused by the next
throw. Q numbers update as the queue advances. The currently playing Q tile is
not numbered on the right because its green state already means "now."

## Poly-to-Q handoff

`Ctrl+Shift+Left-click` an active Poly tile to arm a transfer. Each thread
finishes its current revolution, and faster threads wait at their own
boundaries until the complete bundle is ready.

If Q is stopped, Q starts the transferred pattern at row `00` as soon as the
bundle boundary is complete. If Q is already running, the Poly bundle waits for
Q's next safe pattern boundary and then becomes the foreground Q pattern.

The handoff is atomic: the final Poly release cannot suppress Q's first row-00
event.

## Q-to-Poly handoff

Middle-click the currently playing Q tile to arm the reverse transfer. Q
finishes its current loop, then the pattern's populated tracks are routed into
Poly at row `00`.

- If Q has another queued pattern, that pattern becomes the foreground cue.
- If the Q queue is empty, Q stops while Poly continues.

Middle-clicking a waiting Q tile performs a normal Poly throw because that
pattern does not yet have an active Q loop boundary. If a tile is already
active in both layers, middle-click retains the normal Poly removal gesture.

## Clock, ownership, and effects

Poly uses the shared audio tick, BPM, and TPL but owns independent pattern rows
and transport lifecycle. Starting Poly while FT2 is stopped does not start
Pattern Play, advance the editor row, or read from the pattern displayed in the
editor. Pattern Play, Song Play, and Q can start and stop without resetting
Poly phase.

A tunnel is exclusively owned by its Poly thread until release. Ordinary Q,
Pattern, or Song data for that physical channel is temporarily suppressed,
while ordinary events on unclaimed channels continue normally. Graceful,
immediate, and handoff releases send a clean note-off to every released tunnel.

Poly follows a tunnel's FasTracks ratio and reverse direction when FasTracks is
enabled and unclutched there. A standard or clutched tunnel runs forward at
`1:1`.

Pattern-position effects `Bxx`, `Dxx`, `E6x`, and `EEx` are isolated from Poly
playback and cannot steer the ordinary Pattern/Song transport. `Fxx` remains
shared because tempo is part of the common performance clock.

Explicit global Stop and module loading clear the Poly runtime state.

## Suggested test

1. Create a pattern with data on several tracks, including an effect-only
   control track.
2. Middle-click it without moving the tracker cursor and confirm that every
   populated track is routed.
3. Prepare several destination tracks with visibly different FasTracks ratios
   and throw additional patterns to force routing contention.
4. Queue ordinary Q patterns and confirm that they continue on unclaimed
   channels without altering the Poly spools.
5. `Ctrl+Shift+Left-click` a cyan tile and confirm that the complete bundle
   reaches Q at a clean boundary.
6. Middle-click the active Q tile and confirm that it reaches Poly after the Q
   loop while the next queued Q item, if any, takes the foreground.
