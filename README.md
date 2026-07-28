# FT2 Tapehead Edition

> **Compose like a tracker. Think like a tape machine.**

**FT2 Tapehead Edition** is a performance-oriented fork of **FT2 Clone** by **8bitbubsy**, inspired by the original **FastTracker II** by Triton Productions.

Rather than reinventing FastTracker II, Tapehead Edition expands it with optional tools for live performance, experimental composition, faster editing, and modern workflow improvements—while preserving the speed, look, and philosophy that made the original tracker timeless.

---

# Project Status

**Status:** Open Alpha / Public Testing

Tapehead Edition is under active development and is stable enough for everyday composition and public testing on Linux and Windows.

The project has grown well beyond a collection of patches and now includes transport experimentation, composition assistants, performance controls, MIDI integration, and numerous workflow enhancements.

If you're new to the project, start with:

- FEATURES.md — complete feature overview
- CHANGELOG.md — development history
- docs/FAST_TRACKS.md
- docs/SAMPLE_MAP.md
- docs/PATTERN_INTERPOLATION.md

---

# Highlights

Major additions over stock ft2-clone include:

- Fast Tracks alternate transport engine
- 32-track Fast Tracks control
- Multiple transport ratios
- Dirty Sync / Clean Sync workflow
- Clutch performance controls
- Ratio randomizer
- MIDI Dub / MIDI Output
- Silent Record
- REC+ automatic song expansion
- Inherit Pattern Length (IPL)
- Insert New Pattern (INP)
- Sample Map navigation system
- Pattern Interpolation
- Melodic Walk generator
- Pattern Navigation popup
- Configurable vertical cursor navigation
- Middle-click audition tools
- Per-track output trim
- Performance mute
- Runtime logo loading
- Portable configuration support

See **FEATURES.md** for a complete list.

---

# Philosophy

The goal is not to redesign FastTracker II.

The goal is to preserve its speed, familiarity, and philosophy while removing repetitive actions that interrupt creative flow.

Every major addition is designed to remain optional. A long-time FT2 user should still feel at home, while adventurous users can explore entirely new ways of composing and performing.

---

# Feature Overview

## Recording & Composition

- Silent Record
- REC+
- IPL
- INP
- Pattern Data Zap
- Pattern Interpolation
- Melodic Walk

## Performance

### Fast Tracks

Fast Tracks introduces an alternate transport system where individual tracker channels can run at different rhythmic ratios while remaining synchronized to the song.

Current capabilities include:

- Up to 32 independently controlled Fast Tracks
- Multiple musical ratio selections
- Dirty Sync and Clean Sync workflows
- Momentary and latched clutch controls
- Ratio randomization
- One-click global synchronization
- Visual transport feedback

Designed for evolving polyrhythms, phase relationships, and live performance without changing the familiar FT2 editing workflow.


## Pattern-Programmable FastTracks

Tapehead Edition extends the previously unused XM **Z** effect into a programmable
transport system.

FastTracks commands control
the transport driving each pattern track. This allows transport behavior itself to
become part of the composition.

Current capabilities include:

- Per-track transport ratio selection
- Per-track clutch engage/release
- Global FastTracks on/off
- Global ratio randomization
- Global synchronization
- Global transmission clutch
- Reset all ratios to 1:1 (with or without phase preservation)

Because the XM Z effect is ignored by standard FastTracker-compatible players,
Tapehead Edition modules remain fully compatible with the XM format. Other players
simply preserve the Z commands while ignoring their playback behavior.

## MIDI

MIDI Dub allows tracker playback to be transmitted as live MIDI data for driving external synthesizers, DAWs, or modular software while composing inside FT2.

## Editing

Additional workflow improvements include:

- Sample Map
- Middle-click audition
- Pattern Navigation popup
- Configurable cursor navigation
- Numerous keyboard shortcuts and quality-of-life improvements

---

# Building

Tapehead Edition builds using the same general process as ft2-clone.

See the original project documentation for platform-specific build instructions.

---

# Credits

Tapehead Edition is built upon the outstanding work of **8bitbubsy's FT2 Clone**, itself a faithful recreation of Triton Productions' FastTracker II.

This fork exists out of respect for the original tracker and a desire to explore new creative workflows without losing the character that made FT2 special.

Happy tracking.

## Undo / Redo

Tapehead Edition provides a bounded module-edit history:

- **Alt+Backspace** — Undo
- **Shift+Alt+Backspace** — Redo
- Up to **128 transactions**
- **32 MB** default history ceiling
- Memory is allocated only when edits are recorded; 32 MB is not reserved at startup
- When the ceiling is reached, the oldest transactions are discarded first
- A new edit after undo clears the redo branch

The memory ceiling can be changed in `tapehead.ini`, located beside the program:

```ini
[Undo]
UndoMemoryMB=32
```

Accepted values are 4–1024 MB. The default is intentionally conservative for older systems such as ThinkPad X40-class hardware. Pattern operations are very small; large destructive sample or instrument edits consume the history more quickly.

Current undo transactions include destructive pattern insert/delete operations, track/pattern/block cut and paste, committed interpolation and Melodic Walk previews, sample-editor right-button drawing (one mouse stroke per transaction), internal sample/instrument replacement, and sample or instrument overwrites loaded from disk. Instrument undo also restores the slot name.
