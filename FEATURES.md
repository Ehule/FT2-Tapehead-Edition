# FT2 Tapehead Edition — CP04.6 Feature Reference

> **Compose like a tracker. Think like a tape machine.**

FT2 Tapehead Edition is an experimental, performance-oriented fork of [ft2-clone](https://github.com/8bitbubsy/ft2-clone). It preserves the familiar FastTracker II workflow while adding new tools for live manipulation, nonlinear composition, MIDI integration, sample navigation, and faster pattern editing.

This reference describes the `baker-experimental` branch at the frozen
`cp04.6` tag. RC1 is the older release-candidate baseline; CP04.6 adds the Deck
Matrix, Sample Matrix Editor, expanded routing experiments, and the first
proven tick-resolution Composition Baker.

## What this fork adds

### Deck Matrix

- Full-window Pattern and Sample decks sharing XM BPM/TPL and launch boundaries
- Eight 32-tile Sample banks backed by paired native 16-sample XM instruments
- Independent Sample Q voice plus four Sample Poly voices across all banks
- Native Sample Editor selection, replacement, naming, saving, and deletion
- Direct selected-sample placement on any tile with `Ctrl+right-click`
- Full Sample Matrix Editor with visible Disk import and zero-copy Module assignment
- Multi-file and whole-folder filling across open Sample tiles and bank pages
- Persistent arbitrary `{instrument, sample}` tile references in Tapehead-saved XMs
- Non-destructive Pattern exposure mask and four-state pattern classification
- Per-Sample multichannel destination cycling
- Song-order navigation with separate song, deck, and global stop controls
- Optional direct Deck Matrix startup through `tapehead.ini`

See [`docs/DECK_MATRIX.md`](docs/DECK_MATRIX.md) for current controls and native
Sample Bank behavior.

### Composition Baker

- Opened by holding **Shift** while clicking module **Save**
- **Fast Bake** performs one silent CPU-speed pass from order 0
- **Live** records audible repeated song loops until ordinary **Stop**
- Captures resolved Fast Tracks Pattern/Song heads, ratios, direction, clutch,
  synchronization, master state, and pattern-programmed `Zxx` changes
- Uses one conventional XM row per replayer tick whenever Fast Tracks timing
  is active or may be introduced by `Zxx`
- Live Bake always uses the tick-resolution path so controls can change after
  capture is armed
- Tick-resolution output is saved at TPL 1; source timing commands are
  realized and removed while BPM changes remain
- Uses XM's 32-channel pool for collisions and reuses spill channels by source
  stream
- Optional exact-duplicate merging, enabled by default
- Removes unused spill channels and packs the final channel layout
- Preserves standard instruments and sample data without duplication
- Refuses to write when the performance cannot be represented losslessly or
  exceeds XM's 256-pattern ceiling

See [`docs/COMPOSITION_BAKER.md`](docs/COMPOSITION_BAKER.md) for the complete
workflow, output rules, and current capture boundary.

### Fast Tracks

Fast Tracks gives each tracker channel its own pattern-row transport while retaining FT2's shared master clock.

- Independent transport ratios across all 32 XM channels
- Slower, faster, and near-unity rational ratio bank
- Free-running or deliberately resynchronized phase relationships
- Per-track source-row and phase state
- Transport preservation during live ratio changes
- Momentary per-track clutch for slipping channels out of sync
- Latched global clutch for performance use
- Ratio randomization
- Ratio randomization with immediate resynchronization
- Independent scrolling pattern columns
- Per-track ratio, row, and phase indicators
- Song-wide per-track playback lengths (`LEN`) through all private song orders
- LEN can extend a shorter source pattern with blank rows through row 256
- One optional song-wide `CONTROL` track for the Pattern/Song cycle boundary
- Theme-aware event and clutch highlighting
- Fast Tracks timing integrated with playback and offline rendering

#### Fast Tracks controls

The 32 channel keys are:

```text
Channels  1–10: 1 2 3 4 5 6 7 8 9 0
Channels 11–20: Q W E R T Y U I O P
Channels 21–29: A S D F G H J K L
Channels 30–32: Z X C
```

- `Ctrl+Shift+Track Key` — Toggle Fast Tracks for that channel
- `Alt+Shift+Track Key` — Cycle that channel's ratio
- Hold `Ctrl+Alt+Track Key` — Momentary per-track clutch
- `Ctrl+Alt+Plus` — Toggle the global latched clutch
- Click Fast Tracks logo — Suspend or resume Fast Tracks globally
- `Ctrl+Click` Fast Tracks logo — Randomize active ratios
- `Ctrl+Shift+Click` Fast Tracks logo — Randomize and resynchronize
- `Right-click` Fast Tracks logo — Return assigned transports to 1:1 while preserving phase
- `Ctrl+Right-click` Fast Tracks logo — Return assigned transports to 1:1 and synchronize
- `Right-click` an assigned FastTracks header (the conditional lower strip) — Reverse that private transport
- `Shift+Right-click` an assigned FastTracks header — Toggle Pattern/Song transport
- `Mouse Wheel` over the persistent upper channel header — Adjust that song-wide track `LEN`
- `Shift+Mouse Wheel` over the upper header — Adjust `LEN` in steps of eight
- `Ctrl+Mouse Wheel` over the upper header — Reset `LEN` to `OFF`
- Hold APC40 Shift + Track Control encoder 1–8 — Set the corresponding LEN;
  flashing bar rings expose the shifted hardware layer
- Click the eject-like button at the right of the upper header — Select that track as `CONTROL`; click
  the red active icon again to return to ordinary FT2 pattern boundaries
- Config panel `FT uses LEN` — When checked (the default), private FastTracks
  transports wrap inside `LEN`; when unchecked, FastTracks uses the complete
  source-pattern length without changing standard, Q, or Poly LEN behavior

`LEN OFF` follows the effective pattern domain. Numeric lengths wrap only that
track's resolved source row, including Fast Tracks ratio, reverse, clutch, Q
routing, and Poly playback. The longest LEN can extend a shorter source with
safe blank rows; an explicit shorter LEN still loops inside a longer pattern.
A `CONTROL` track persists through the order list and changes when the overall
pattern/order boundary occurs; it does not force other tracks onto its row.
Tapehead XM saves retain this metadata, while standard XM export omits it and
ordinary XMs load with every `LEN` off and no `CONTROL` track.

The LEN header is always visible. While FastTracks is enabled, its ratio,
reverse, phase, and Song `order>pattern` diagnostics occupy a separate strip
directly below it. Rows outside an active FastTracks LEN remain visible but
dimmed instead of being repeated through the short loop. FastTracks source
patterns remain stationary while their independent playhead outlines move
through the rows. When a CONTROL track is assigned, its local phase also owns
the tracker's central scrolling row. By default, cyan per-track cell
outlines show LEN playheads, red identifies the CONTROL playhead, and amber
identifies a private FastTracks playhead that is using the full-pattern domain.
Those three head colors plus FastTracks sync, phase, and Song-badge colors are
editable in the scrollable Layout palette.


### Pattern-programmable transport (`Zxx`)

Tapehead Edition uses the XM **Z** effect for persistent, pattern-programmable FasTracks transport control. The compatibility-frozen command table, exact ratio mapping, and behavior notes are maintained in [docs/FAST_TRACKS.md](docs/FAST_TRACKS.md#pattern-programmable-commands).

### MIDI Dub output

- MIDI output support
- Dedicated virtual output named `FT2 Tapehead MIDI Dub`
- Selection and persistence of external MIDI output devices
- Replayer-driven MIDI note output during song playback
- Per-track outgoing channel assignments through `[MIDIDub]` in `tapehead.ini`
- Tracks 1-16 default to MIDI channels 1-16; tracks 17-32 repeat that mapping
- Any tracker tracks may be manually assigned to the same MIDI channel
- MIDI note-on and note-off handling
- Velocity derived from tracker playback state
- MIDI panic / all-notes-off support
- Feedback protection against selecting the virtual output as its own input

### Per-channel performance mixing

- Per-track output trim without changing pattern data
- `Ctrl+Mouse Wheel` over a scope — Adjust output trim
- `Ctrl+Left-click` a scope — Reset trim to 100%
- Non-destructive performance mute
- `Shift+Left-click` a scope — Toggle performance mute
- Performance-muted tracks continue running internally so envelopes, loops,
  effects, timing, and MIDI Dub output remain active. This can silence FT2's
  samples while an external destination such as SunVox remains audible.
- Stock channel mute stops normal channel processing and further MIDI Dub note
  triggers as well as FT2 sample playback.
- Short gain ramps reduce clicks when muting
- Red scope overlay shows performance-mute state

### Multichannel and mono output

- Up to sixteen logical stereo buses (`A` through `P`)
- Configurable exposed bus count through `[Audio] OutputBuses` in
  `tapehead.ini`
- `Alt+Left-click` a scope — Cycle that physical FT2 channel's output bus
- `Ctrl+Alt+Left-click` a scope — Toggle an additional feed to Bus A
- Bus markers appear at the lower-right corner of scopes
- Poly Matrix threads inherit the output assignment of their destination tunnel
- Native Linux `Tapehead JACK Virtual Outputs` device exposes every bus as a
  named stereo port pair for REAPER, PipeWire graphs, or direct hardware routing
- Multichannel-capable SDL devices remain supported without JACK
- Stereo-only devices fold every logical bus safely to Bus A
- Optional Mono Outputs mode routes each tracker lane to one physical output
  before FT2 stereo panning; the live Config checkbox and `tapehead.ini` setting
  control the same runtime mode
- Setup and current behavior are documented in
  [`docs/MULTICHANNEL_OUTPUT.md`](docs/MULTICHANNEL_OUTPUT.md)

### Pattern Editor audition

- Middle-click a pattern cell — Audition that cell's note
- `Shift+Middle-click` a row — Audition every playable note across that row
- Auditioned notes stop when the middle mouse button is released

### Pattern Editor navigation

- Configurable vertical navigation at pattern boundaries
- Wrap within the current pattern
- Stop at the beginning or end of the pattern
- Continue into the previous or next order-list position
- Configurable Page Up / Page Down behavior
- `Ctrl+Shift+Space` — Open the pattern-navigation popup
- Improved cursor visibility
- Theme-aware breathing cursor animation

### Pattern interpolation and Melodic Walk

- `Ctrl+Shift+V` — Preview volume-column interpolation
- `Ctrl+Shift+B` — Preview compatible effect interpolation
- `Ctrl+Shift+M` — Generate a previewable Melodic Walk
- Single-anchor open-ended melodic generation
- Two-anchor walks that preserve explicit endpoints
- Multiple selected tracks can use independent anchors
- Scale-aware note generation
- Chromatic, major, natural minor, pentatonic, modal, Freygish, and harmonic-minor scales
- Repeat bank fills a one-note selection across rows and multiple tracks at the current spacing
- A zero edit step displays a warning instead of failing silently
- Press the active scale number again to reverse an open-ended walk
- Grave / tilde and `Shift+Grave` — Adjust preview spacing
- Playback and Fast Tracks controls remain usable during preview
- `Enter` — Commit preview
- `Escape` — Cancel and restore the original pattern
- Preview data uses a temporary visual tint

### Transpose and VIEW painting

- Manual order scrolling and pattern-number changes remain the immediate
  Transpose target during Pattern Play; delayed playback synchronization can no
  longer redirect edits into a previous pattern.
- The Transpose panel includes an optional `VIEW` checkbox.
- With `VIEW` off, Track, Pattern, Song, and Block Transpose retain their normal
  complete-data scopes.
- With `VIEW` on, Transpose changes only event slots currently rendered in the
  pattern editor. Each visible FasTracks channel follows its own private row
  window, including its private source pattern in Song transport.
- Horizontally hidden tracks and rows outside the current editor window remain
  unchanged, making high ratios usable as a live “spinning canvas.”
- Pattern and Song buttons both address all visible canvas tracks in VIEW mode;
  Track addresses the cursor track and Block intersects the visible canvas with
  the current block selection.
- A VIEW click snapshots the rendered slots at that instant. At extreme ratios,
  adjacent notes can belong to different private-head moments, so notes that
  began in unison may be captured and transposed differently. The resulting
  divergence is the intended “painting on a spinning canvas” behavior.

### Silent Record, IPL, INP, and REC+

- Silent Record Entry allows recording or editing without auditioning every entered note
- `Ctrl+Grave` (either Ctrl plus the physical `` ` / ~ `` key) toggles Silent Record globally,
  including during Melodic Walk preview
- Inherit Pattern Length (`IPL`) lets new patterns inherit the previous pattern's length
- Insert New Pattern (`INP`) creates a fresh pattern instead of duplicating the current one
- `Shift`-clicking `INP` creates a new independent copy of the current pattern,
  including its exact pattern length, regardless of the IPL setting
- Automatic Pattern Generation / `REC+` can extend the song while recording reaches its end
- REC+ tracks whether new material was generated during the take
- REC+ includes exhaustion handling and a hidden game-over state
- Pattern Data Zap clears musical data while preserving pattern allocation, lengths, and song structure

### Sample extraction workflow

- `Shift+X` — Extract the selected sample range to a new instrument
- `Shift+E` — Extract from the sample cursor to the end to a new instrument
- `Ctrl+Shift+X` — Extract the selection into a new sample slot in the current instrument
- `Ctrl+Shift+E` — Extract cursor-to-end into a new sample slot in the current instrument
- Automatically finds appropriate destination instruments or sample slots
- Preserves relevant sample playback properties
- Translates loop points into the extracted sample's coordinates
- Generates collision-safe names
- Refreshes the instrument and sample interface after extraction

### Sample Map

Sample Map translates waveform positions into pattern-and-row addresses, allowing recorded audio to be navigated and placed in tracker space.

- Pattern/row address display such as `P00|00`
- Pattern/row selection-range display
- Independent stored map origin for each instrument
- Set an origin from the current song position and Pattern Editor row
- Reset an origin to `P00|00`
- Opening the Sample Editor does not overwrite the stored origin
- `Alt+Shift+X` — Extract a selection, create an instrument, and stamp `C-4` at its mapped address
- `Alt+Shift+E` — Extract cursor-to-end, create an instrument, and stamp `C-4`
- Extract + Stamp preserves the mapped origin in the destination instrument
- The order list can be extended automatically so extracted audio has room to finish
- Large automatic expansions request confirmation

See [`docs/SAMPLE_MAP.md`](docs/SAMPLE_MAP.md) for the workflow in detail.

### Graphics, configuration, and portability

- Tapehead Edition logo and badge artwork
- Theme-aware Fast Tracks logo rendering
- Runtime custom BMP logo loading
- Embedded fallback logo assets
- Palette-sensitive UI overlays
- Scope graphics for trim and performance mute
- Portable configuration stored beside the executable
- Linux and Windows build integration for new modules

## Stock ft2-clone comparison

| Capability | Stock ft2-clone | Tapehead Edition |
|---|:---:|:---:|
| Classic FT2-compatible tracker workflow | Yes | Yes |
| Per-track alternate transports | — | Yes |
| 32-channel transport ratios | — | Yes |
| Per-track clutch and phase performance | — | Yes |
| Ratio randomization | — | Yes |
| MIDI playback output / MIDI Dub | — | Yes |
| Per-track output trim | — | Yes |
| Non-destructive performance mute | — | Yes |
| Middle-click cell audition | — | Yes |
| Shift-middle-click row audition | — | Yes |
| Configurable boundary navigation | — | Yes |
| Pattern interpolation previews | — | Yes |
| Scale-aware Melodic Walk generation | — | Yes |
| Silent Record Entry | — | Yes |
| Inherit Pattern Length | — | Yes |
| Insert New Pattern | — | Yes |
| Automatic Pattern Generation / REC+ | — | Yes |
| Pattern Data Zap | — | Yes |
| Sample extraction shortcuts | — | Yes |
| Pattern-addressed Sample Map | — | Yes |
| Extract + Stamp workflow | — | Yes |
| Pattern Q plus four Pattern Poly spools | — | Yes |
| Sample Q plus four Sample Poly voices | — | Yes |
| Visual Sample Matrix import/assignment editor | — | Yes |
| Fast Tracks performance-to-XM baking | — | Yes |
| Native JACK/PipeWire-JACK output buses | — | Yes |
| Per-track mono hardware routing | — | Yes |
| Runtime custom logo loading | — | Yes |
| Executable-adjacent portable configuration | — | Yes |

## Testing status

CP04.6 is a proven development checkpoint, not a final release. It is suited to
musicians, tracker users, and developers who are comfortable testing an active
fork while keeping important modules backed up.

Please expect:

- experimental behavior at extreme Fast Tracks ratios or timing settings;
- experimental interaction boundaries between the ordinary tracker, Deck
  Matrix, and live baker;
- features that may be refined after real-world use;
- standard XM files to remain editable, while runtime-only Tapehead behavior
  must be baked when stock XM playback needs to reproduce it.

Useful testing areas include:

- Linux and Windows builds
- audio devices and unusual buffer configurations
- MIDI hardware and virtual MIDI routing
- Fast Tracks across varied BPM, TPL, ratio, and pattern-length combinations
- long Sample Map and REC+ sessions
- live use of trim, mute, clutch, and ratio changes
- old or unusual XM modules
- Fast Bake and Live Bake with mixed ratios, pattern `Zxx`, repeated loops, and
  high event density

When reporting a problem, include your operating system, build method, audio/MIDI setup, module or reproduction steps, and the settings needed to trigger it.

## Documentation

- [`docs/README.md`](docs/README.md)
- [`docs/COMPOSITION_BAKER.md`](docs/COMPOSITION_BAKER.md)
- [`docs/DECK_MATRIX.md`](docs/DECK_MATRIX.md)
- [`docs/FAST_TRACKS.md`](docs/FAST_TRACKS.md)
- [`docs/MULTICHANNEL_OUTPUT.md`](docs/MULTICHANNEL_OUTPUT.md)
- [`docs/CONFIGURATION.md`](docs/CONFIGURATION.md)
- [`docs/PATTERN_INTERPOLATION.md`](docs/PATTERN_INTERPOLATION.md)
- [`docs/SAMPLE_MAP.md`](docs/SAMPLE_MAP.md)
- [`docs/RC1_RELEASE_NOTES.md`](docs/RC1_RELEASE_NOTES.md) — historical RC1 baseline
- [`CHANGELOG.md`](CHANGELOG.md)
- [`HOW-TO-COMPILE.txt`](HOW-TO-COMPILE.txt)

## Project philosophy

Every Tapehead addition is intended to remain optional. Disable the new systems and the tracker should retain the speed, structure, and familiarity of classic FastTracker II.

Enable them, and FT2 becomes something stranger: a tracker that can drift, clutch, map, improvise, transmit, and perform.

> **It's almost like the software has learned to dream the song instead of speaking it.**

## Credits

- Original FastTracker II by Triton Productions
- ft2-clone by 8bitbubsy
- Tapehead Edition concept, project direction, and testing by Ehule
- Development, debugging, patch-generation, and documentation assistance with OpenAI ChatGPT
