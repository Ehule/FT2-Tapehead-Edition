# Changelog

All notable changes to FT2 Tapehead Edition are documented here.

This project is under active development. Experimental features are identified clearly so that working checkpoints can be preserved before further changes are made.

The newest entry describes the current source. Older sections are historical
milestones and may contain limitations or planned work that later entries
supersede. See [`docs/README.md`](docs/README.md) for the current documentation
map.

## Disk Op browse-by-ear sample audition — 2026-08-20

- Sample and Instrument Disk Op modes now keep a persistent highlighted row.
  Unmodified Up/Down moves it and Enter or double-click explicitly opens or
  commits the selection using the current Disk Op mode.
- Preview is enabled by default from a compact switch below the parent/root
  controls. In this safe mode, click and arrow navigation never alter the song;
  recognized samples are connected to QWERTY and MIDI note input instead.
- With Preview disabled, click and arrow navigation live-load recognized samples
  into the current sample slot or one-sample instrument. This supports auditioning
  replacements inside a running pattern, with one undo entry per replacement.
- Preview decoding is asynchronous, mixes stereo files to mono without opening
  import dialogs, and discards superseded work when browsing quickly. Raw or
  otherwise ambiguous files remain loadable but are not auto-auditioned.
- Preview sample memory and its dedicated mixer voice are private runtime state;
  preview notes never reach Edit, Pattern Record, or Song Record and never steal
  a pattern channel. Closing Disk Op restores ordinary keyboard and MIDI routing.
- Ctrl+Up/Down changes the destination sample slot while leaving the highlighted
  browser candidate in place; Shift+Up/Down retains instrument selection.
- Dedicated preview notes now select their sinc interpolation kernel before the
  first mixer callback, fixing the original QWERTY/MIDI audition crash.
- The asynchronous decoder is newest-selection-wins and joinable at shutdown,
  so no preview worker can outlive audio/replayer teardown.
- On Windows, drive buttons begin below Preview and remain compact enough to
  expose all eight supported drive entries.

## Per-track transport visual consistency — 2026-08-20

- FastTracks, LEN and Freeze lanes now always use stationary pattern data with
  their moving private playhead during ordinary playback on every platform.
  Standard lanes retain classic FT2 scrolling. Record modes and block editing
  retain the conventional coordinate view for write safety.
- Removed the machine-local `PerTrackTransportVisuals` override that could make
  Linux and Windows render the same transport state differently.
- During playback the pattern-data body is display-only: mouse buttons cannot
  move the cursor, create or clear highlights, audition rows, or switch the
  transport view. LEN/control and FasTrack channel-header controls remain live.
  When stopped, the original mouse editing and audition gestures are unchanged.

## Dedicated Tuning/Drift column — 2026-08-10

- Added a restricted `Mxx`/`Nxx` lane to internal pattern cells, independently
  preserving MicroTune and MicroDrift beside the ordinary XM effect.
- Delayed `EDx` notes now defer their associated tuning instruction until the
  actual trigger tick.
- Added legacy effect promotion and a signed, versioned Tapehead XM extension;
  stock XM packing remains strictly five fields and standard effects win any
  lossy export collision.

## Transport Punch + manual one-shot strumming — 2026-08-09

- Added APC footswitch Transport Punch with configurable Sustain/Cut,
  Toggle/Hold, and Silent/Audition frozen-navigation behavior.
- Frozen transport stops ordinary, Matrix, and FastTracks scheduling without
  pausing the mixer; manual strumming remains active and resume honors whether
  the destination row was already auditioned.
- Fixed the automatic display queue repainting its frozen row over manual jog
  and order-navigation positions. Added `TransportFreezeResume=Next|Retrigger`
  so the original freeze row can either be consumed or deliberately replayed.
- Changed Momentary strumming into a natural forward one-shot and added
  direction-sensitive ManualPingPong one-shots. Both override sample loops.
- Shift + APC Up/Down now jumps through song-order positions by the current edit
  step, including repeated occurrences of the same pattern.
- Remapped APC Record Arm 1–8 to the corresponding per-track FastTracks clutch;
  shifted presses remain unassigned.

## CP04.6 — 2026-08-04 — Fast Tracks tick-resolution baker

Frozen as tag `cp04.6` on the `baker-experimental` branch.

- Fixed Fast Bake working only for synchronized 1:1 Fast Tracks. When any
  Fast Track is configured, active, or introduced later by Zxx, the destination
  now advances once per replayer tick
  instead of once per master row, so fractional, mixed and 5:1 crossings are
  written to distinct ordinary XM rows rather than rejected as sub-row events.
- Live Bake always uses the same tick-resolution timeline because ratios can be
  changed after recording is armed.
- Tick-resolution output uses TPL 1. Source TPL commands are resolved into the
  expanded timeline and stripped; BPM changes remain in the standard XM.
- The established row-resolution path remains unchanged for ordinary Fast Bake
  with Fast Tracks disabled.
- Validated with synchronized `1:1`, synchronized `5:1`, mixed-ratio live
  performance, and a collision-heavy stress module. The stress bake expanded
  beyond its source channel count and compacted successfully.
- Added native timeline coverage to the baker allocator suite. See
  [`docs/COMPOSITION_BAKER.md`](docs/COMPOSITION_BAKER.md) for current use and
  output rules.

## CP04.5 — Fast Tracks 32-channel bake allocator

- Replaced the baker's first same-cell failure with a 32-channel spill
  allocator. A Fast Tracks stream stays on its original XM channel when
  possible, claims a spare channel when the destination cell is occupied, and
  reuses its owned spill channels on later rows.
- Added **Merge exact duplicate voices** to the Shift+Save Bake window. It is
  enabled by default and applies to both Fast Bake and Live Bake.
- Exact duplicates from the same logical Fast Tracks stream can collapse at
  the identical XM instant. Events from distinct source tracks are merged only
  when their completed baked channel trajectories are byte-for-byte identical.
- Added a final compaction pass that removes empty spill channels, packs the
  used channels contiguously, and saves the smallest channel count required by
  the realized performance.
- Bake completion now reports relocated and merged event counts. A collision
  is reported only after no safe channel remains in XM's 32-channel pool.
- Added standalone native allocator regression coverage and included the new
  source/header in the Visual Studio project and filters.

## CP04.4b — Baker resolved-command correction

- Fixed Fast Bake and Live Bake falsely rejecting a performance when a private
  Fast Track encountered a Tapehead Z command or already-resolved flow command
  between master rows.
- After the command has changed the Fast Tracks transport and is stripped from
  the conventional XM cell, the baker now discards the resulting empty cell
  instead of counting it as an unsupported sub-row event.
- Genuine sub-row notes/effects and same-cell event collisions remain subject
  to the existing lossless-first checks.

## CP04.4 — Live Fast Tracks baking

- Expanded the **Shift + module Save** Bake window with **Fast Bake**, **Live**,
  and **Cancel** choices.
- Added an armed live composition recorder: choose **Live**, press **Play Song**,
  adjust Fast Tracks ratios, clutch, reverse, sync, and other controls while the
  source song loops, then press the ordinary **Stop** button to write the
  realized performance as a standard XM.
- Live baking uses audible real-time playback and captures the resolved tracker
  event stream before mixing. Repeated source patterns therefore become new
  linear destination patterns whose contents reflect each loop's performed
  Fast Tracks state.
- Kept MIDI Dub and Sample Deck ticking audible during a live bake; only the
  existing silent Fast Bake suppresses external/live output.
- Preallocates live capture pattern memory before playback so the audio callback
  never needs to allocate while performing.
- Shortens the final destination pattern when Stop occurs between 64-row
  boundaries, so the baked song ends at the performed stopping row instead of
  adding a padded silent tail.
- Pressing **Stop** before **Play Song** cancels an armed live bake without
  writing a file. Pattern-play and record transports are rejected while the
  live song baker is armed.
- Preserved CP04.3's lossless-first collision/sub-row checks and XM 256-pattern
  ceiling.

## CP04.3 — Composition Baker milestone 1

- Added **Shift + Save** for modules as the entry point to a dedicated
  **Bake Module** confirmation window.
- Added a silent, faster-than-real-time composition pass through the actual
  replayer, including Fast Tracks private heads, ratios, reverse, clutch state,
  and pattern Z commands.
- Added resolved event capture before mixing and conventional 64-row XM pattern
  generation. Tapehead Z commands and already-resolved source flow commands are
  removed from the flattened copy.
- Added exact Fast Tracks runtime snapshot/restore, including fractional phase,
  so baking does not alter the loaded composition or its performance state.
- Baked files omit Tapehead Sample Matrix metadata and default to a
  `-BAKED.xm` filename.
- Added conservative collision reporting: no XM is written when this milestone
  cannot represent simultaneous or sub-row events without musical loss.
- Composition baking currently requires Song, Q, Poly, and Sample Deck playback
  to be stopped. Live performance capture remains a later layer over this core.

## Pattern interpolation v1

- Add `Ctrl+Shift+V` previewable absolute volume interpolation.
- Add `Ctrl+Shift+B` previewable `8xx`/`Cxx` effect interpolation.
- Add `Ctrl+Shift+M` line-step-aware note interpolation.
- Add Freygish and Whole Tone scale presets and reorganize the note preset bank.
- Make generated interpolation notes inherit their endpoint instrument.
- Add live `1`-`0` scale preset auditioning from the original block snapshot.
- Add Enter-to-commit, Escape/unrelated-key cancellation, and a distinct preview selection tint.
- Reject incompatible endpoints or occupied interior target cells without overwriting pattern data.

## Deck Matrix CP04.2 — Sample Matrix Editor interaction consistency

- Added the Deck Matrix momentary alternate-color/inset feedback to every
  bottom Sample Matrix Editor action, including the brief **DONE** acknowledgement
  before returning to performance mode.
- Added three-row mouse-wheel scrolling when the pointer is over the DISK file
  list and prevented full-window Matrix wheel events from reaching the hidden
  tracker underneath.
- Made the Sample Matrix Editor initialize from FT2's Sample Disk Op path only
  on its first opening. Later trips between the editor and Deck Matrix retain
  and rescan the last browser directory used during that program run.

## Deck Matrix CP04.1 — Sample bank reuse and stopped-transport queue

- Renamed the Sample Matrix Editor's clipped **REFRESH** button to **UPDATE**.
- Made **CLEAR BNK** detach the backing `SBxxA/B` instruments without deleting
  their native samples. The samples remain in FT2 while the cleared Matrix
  tiles become immediately reusable by later disk fills.
- Added an independent Sample Deck boundary clock for stopped native
  transport. The first sample starts immediately, later Q clicks retain the
  four-item queue, and Q/Poly starts and stops commit at the current
  pattern-length boundary using the current BPM/TPL.
- Kept Sample Q synchronized to real tracker boundaries whenever Song or
  Pattern playback is running.

## Deck Matrix CP04 — Sample Matrix Editor

- Added the full-window **EDIT SMP** mode while keeping the 32-tile Sample
  Matrix and all eight Sample banks visible.
- Added a native sample-file browser with folder navigation, natural sorting,
  single selection, `Ctrl` individual selection, and `Shift` range selection.
- Added explicit **DISK** and **MODULE** source modes. Disk files are imported
  once into ordinary native FT2 Sample Bank instruments; Module samples are
  assigned by `{instrument, sample}` reference without duplicating audio.
- Added visible **IMPORT ONE**, **FILL SEL**, **FILL DIR**, **ASSIGN**, **ASSIGN
  ALL**, **UNASSIGN**, **DELETE**, **CLEAR BNK**, and **DONE** actions.
- Made bulk disk fills start at the selected tile, skip occupied tiles, cross
  later banks, preflight instrument capacity, and report imported/omitted
  counts.
- Made `UNASSIGN` and `CLEAR BNK` non-destructive. **DELETE** is the separate
  confirmed operation that removes the native sample and affects all tiles
  referencing it.
- Added a compact Tapehead metadata block after the standard XM payload so
  arbitrary Module references and explicit empty tiles survive XM save/reload
  while the XM remains playable in ordinary compatible trackers.
- Kept the CP03.3 modifier gestures as optional power-user shortcuts and fixed
  the `%02X` status-buffer compiler warnings.

## Deck Matrix CP03.3 — direct sample placement and command cheatsheet

- Added `Ctrl+right-click` on a Sample Deck tile to copy the currently selected
  native FT2 sample into that exact tile.
- Preserved complete sample audio and metadata, including name, tuning, loop,
  volume, and panning; replacement keeps the tile's existing output route.
- Automatically creates and tags only the required `SBxxA/B` backing
  instrument when a destination bank half does not exist.
- Added occupied-tile confirmation, non-modal success/error feedback, safe Deck
  voice shutdown before memory replacement, and Undo/Redo sample capture.
- Reworked `docs/DECK_MATRIX_CP03.md` into the complete Deck Matrix command
  cheatsheet, including Q/Poly boundary, handoff, cancellation, hard-stop,
  Sample Bank, and song-transport behavior.

## Deck Matrix CP03.2 — universal tile hard stop

- Added `Ctrl+Alt+left-click` as an immediate tile-local hard stop across Q and
  Poly without replacing the established yellow/orange/red boundary gestures.
- Made the gesture cancel numbered waiting Q entries and pending Poly starts;
  an inactive tile is unchanged and cannot be masked accidentally.
- Kept Pattern and Sample decks independent. Killing active Pattern Q clears
  its Q queue and immediately resumes an interrupted native song.

## Deck Matrix CP03.1 — transport feedback and extended bank import

- Shortened `PLAY PATT` to `PLAY PAT` and added momentary alternate-color,
  inset-bevel feedback to the order arrows and five transport buttons.
- Made the `Oxx Pxx` display follow the audible order and pattern during native
  Song playback.
- Made every numbered waiting Pattern Q tile directly cancelable instead of
  limiting queue undo to the newest entry. The active tile keeps its existing
  yellow quantized boundary-exit behavior.
- Extended Sample Matrix folder imports across successive banks, allocating one
  standard FT2 instrument per 16 naturally sorted samples instead of omitting
  files after sample 32.
- Added atomic destination preflight, complete-range replacement confirmation,
  and a capacity warning when the selected bank leaves too few of the 256
  Matrix tiles for the entire folder.

## Deck Matrix CP03 — native Sample Banks and tracker transport

- Renamed the full-window surface to **Deck Matrix** and removed the permanent
  click-instruction legends.
- Added eight Sample bank selectors mirroring the Pattern bank row. Sample Q
  and Poly state now keeps unique identities across all 256 tile positions.
- Replaced the private decoded Sample cache with native FT2 instrument/sample
  references. Each 32-tile bank is stored as two tagged 16-sample instruments.
- Connected Matrix tiles to Sample Editor selection and native sample or
  paired-bank deletion.
- Added upper-right Pattern/Sample Q and Poly counters plus non-modal lane and
  queue warnings.
- Removed repeated song-membership `S` labels and added distinct empty,
  Matrix-only, song-used, and masked Pattern tile treatments.
- Added song-order navigation and `PLAY SNG`, `PLAY PATT`, `STOP SNG`, `STOP
  DECK`, `STOP ALL`, and `TRACKER` controls.
- Made Space inside Deck Matrix stop only the native tracker transport while
  leaving independently latched deck voices alone.
- Preserved MIDI Dub mapping, multichannel routing, and Q/Poly isolation.

## Launcher CP02.2 — restored configurable MIDI Dub track routing

- Restored the `[MIDIDub]` section from the validated HDV3 MIDIMap checkpoint,
  with explicit `Track01` through `Track32` outgoing MIDI channel assignments.
- Preserved the default mapping: tracks 1-16 use MIDI channels 1-16, and tracks
  17-32 repeat channels 1-16.
- Applied the configured mapping consistently to note-on, note-off, timed gate
  release, and MIDI panic behavior without changing launcher or audio routing.
- Added the MIDI configuration regression test to the complete native suite.

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
