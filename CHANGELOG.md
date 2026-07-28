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

## Unreleased

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
