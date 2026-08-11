# FT2 Tapehead Edition review instructions

## Reviewer role

Review for concrete behavioral defects, memory-safety problems, state-machine
regressions, resource-lifetime errors, and compatibility breaks. Prefer a small
number of high-confidence findings over generic C advice.

For every finding, identify the executable path or user action that reaches the
problem and explain the observable result. If hardware behavior cannot be
proven statically, label the assumption instead of presenting it as fact.

## Project baseline

FT2 Tapehead Edition is an experimental performance fork of 8bitbubsy's
FT2 Clone. The integrated default branch is the behavioral baseline. Tapehead
adds optional systems around the old tracker; it does not replace the native
tracker.

When Tapehead features are disabled or idle, ordinary FT2 Clone playback,
editing, saving/loading, keyboard and mouse input, audio, and UI behavior must
remain unchanged unless the PR explicitly documents an intentional change.

This is deliberately old-style C built on a hardcoded legacy architecture.
Shared globals, compact functions, fixed-size tables, and direct state access
are not findings by themselves. Report them only when the changed code creates
a demonstrated correctness, safety, lifetime, or real-time problem. Do not ask
for broad rewrites unrelated to the PR.

## Sources of truth

Use this order when sources disagree:

1. The PR's stated and accepted behavior.
2. Current code and regression tests.
3. Current subsystem documentation referenced by `.greptile/files.json`.
4. Historical checkpoint documents.

Several documents retain CP or branch names for history. Treat stale wording as
a documentation issue, not as authority over newer tested behavior.

## High-risk invariants

### Indices, limits, and ownership

- Check every conversion between one-based UI/INI values and zero-based C
  storage.
- Check tracks/channels, patterns, rows, song positions, instruments, samples,
  MIDI channels, matrix banks/slots, output buses, and baker spill channels
  against their actual limits.
- Follow allocation, copy, replacement, unload, undo, bake, and error paths.
  Samples and instruments must not be leaked, double-freed, aliased after
  replacement, or partially committed.
- Reject silent truncation, wrapping, overwriting, or musically different
  fallback behavior when an operation cannot be represented.

### Real-time audio and shared state

- Audio callback and replayer paths must not allocate/free memory, perform file
  or device I/O, log, wait, or take a lock that can block.
- UI/MIDI-owned changes observed by audio code must use the project's existing
  synchronization, snapshot, or ownership model. Look for torn multi-field
  transitions and use-after-free during module/device replacement.
- Do not propose a conventional mutex in the audio callback as a casual fix.

### Transport state machines

Trace changes across all affected transport owners:

- native Song and Pattern playback;
- per-track FastTracks private playheads, ratios, direction, clutch, and mode;
- Pattern Matrix Q and Poly activity;
- Sample Matrix scheduling;
- pattern jog/strum while stopped or playing;
- Transport Punch freeze/resume;
- Composition Baker capture.

Check forward and reverse movement, pattern boundaries, repeated order entries,
stop versus freeze, clutch transitions, queued work, song/pattern modes,
displayed versus sounding rows, and whether a row is consumed or retriggered on
resume. One transport must not accidentally advance, stop, or steal state from
another.

### Composition Baker and XM compatibility

- Preserve the resolved musical event stream: note, instrument/sample identity,
  volume, effects, timing, direction, Sample Morph selection, microtonal state,
  mute/solo cuts, and supported Matrix/FastTracks performance.
- Track/master gain and hardware routing are live mix state and are excluded
  unless a PR explicitly changes that contract.
- Respect the 32-channel allocator, 256-pattern XM limit, TPL-1 expansion, note
  delay representability, and collision rules.
- On overflow, allocation failure, unrepresentable timing, or empty capture,
  write no output file and preserve the source module.
- Standard output must remain a conventional XM. Tapehead output may preserve
  documented extensions without breaking ordinary XM readers.

### Sample Morph and microtonal behavior

- Sample Morph selects among populated samples without rewriting the source
  instrument or changing the played note/pitch. Baker-created private
  instruments must own their copied samples and should be deduplicated only
  when playback semantics are identical.
- The dedicated Tuning/Drift lane accepts only `Mxx` and `Nxx`. Verify
  precedence when those commands also occur in the ordinary effect field,
  persistence across rows, strum behavior, baking, undo, and save/load.

### MIDI/APC40 lifecycle

- Surface input/output is separate from the musical keyboard and MIDI Dub.
  Failure or ambiguity in one port must not disable the others.
- Validate message length, status/channel, controller/note number, mapping
  arguments, selected track/bank/slot, and device availability before access.
- Preserve relative-encoder direction and every detent where documented.
- Opening, switching, module loading, profile disabling, and exit must leave
  devices and LEDs in a clean state.

### Output and platform behavior

- Preserve stereo fallback when JACK or multichannel SDL output is unavailable.
- Keep logical stereo buses, mono-output routing, track trim/mute, and physical
  channel counts distinct.
- Watch for Linux-only assumptions entering Windows builds, or JACK-only
  assumptions entering SDL/stereo paths.
- Changes to device buffers, channel layouts, or callbacks require matching
  bounds and teardown handling.

### Undo, save, and documentation

- A user-visible edit should be one coherent undo transaction, restore all
  affected data on redo, clear redo after divergent edits, update the modified
  state, and preserve selection/cursor state where documented.
- Config defaults, accepted ranges, UI controls, and
  `release/other/tapehead.ini` must agree.
- When a PR changes documented controls or behavior, request a focused
  documentation update.

## Validation expectations

The integrated native acceptance command is:

```bash
python3 scripts/test_all_native.py
```

Require or suggest the narrowest relevant test as well. Do not claim that a
static or native test proves APC40 feel, real MIDI timing, JACK routing, SDL
hardware behavior, or audible output; those still require explicit hardware
testing.
