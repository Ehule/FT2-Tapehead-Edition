## Pattern interpolation v1

- Add `Ctrl+Shift+V` previewable absolute volume interpolation.
- Add `Ctrl+Shift+B` previewable `8xx`/`Cxx` effect interpolation.
- Add `Ctrl+Shift+M` line-step-aware note interpolation.
- Add Freygish and Whole Tone scale presets and reorganize the note preset bank.
- Make generated interpolation notes inherit their endpoint instrument.
- Add live `1`-`0` scale preset auditioning from the original block snapshot.
- Add Enter-to-commit, Escape/unrelated-key cancellation, and a distinct preview selection tint.
- Reject incompatible endpoints or occupied interior target cells without overwriting pattern data.

# Changelog

All notable changes to FT2 Tapehead Edition are documented here.

This project is under active development. Experimental features are identified clearly so that working checkpoints can be preserved before further changes are made.

## Experimental mono hardware routing — Pass 8

- Added a live `Mono` checkbox to Config -> Audio. Stereo routing remains the
  default and can be restored without changing any XM panning data.
- Added a separate mono routing bank: each scope letter represents one physical
  output (`A=1`, `B=2`, `C=3`, `D=4`, etc.) while Mono Out is enabled.
- Initialized mono routes by physical tracker lane, repeating across available
  outputs, so a four-output device begins `1=A`, `2=B`, `3=C`, `4=D`.
- Routed voices from a dedicated mono gain path before FT2 stereo panning. Pan
  commands and envelopes remain stored but cannot leak into adjacent outputs.
- Kept the VST-style persistent output endpoint: switching Mono Out does not
  reopen SDL/JACK or remove live Graph connections.
- Reserved Ctrl+Alt-click in Mono Out for the proposed future per-track stereo
  override. Ordinary Alt-click cycles the available mono destinations.
- Expanded JACK diagnostics to meter physical `A-D` outputs individually in
  Mono Out mode.
- Added exclusive-delivery tests for all four outputs at both `800` and `8FF`,
  alongside the existing stereo A/B/A+B regression coverage.

## Experimental multichannel output — Pass 7 VST-style JACK lifecycle

- Fixed the Pass 6 startup regression where selecting Tapehead JACK Virtual
  Outputs opened the client but never activated its process callback, leaving
  FT2's audio-driven replayer apparently frozen.
- Matched the FT2 plugin's lifecycle model: open/register first, finish mixer
  buffer setup, activate exactly once on the initial resume, and keep the
  client active across later editor pause/resume operations.
- Added a second pause check after the JACK callback acquires the shared mixer
  lock, closing the race between a pending callback and editor changes to
  voice/sample pointers.
- Corrected the native JACK regression test to exercise FT2's real
  open-then-resume path instead of manually activating the fake client.

## Experimental multichannel output — Pass 6 JACK connection persistence

- Kept the native JACK client and all named output ports active while FT2
  pauses audio for instrument, sample, undo, module, and editor operations.
- Replaced JACK client deactivation during those operations with a realtime-safe
  silence flag, so the live QjackCtl Graph connections are not destroyed.
- Waited for any already-running mixer callback before allowing an editor
  operation to alter voice or sample pointers.
- Added a simulated JACK regression test proving pause outputs silence, resume
  restores all four port buffers, and neither action reactivates or deactivates
  the client. Final shutdown remains the only deactivation.

## Experimental multichannel output — Pass 5 live-routing fix

- Confirmed from the X220/JACK hardware trace that channel route masks changed
  correctly (`0x0001` to `0x0002`) while real tracker audio vanished before
  reaching the Bus B JACK port.
- Restored direct mixer-to-bus delivery for every exclusive A, B, or later
  single-bus route, bypassing the neutral scratch-copy stage for the ordinary
  case.
- Retained Pass 3's authoritative per-render JACK bus count, so the stale
  stereo-count failure from Pass 2 cannot fold an exclusive B route back to A.
- Kept the neutral render-and-copy path only for intentional multi-destination
  routes such as A+B (`To Main`), preserving single execution of tracker voice
  and event state.
- Retained the opt-in Pass 4B JACK meters for hardware confirmation without a
  diagnostic test tone.

## Experimental multichannel output — Pass 3

- Repaired the live JACK Bus B silence reproduced with the Echo Indigo DJx.
- Made JACK's configured output-bus count authoritative for each render cycle,
  preventing a stale stereo device count from folding Bus B into Bus A.
- Rendered each physical FT2 channel once into a neutral scratch buffer before
  distributing it to A-only, B-only, or To Main duplicate destinations.
- Added a live mixer delivery regression test covering A, B, and A+B while the
  stored global bus count is deliberately stale.
- Removed the `channel` shadowing warning from the JACK audio callback.

## Experimental multichannel output — Pass 2

- Added a native Linux JACK/PipeWire-JACK backend that dynamically loads the
  installed JACK runtime without requiring JACK development headers.
- Added `Tapehead JACK Virtual Outputs` to the existing Config > Audio output
  device list whenever a compatible JACK runtime is installed.
- Exposed every configured stereo bus as named ports such as `bus_A_L`,
  `bus_A_R`, `bus_B_L`, and `bus_B_R` for REAPER or hardware patching.
- Kept JACK sample rendering planar and 32-bit float while preserving the
  existing SDL interleaved output and safe stereo fold-down paths.
- Added a non-blocking realtime lock: JACK renders silence instead of making
  its process thread wait while Tapehead performs a protected editor change.
- Added a simulated JACK-server test covering runtime loading, port creation,
  four-channel buffer delivery, pause/resume, locking, and shutdown.

## Experimental multichannel output — Pass 1

- Added sixteen fixed-capacity logical stereo buses (`A` through `P`).
- Added `OutputBuses=1-16` under `[Audio]` in `tapehead.ini`.
- Added per-physical-channel output routing with `Alt+Left-click` over scopes.
- Added `Ctrl+Alt+Left-click` “To Main” duplication without replaying tracker
  events or advancing a sample voice twice.
- Added multichannel SDL stream negotiation and 16-bit/float interleaving for
  2 through 32 output channels.
- Added safe stereo fallback/fold-down when the selected device cannot expose
  the requested layout.
- Preserved stereo song-to-WAV rendering and kept routing runtime-only.
- Made Poly Matrix threads inherit the destination tunnel's output assignment.

## RC1 — 2026-07-30

RC1 freezes the tested Structural Checkpoint 01L as the first release-candidate
baseline. It combines the subsystem refactor with the 01A–01L workflow,
input-routing, pattern-context, and VIEW Transpose repairs below.

### Structural checkpoint 01L

- Made manual order scrolling and pattern-number changes atomically synchronize
  the replayer pattern, editor pattern, editor sync shadow, song position, and
  row even while Pattern Play is active.
- Cleared obsolete audio/video pattern-sync entries during those manual
  handoffs, preventing a delayed stock FT2 event from restoring the previous
  Transpose target.
- Preserved FasTracks ratio, private phase, clutch, direction, and transport
  mode across the repaired context handoff.
- Made Song-mode FasTracks columns render the pattern actually read by each
  private head instead of borrowing note data from the master pattern.
- Added the Transpose panel's `VIEW` toggle. In VIEW mode, Transpose snapshots
  the literal rendered slots around each visible track's private head; hidden
  rows and horizontally hidden tracks are unaffected.
- VIEW mode follows Song-mode private source patterns, deduplicates wrapped
  rows, preserves the Current/All Instruments filter, and leaves ordinary
  Track/Pattern/Song/Block Transpose behavior unchanged when disabled.
- Defined VIEW as an instantaneous rendered-slot snapshot. At extreme
  FasTracks ratios, adjacent columns or notes can be sampled from different
  private-head moments; paired notes that began together can therefore be
  transposed differently. This is intentional live “spinning canvas” behavior,
  not a failure of the underlying Transpose operation.

### Structural checkpoint 01K

- Fixed `Ctrl+Grave` falling through to the stock edit-step control when Ctrl
  and Grave arrived during the same SDL event-queue pass.
- Made key-down routing use the modifier mask attached to the exact key event
  instead of the stale once-per-frame modifier snapshot.
- Allowed either Ctrl key to toggle Silent Record while preserving plain Grave
  and `Shift+Grave` spacing and `Ctrl+Shift+R` for FasTracks channel 14.
- Captured the INP button's modifiers on mouse-down, so `Shift+INP` reliably
  duplicates the current pattern instead of occasionally creating an empty
  plain-INP pattern.
- Made Pattern Matrix song-membership and populated/empty color transitions
  update directly at order and pattern mutation boundaries.

### Structural checkpoint 01J

- Kept the position editor, pattern editor, and replayer on the same newly
  inserted pattern when `INP` or `Shift+INP` is used during playback.
- Fixed stock Pattern/Track/Block Transpose appearing to do nothing after a
  playback-time `Shift+INP` duplication because it was modifying the previous
  source pattern instead of the visible copy.
- Preserved stock FT2 current-instrument Transpose behavior for notes whose
  instrument column is `00`; All Instruments Transpose continues to include
  them.

### Structural checkpoint 01I

- Made every global FasTracks `Z20-Z2B` command apply atomically after the
  complete channel pass for the current audio tick.
- Removed channel-order dependence from `Z23`, `Z24`, and `Z2B`, so assigned
  tracks on both sides of a control lane remain exactly synchronized.
- Restored the green phase indicator after `Z24`/`Z2B` by correcting the
  underlying one-tick phase error rather than forcing a visual refresh.

### Structural checkpoint 01H

- Moved the global Silent Record Entry shortcut from unreachable
  `Ctrl+Shift+R` to `Ctrl+Grave` (the physical `` ` / ~ `` key).
- Preserved `Ctrl+Shift+R` as the FasTracks Pattern-transport toggle for
  channel 14.
- Gave `Ctrl+Grave` priority during Melodic Walk preview without changing the
  existing plain Grave and `Shift+Grave` spacing controls.
- Prevented held `Ctrl+Grave` from toggling Silent Record repeatedly through
  keyboard repeat.

### Structural checkpoint 01G

- Restored Sample Editor priority for `Ctrl+Shift+E` and `Ctrl+Shift+X`, which
  were being consumed by the 32-channel FasTracks keyboard map.
- Preserved the same keys as FasTracks channel 13 and 31 toggles whenever the
  Sample Editor is not visible.
- Prevented held extraction chords from filling multiple sample slots through
  keyboard repeat.

### Structural checkpoint 01F

- Added confirmation before `Ctrl`-right-click clears a Matrix pattern's event
  data while preserving its song-order positions.

### Structural checkpoint 01E

- Empty patterns that remain referenced by the song order now use a dimmed
  Pattern Text color in the Pattern Matrix.
- Added `Shift`-right-click with confirmation to delete a Matrix pattern and
  remove all of its song-order references.
- Added `Ctrl`-right-click to clear a pattern's event data while preserving its
  song-order positions as intentional empty sections.
- Both Matrix pattern-management actions are atomic Undo/Redo transactions.

### Structural checkpoint 01A

- Added `Shift`-click on `INP` to insert an independent copy of the current
  pattern, including all pattern data and its exact length, whether IPL is on
  or off.
- Added atomic Undo/Redo for the duplicated pattern and inserted order entry.

### Structural checkpoint 01D

- Pattern Matrix pads now update immediately when an unreferenced pattern gains
  its first event or loses its final event; changing Matrix banks is no longer
  required to refresh populated/empty status.

### Structural checkpoint 01C

- Replaced the subtle song-membership background tint with persistent,
  theme-controlled pattern-number colors: Pattern Text for song-referenced
  patterns and Mouse/channel-header color for populated Matrix-only patterns.

### Structural checkpoint 01

- Extract the FasTracks transport, state, ratios, controls, and audio advance path from `ft2_replayer.c` into a named subsystem.
- Extract the Pattern Matrix transport state and borrowed-surface drawing into dedicated modules.
- Classify populated Pattern Matrix pads by number color: song-referenced patterns use the theme's Pattern Text color, while Matrix-only patterns use its Mouse/channel-header color.
- Give the pattern editor one coherent FasTracks snapshot per redraw instead of assembling diagnostics from separate live getters.
- Isolate the rational FasTracks clock from FT2/SDL dependencies and add native C parity tests for first-tick, high-speed, slow-ratio, remainder, and TPL-change behavior.
- Preserve the frozen `Z` command map, Pattern/Song traversal, reverse, clutch, phase, Matrix queue, and Matrix exit behavior.

### Hardening checkpoint

- Make direct Linux builds compile to a temporary executable and preserve the previous working binary if compilation or asset validation fails.
- Load the undo memory ceiling from the shared executable-adjacent `tapehead.ini`.
- Replace the obsolete eight-track FasTracks prototype notes with a current 32-channel transport manual and frozen `Z` command map.
- Remove obsolete pre-feature source snapshots and channel-trim patch helpers from the source tree.

- Replace the Whole Tone Melodic Walk preset with a Repeat bank that propagates one source note across the selected tracks and rows.
- Warn when Melodic Walk is started with an edit step of zero.

### Fixed — Fast Tracks suspend/resume phase retention

- The global Fast Tracks logo now freezes each selected track's private master-row phase while suspended.
- Resuming no longer reconstructs slower-ratio phase from the current global song row.
- Ratios such as `1:2`, `2:3`, and `3:4` now retain their hidden denominator position as well as their visible source row.
- Shift-click phase reset still deliberately aligns selected Fast Tracks to the current master transport.

### Added — First per-track ratio bank

- Added per-track rational ratio state for the first eight Fast Tracks.
- Added an initial live ratio bank: `1:2`, `2:3`, `3:4`, `1:1`, and `2:1`.
- Added `Alt+Shift+1` through `Alt+Shift+8` to cycle the corresponding track through the ratio bank while preserving its current private source-row phase.
- Replaced the Track 8-only `3:4` special case with shared numerator/denominator lookup used by every Fast Track.
- Updated the pattern status display to show every active ratio as `source rows : master rows`.

### Current timing boundary

- Ratios at or below `1:1` use master-row resolution and correctly hold source rows without retriggering them.
- `2:1` retains the proven half-row tick path.
- Faster non-integer ratios such as `3:2`, `4:3`, and `5:4` still require the planned persistent tick-phase accumulator and are deliberately not faked by this milestone.

### Added — Fast Tracks experimental milestone

- Added independent 2× playback transports for Tracks 1–8.
- Added `Ctrl+Shift+1` through `Ctrl+Shift+8` shortcuts to toggle Fast Tracks independently for the first eight tracks.
- Added an independent scrolling pattern view for every enabled Fast Track.
- Added a compact `2X` indicator and hexadecimal private source-row readout for each enabled Fast Track.
- Added a tiny fixed playhead marker so pattern data appears to rotate past a stationary tape head.
- Added theme-safe coloring for populated Fast Track pattern data using FT2's existing emphasized-row palette color.
- Preserved stock coloring for empty pattern cells, channel headings, and inactive tracks.

### Fast Tracks behavior confirmed

- Fast Tracks share the normal FT2 master transport, pattern order, BPM, and tick timing while reading pattern rows at 2×.
- Each enabled track retains its own private source row and phase.
- Toggling tracks at different moments creates independent phase relationships.
- Disabling a Fast Track returns that track to the master pattern position.
- Up to eight independently scrolling Fast Tracks have been tested simultaneously without renderer failure.
- Pattern note entry remains functional while the independent column view is active. Newly entered data may not be visible immediately because the column is centered on its private playback row; it appears when the rotating view reaches that stored row.
- When playback is stopped, the current experimental view can appear mechanically coupled to the master edit cursor. A future composition-view option is planned to align columns while stopped.

### Global effect discovery

- Effects placed on ordinary tracks continue to execute at the master transport rate.
- Global effects placed on a Fast Track are encountered at that track's faster row-reading rate and can therefore affect the complete song more frequently.
- E6 pattern-loop behavior has been tested: placing E6 on a 2× Fast Track causes its global loop behavior to be encountered at the Fast Track rate, producing a faster rhythmic result.
- This behavior is considered musically useful and is intentionally preserved during the experimental phase.

### Current limitations

- All enabled Fast Tracks currently use a fixed 2× ratio.
- Only Tracks 1–8 can be enabled as Fast Tracks.
- Fast Tracks state and ratios are not yet stored in XM metadata.
- No global logo suspend/resume or phase-reset control is implemented yet.
- Multi-pattern behavior and conflicting global effects require broader testing.
- Fast Tracks-specific behavior is not represented in stock XM playback and will eventually require a baking/export strategy for translation to standard FT2 data.

### Planned next steps

- Add a global Fast Tracks logo control that suspends and resumes the selected Fast Tracks without erasing their phase state.
- Add a phase-reset command that aligns selected Fast Tracks to a common source position.
- Add an all-eight selection command.
- Add per-track ratios such as 1×, 1/2×, 2×, 3×, and 4× after the global state controls are stable.
- Investigate persistent Tapehead metadata while preserving ordinary XM compatibility.
- Test Linux and Windows builds from the same checkpoint.
