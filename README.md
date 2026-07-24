
# FT2-Tapehead Edition

> **Compose like a tracker. Think like a tape machine.**

**FT2-Tapehead Edition** is a workflow-focused fork of **FT2 Clone** by **8bitbubsy**, inspired by the original **FastTracker II** by Triton Productions.

It reimagines tracker composition through the lens of tape-machine recording while preserving the classic FastTracker II interface and philosophy.


The goal is not to redesign FastTracker II---it is to preserve its
speed, familiarity, and philosophy while removing repetitive actions
that interrupt creative flow.



## Features

### Silent Record

Record into patterns without hearing every newly entered note, reducing
loop fatigue during composition.

### Inherit Pattern Length (IPL)

Automatically inherit the previous pattern's length when creating new
patterns, making mixed-length song structures effortless.

### Insert New Pattern (INP)

Insert a brand-new pattern instead of duplicating the current one.

### Automatic Pattern Generation (APG / REC+)

While recording in Song Record mode, **REC+** automatically extends the
song by creating new patterns as playback reaches the end.

This transforms FT2 from a pattern editor into a tape-inspired
composition environment while preserving its classic workflow.

### Pattern Data Zap (PatData)

Erase all musical note data while preserving:

-   Pattern lengths
-   Song order
-   Pattern allocation
-   Overall song structure

Think of it as erasing the tape while leaving the reels intact.

### REC+ Interface

When APG is enabled, the normal Song Record button becomes **REC+**,
clearly indicating the enhanced recording mode.

### Hidden Easter Egg

If you genuinely fill every available pattern through REC+ recording,
FT2-Tapehead Edition rewards you with a hidden **GAME OVER** screen.

Subsequent attempts display **REC+ FULL**.

## Sample Map

Sample Map places recorded audio in tracker space. The Sample Editor can display waveform positions as pattern-and-row addresses, while each instrument stores its own map origin. **Alt+Shift+X** extracts a selection and stamps it into the song; **Alt+Shift+E** does the same from the sample cursor to the end. The song is extended automatically so the extracted audio can finish playing. See [`docs/SAMPLE_MAP.md`](docs/SAMPLE_MAP.md) for the complete workflow and shortcuts.

## Fast Tracks — Experimental

Fast Tracks gives the first eight channels independent pattern-row transports while retaining one shared FT2 master clock. Each enabled channel currently reads the active pattern at **2×**, producing independently phased rhythmic and melodic cycles.

Use:

```text
Ctrl+Shift+1 through Ctrl+Shift+8
```

to toggle Fast Tracks for Tracks 1–8. The modifier keys can be held while rolling across the number keys, making it easy to engage or disengage several tracks quickly.

Use **Alt+Shift+1** through **Alt+Shift+8** to cycle a selected track through the first rational ratio bank: `1:2`, `2:3`, `3:4`, `1:1`, and `2:1`.

Each active Fast Track displays:

- an independently scrolling pattern column;
- a tiny fixed playhead resembling a tape head or mechanical scale pointer;
- a `2X` transport label;
- a hexadecimal private source-row readout;
- theme-safe alternate coloring on populated pattern events.

Fast Tracks can be activated at different moments, creating independent phase relationships. Turning an individual track off returns it to the master transport. Global effects placed on a Fast Track are encountered at that track's faster row-reading rate while still affecting the complete song; E6 pattern loops have been confirmed to produce musically useful accelerated global behavior.

Fast Tracks remains experimental. Ratios other than 2×, global logo controls, persistent module metadata, multi-pattern validation, and stock-XM baking are planned. See [`docs/FAST_TRACKS.md`](docs/FAST_TRACKS.md) for detailed behavior and test notes, and [`CHANGELOG.md`](CHANGELOG.md) for the current milestone.

## Philosophy

Every addition in FT2-Tapehead Edition is optional.

Disable the new features and the tracker behaves like the standard
FastTracker II Clone.

Enable them, and the tracker becomes a fast, improvisational composition
tool inspired by tape-machine workflows rather than repetitive pattern
editing.

> Spend less time managing patterns and more time making music.

## Roadmap

Planned ideas include:

-   Per-channel output trim / mixer view
-   Additional tape-inspired workflow improvements
-   Optional UI quality-of-life enhancements
-   More hidden easter eggs

------------------------------------------------------------------------

FT2-Tapehead Edition is developed with respect for the original
FastTracker II workflow and the FT2 Clone project. The goal is evolution
through optional workflow enhancements---not replacing what made FT2
great.

## Building

FT2-Tapehead Edition can be built on both Linux and Windows using the included build script.

### Linux

Install the required development packages for your distribution, then run:

```bash
./make-linux.sh
```

The compiled executable will be placed in:

```text
release/other/ft2-clone
```

### Windows

FT2-Tapehead Edition uses the included Visual Studio solution for Windows builds.

1. Install **Visual Studio 2026 Community**.
2. Install the **Desktop development with C++** workload.
3. Open `vs2026_project/ft2-clone.slnx`.
4. Select the **Release** configuration.
5. Select the **x64** platform.
6. Build the solution.

If Visual Studio reports that the Windows SDK is missing, change the project to use the SDK version installed on your system.

The compiled executable will be generated in the Visual Studio output directory.

## Acknowledgements

- Original **FastTracker II** by **Triton Productions**
- **FT2 Clone** by **8bitbubsy**
- **FT2-Tapehead Edition** design, testing, and project direction by **Ehule**
- Development assistance, code-patch generation, debugging support, and documentation assistance provided with **OpenAI ChatGPT**

FT2-Tapehead Edition builds upon FT2 Clone with optional workflow enhancements focused on improvisation, continuous recording, and tape-inspired composition.

## Tape Head Edition Features

This fork adds optional live-performance workflow improvements while remaining fully compatible with standard FastTracker II XM files.

### Performance Mute

**Shift + Left Click** on a scope to toggle **Performance Mute**.

Unlike the original FT2 mute, Performance Mute does **not** stop the channel internally. Playback continues silently so loops, envelopes, LFOs, effects, and timing remain perfectly synchronized.

Performance Mute is displayed with a **red mute indicator**.

### Per-Track Output Trim

Hold **Ctrl** and use the **mouse wheel** over a scope to adjust that track's output trim.

This functions as a simple gain control for live mixing without modifying any pattern data.

**Ctrl + Left Click** resets the trim level back to **100%**.

### Design Philosophy

All live-performance features are designed so that existing FastTracker II modules remain fully compatible with stock FT2. These controls affect only live playback and monitoring—they do not modify the XM file format or alter saved song data.

<!-- TAPEHEAD-ADDENDUM-START -->

---

# FT2 Tape Head Edition Addendum

FT2 Tape Head Edition is a preservation-minded fork of FastTracker II Clone. Its additions are intended to extend the original tracker workflow without preventing songs from being opened, edited, or translated back to stock FastTracker II conventions.

The guiding principle is simple:

> Add faster and more expressive ways to work while keeping the underlying tracker recognizable, portable, and compatible.

## Tape Head Edition features

### Silent Record Entry

Notes and events can be entered while recording without automatically auditioning the entered note.

This supports quiet editing, live playback preparation, and workflows where entering data should not interrupt or layer over the currently playing material.

### REC+ indicator

A dedicated visual recording indicator distinguishes the enhanced Tape Head recording behavior from ordinary editing and playback states.

### Inherit Pattern Length

Newly inserted patterns can inherit the length of the current pattern instead of always reverting to the stock default length.

This is useful when constructing songs from groups of patterns that share unusual or extended lengths.

### Performance Mute

Tracks can be muted as a reversible performance control without rewriting or deleting their pattern contents.

These mute states remain editing and playback metadata rather than destructive changes to the musical data.

### Per-track output trim

Each track can carry a simple output-level trim.

The trim is designed as a non-destructive playback control. It avoids the need to edit every individual volume event merely to balance a track during composition or performance.

### Play Range — Shift+S

When a range is selected in the sample editor, **Shift+S** plays the selected portion directly.

### Play Display — Shift+D

**Shift+D** plays the sample area currently visible in the sample editor.

Together, Play Range and Play Display make navigating and auditioning long recordings substantially faster.

### Extract to Instrument — Shift+X

When a sample range is selected, **Shift+X** copies that range into a new instrument.

The extraction process:

- keeps the original instrument selected;
- preserves the visible sample range and editing context;
- copies the selected audio into a genuinely new instrument;
- preserves applicable sample settings and loop information;
- removes filename extensions from generated instrument names;
- creates sequential names such as `Recording-01`, `Recording-02`, and so on;
- fills suitable empty instrument slots before appending after occupied groups;
- updates the instrument list immediately, including across instrument banks;
- reports when no free instrument slots remain.

This supports a tape-like workflow in which one long master recording can be divided into many playable instruments without repeatedly saving, loading, renaming, or changing focus.

### Clear Instrument — Shift+Right-click

Hold **Shift** and right-click an occupied instrument slot to clear that specific instrument.

The command operates on the instrument underneath the mouse pointer, not necessarily the currently selected instrument.

Behavior:

- a confirmation dialog is always shown before deletion;
- empty instrument slots are ignored;
- the clicked instrument and all of its samples, envelopes, settings, and name are removed;
- clearing another instrument does not change the current instrument;
- the current sample selection and editor view remain undisturbed when another instrument is cleared;
- ordinary right-click renaming and left-click selection continue to work normally.

This is intended for precise cleanup of individual extracted instruments. FT2's existing Trim and Zap operations remain more appropriate for bulk cleanup.

## Extract-and-refine workflow

The new sample tools are designed to work together:

1. Load or record a long master sample.
2. Navigate to a useful section.
3. Use **Shift+D** to audition the displayed region.
4. Select a precise range.
5. Use **Shift+S** to audition the selection.
6. Press **Shift+X** to extract it into a new instrument.
7. Continue working on the master without changing instruments.
8. Shift+right-click any unwanted extracted instrument and confirm its removal.

This creates a fast, non-disruptive workflow resembling work with tape, hardware samplers, and dedicated phrase workstations.

## Compatibility philosophy

Tape Head Edition should preserve ordinary FT2/XM musical data wherever practical.

Features that exist only as playback or editing conveniences should remain non-destructive. Material created in Tape Head Edition should be straightforward to prepare for stock FT2 by baking or translating enhanced behavior when necessary.

The project does not aim to replace FastTracker II with a modern DAW. It aims to explore how far the original tracker design can be extended while retaining its speed, character, limitations, and historical identity.

<!-- TAPEHEAD-ADDENDUM-END -->
