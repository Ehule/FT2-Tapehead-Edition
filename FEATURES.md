# FT2 Tapehead Edition — Open Testing

> **Compose like a tracker. Think like a tape machine.**

FT2 Tapehead Edition is an experimental, performance-oriented fork of [ft2-clone](https://github.com/8bitbubsy/ft2-clone). It preserves the familiar FastTracker II workflow while adding new tools for live manipulation, nonlinear composition, MIDI integration, sample navigation, and faster pattern editing.

The project is now open for early testing. It is usable, actively developed, and capable of making real music, but some features remain experimental and documentation is still being assembled.

## What this fork adds

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
- `Right-click` Fast Tracks logo — Return assigned transports to 1:1 while preserving phase (`Alt+Click` is also supported where the window manager does not intercept it)
- `Ctrl+Alt+Click` Fast Tracks logo — Return assigned transports to 1:1 and synchronize


## Pattern Effect Commands (Zxx)

Tapehead Edition introduces a new family of pattern-programmable transport effects
using the previously unused XM **Z** effect.

### Per-Track

| Command | Description |
|---------|-------------|
| Z00-Z0F | Select FastTracks transport ratio |
| Z10 | Release track clutch |
| Z11 | Engage track clutch |
| Z12 | Disable FastTracks on this track |
| Z13 | Enable this track in Pattern mode |
| Z14 | Sync this track to the master |
| Z15 | Select Pattern transport |
| Z16 | Select Song transport |
| Z17 | Set this track to 1:1, preserving phase |
| Z18 | Set this track to 1:1 and sync |

### Global

| Command | Description |
|---------|-------------|
| Z20 | FastTracks OFF |
| Z21 | FastTracks ON |
| Z22 | Randomize all transport ratios |
| Z23 | Synchronize all transports |
| Z24 | Reset all ratios to 1:1 and synchronize |
| Z25 | Global clutch OFF |
| Z26 | Global clutch ON |
| Z27 | Reset all ratios to 1:1 while preserving phase |
| Z28 | Select Pattern transport on all assigned tracks |
| Z29 | Select Song transport on all assigned tracks |
| Z2A | Set all assigned tracks to 1:1, preserving phase |
| Z2B | Set all assigned tracks to 1:1 and synchronize |

These commands modify transport state rather than audio processing, allowing tempo
relationships, synchronization, and drift to become programmable compositional
elements.

### MIDI Dub output

- MIDI output support
- Dedicated virtual output named `FT2 Tapehead MIDI Dub`
- Selection and persistence of external MIDI output devices
- Replayer-driven MIDI note output during song playback
- One MIDI channel corresponding to each tracker channel
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
- Muted tracks continue running internally so envelopes, loops, effects, and timing remain synchronized
- Short gain ramps reduce clicks when muting
- Red scope overlay shows performance-mute state

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
- Chromatic, major, natural minor, pentatonic, modal, Freygish, harmonic minor, and whole-tone scales
- Press the active scale number again to reverse an open-ended walk
- Grave / tilde and `Shift+Grave` — Adjust preview spacing
- Playback and Fast Tracks controls remain usable during preview
- `Enter` — Commit preview
- `Escape` — Cancel and restore the original pattern
- Preview data uses a temporary visual tint

### Silent Record, IPL, INP, and REC+

- Silent Record Entry allows recording or editing without auditioning every entered note
- Inherit Pattern Length (`IPL`) lets new patterns inherit the previous pattern's length
- Insert New Pattern (`INP`) creates a fresh pattern instead of duplicating the current one
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
| Runtime custom logo loading | — | Yes |
| Executable-adjacent portable configuration | — | Yes |

## Testing status

Tapehead Edition is currently suited to curious musicians, tracker users, and developers who are comfortable testing an active fork.

Please expect:

- experimental behavior at extreme Fast Tracks ratios or timing settings;
- incomplete or changing keyboard documentation;
- features that may be refined after real-world use;
- standard XM files to remain editable, while some live Tapehead behaviors cannot be reproduced by stock XM players unless rendered or translated.

Useful testing areas include:

- Linux and Windows builds
- audio devices and unusual buffer configurations
- MIDI hardware and virtual MIDI routing
- Fast Tracks across varied BPM, TPL, ratio, and pattern-length combinations
- long Sample Map and REC+ sessions
- live use of trim, mute, clutch, and ratio changes
- old or unusual XM modules

When reporting a problem, include your operating system, build method, audio/MIDI setup, module or reproduction steps, and the settings needed to trigger it.

## Documentation

- [`docs/FAST_TRACKS.md`](docs/FAST_TRACKS.md)
- [`docs/PATTERN_INTERPOLATION.md`](docs/PATTERN_INTERPOLATION.md)
- [`docs/SAMPLE_MAP.md`](docs/SAMPLE_MAP.md)
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
