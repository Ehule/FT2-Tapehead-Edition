# Composition Baker CP04.5

> **Historical checkpoint.** CP04.5 introduced the allocator retained by
> CP04.6, but predates the tick-resolution timeline. Use
> [`COMPOSITION_BAKER.md`](COMPOSITION_BAKER.md) for current behavior.

CP04.5 gives Fast Tracks baking a 32-channel allocation and cleanup pass. Fast
Bake and Live Bake still capture the resolved event stream before mixing; the
difference is that an occupied destination cell no longer ends the bake while
XM still has a safe spare channel.

## Bake window

Hold **Shift** while clicking module **Save**. The Bake Module window now has:

- **Fast Bake** — silent CPU-speed one-pass composition bake
- **Live** — audible looping performance capture, completed by Stop
- **Merge exact duplicate voices** — enabled by default
- **Cancel**

The duplicate choice is runtime-only and remains at its most recent setting
while Tapehead is open.

## Channel allocation

Each source tracker channel begins with its matching XM output channel. When a
resolved Fast Tracks event reaches an already occupied cell, the baker:

1. Reuses another spill channel already owned by that logical source stream if
   its current cell is free.
2. Otherwise claims an unused channel from XM's 32-channel pool.
3. Keeps the logical stream bound to that spill channel on later rows whenever
   possible.
4. Reports a collision only if no channel can represent the event.

Instrument numbers and all five XM cell fields travel with the relocated event;
instruments themselves are not duplicated.

## Duplicate policy

With **Merge exact duplicate voices** enabled:

- Repeated copies from the same logical Fast Tracks stream merge only when the
  complete XM cell and its resolved row/tick timing are identical.
- Separate source tracks are not merged because one coincident note matches.
  They merge only if their completed baked channel trajectories are identical
  for the whole performance.

With the option disabled, exact duplicates are preserved and allocated like
other collisions. This intentionally retains possible gain, phase, envelope,
and accidental-noise effects.

## Final compaction

Allocation may temporarily use any of the 32 XM channels. Before saving, the
baker removes unused spill channels, moves all used channels into one contiguous
block, and sets the XM channel count to the smallest value required by the
captured result.

The completion message reports:

- Events relocated to spill channels
- Exact duplicate events merged

## Remaining lossless limits

The existing sub-row rules remain. A single note can use XM `EDx` note delay;
effects or timing beyond XM's delay range can still be reported as unsupported.
An event is still rejected if all 32 output channels are unavailable. The baker
does not silently discard either case.
