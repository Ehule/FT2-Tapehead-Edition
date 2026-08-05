# FasTracks

> **Current CP04.6 reference.** This document contains the compatibility-frozen
> `Zxx` command map. For translation into stock XM playback, see
> [`COMPOSITION_BAKER.md`](COMPOSITION_BAKER.md).

FasTracks gives every XM channel a private row-reading transport while retaining FastTracker II's shared audio clock. The master still owns BPM, ticks per line, playback timing, and the ordinary order list. Each assigned track adds its own ratio, phase, source position, direction, mode, and clutch state.

The result behaves less like 32 unrelated sequencers than 32 tape heads sharing one motor.

FasTracks is a runtime interpretation layer. It does not change the XM pattern format, and the transport setup is not written into XM metadata.

## Transport model

Each of the 32 channels can be in one of three modes:

- **Standard** — read the ordinary master pattern and row.
- **Pattern** — keep a private row that wraps inside the pattern selected by the master order.
- **Song** — keep a private order and row that traverse the complete order list independently.

All private transports use the same rational tick accumulator, including `1:1`. Changing ratio preserves the track's private source position and normalized sub-row phase. Direction changes also preserve phase.

Song transport continues along its private order list while the Pattern Matrix changes the pattern heard by the master. This separation is intentional: the Matrix can perform one layer while Song-mode FasTracks freewheel underneath it.

The pattern editor renders each Song-mode channel from that private order's
actual source pattern. The Transpose panel's optional `VIEW` mode can therefore
modify the events visibly passing under each private head without touching
rows or tracks outside the current editor window.

## What is heard, seen, and edited

Classic FT2 usually lets three related identities appear to be one:

- the pattern and row currently heard by the replayer;
- the pattern rows currently drawn in the editor;
- the pattern object targeted by an edit command.

RC1 makes ordinary navigation publish one atomic pattern context, so normal
Transpose can no longer lag behind Pattern Play and silently modify a previous
pattern. FasTracks still separates those identities intentionally when a
private head is active. Song-mode columns draw from each private head's actual
source pattern, and VIEW Transpose explicitly edits a snapshot of those drawn
event slots.

This is FT2's useful “hear no evil, see no evil” quality made explicit: ordinary
editing keeps the identities together, while FasTracks exposes their creative
separation only when the user asks for it.

At extreme ratios such as `5:1`, the display is a sampled view of rapidly
changing private transports. Adjacent notes or tracks can be captured from
different private-head moments, even if they started in unison. VIEW therefore
may transpose one visible member of a pair and not the other. The target list is
frozen for that button press and each underlying event slot is modified at most
once, so this divergence is intentional sampling behavior rather than a missed
or repeated Transpose operation.

## Ratio bank

The live ratio bank cycles in this order:

`1:2 → 2:3 → 3:4 → 4:5 → 5:6 → 7:8 → 15:16 → 1:1 → 17:16 → 8:7 → 6:5 → 5:4 → 4:3 → 3:2 → 2:1 → 3:1 → 5:1`

The ratio is written as `source rows : master rows`.

The `Z0x` pattern bank addresses the 16 non-neutral ratios:

| Command | Ratio | Command | Ratio |
|---|---:|---|---:|
| `Z00` | `1:2` | `Z08` | `8:7` |
| `Z01` | `2:3` | `Z09` | `6:5` |
| `Z02` | `3:4` | `Z0A` | `5:4` |
| `Z03` | `4:5` | `Z0B` | `4:3` |
| `Z04` | `5:6` | `Z0C` | `3:2` |
| `Z05` | `7:8` | `Z0D` | `2:1` |
| `Z06` | `15:16` | `Z0E` | `3:1` |
| `Z07` | `17:16` | `Z0F` | `5:1` |

Use `Z17` or `Z18` for the neutral `1:1` ratio.

## Keyboard controls

The physical track-key map covers all 32 XM channels:

```text
Channels  1-10: 1 2 3 4 5 6 7 8 9 0
Channels 11-20: Q W E R T Y U I O P
Channels 21-29: A S D F G H J K L
Channels 30-32: Z X C
```

- `Ctrl+Shift+Track Key` — toggle that track between Standard and Pattern transport.
- `Alt+Shift+Track Key` — cycle that track through the full ratio bank.
- Hold `Ctrl+Alt+Track Key` — momentary per-track clutch.
- `Ctrl+Alt+Plus` — toggle the global transmission clutch.

The momentary per-track clutch temporarily uses the master row and freezes that private transport. Releasing it synchronizes the private transport to the master.

The latched global transmission clutch is deliberately different: audible playback uses the master while every assigned private transport keeps advancing silently. Releasing the global clutch returns directly to the naturally accumulated private positions.

## Mouse controls

### FasTracks logo

- Left-click — suspend or resume FasTracks globally without erasing the assigned tracks, ratios, modes, directions, or private phases.
- `Shift+Left-click` — synchronize every assigned private transport to the master.
- `Ctrl+Left-click` — randomize every assigned ratio while preserving private position and phase.
- `Ctrl+Shift+Left-click` — randomize and synchronize every assigned transport.
- Right-click — set every assigned track to `1:1` while preserving its private position and phase.
- `Ctrl+Right-click` — set every assigned track to `1:1` and synchronize it to the master.

The global logo acts as a master audible/transport enable. While it is off, private transports are preserved but do not advance. Pattern commands can still prepare ratios, modes, and clutch state underneath it.

### Channel header

- Right-click an assigned, audibly enabled channel header — toggle its private direction.
- `Shift+Right-click` an assigned channel header — toggle Pattern/Song transport.

Reverse changes traversal direction rather than rewriting pattern data. Pattern mode wraps backward within the current pattern; Song mode crosses pattern and order boundaries backward.

## Pattern-programmable commands

Tapehead Edition uses the XM `Z` effect for FasTracks control. The meanings in this table are frozen for module compatibility. Unlisted `Z` parameters are reserved and currently do nothing.

### Per-track commands

| Command | Frozen meaning |
|---|---|
| `Z00-Z0F` | Select one of the 16 non-neutral ratios listed above |
| `Z10` | Release the per-track clutch and rejoin the master |
| `Z11` | Engage the per-track clutch |
| `Z12` | Disable FasTracks on this track |
| `Z13` | Enable this track in Pattern mode |
| `Z14` | One-shot sync this private transport to the master; preserve ratio and mode |
| `Z15` | Select Pattern transport |
| `Z16` | Select Song transport |
| `Z17` | Set this track to `1:1`; preserve private position and phase |
| `Z18` | Set this track to `1:1` and synchronize it to the master |

### Global commands

| Command | Frozen meaning |
|---|---|
| `Z20` | FasTracks master OFF |
| `Z21` | FasTracks master ON |
| `Z22` | Randomize all assigned ratios; preserve private positions and phases |
| `Z23` | Synchronize all assigned private transports to the master |
| `Z24` | Set all assigned tracks to `1:1` and synchronize |
| `Z25` | Global transmission clutch OFF |
| `Z26` | Global transmission clutch ON |
| `Z27` | Set all assigned tracks to `1:1`; preserve private positions and phases |
| `Z28` | Select Pattern transport on all assigned tracks |
| `Z29` | Select Song transport on all assigned tracks |
| `Z2A` | Compatibility alias of `Z27` |
| `Z2B` | Compatibility alias of `Z24` |

Pattern commands are persistent state changes rather than one-row audio effects.
Global commands are queued when any playing transport encounters them,
including a private FasTracks head, and then applied in encounter order after
every channel has advanced for that audio tick. A control lane can therefore
encounter global commands at its own ratio without making the result depend on
whether controlled tracks sit before or after it.

Direction is mouse-controlled in this checkpoint; no `Z` direction command is assigned.

## Synchronization vocabulary

- **Preserve phase / dirty change** — retain each private source position and normalized accumulator while changing ratio or another property.
- **Synchronize / clean sync** — align a private source row and fractional tick phase to the master at that instant.
- **Clutch** — temporarily route audible playback through the master according to the per-track or global behavior described above.

A synchronized non-`1:1` transport begins drifting again immediately because its rate differs from the master. Synchronized `1:1` remains aligned.

## Session state and XM compatibility

FasTracks setup is runtime-only:

- Loading another module retains the current master state, assigned tracks, modes, ratios, and directions.
- Source rows, source orders, fractional phases, and clutch state are reset for the newly loaded module.
- `Z` commands placed near the beginning of a module can establish a deterministic Tapehead performance setup.
- Stock FastTracker-compatible software can load, display, edit, play, and resave the XM. It ignores the Tapehead runtime behavior, so the music plays from the ordinary master transport.

Do not place required musical data outside the standard XM structure. The
**Shift + module Save** baker can translate FasTracks playback into ordinary
pattern data when stock XM playback must reproduce the result. Its **Fast Bake**
mode resolves one pass silently; **Live** mode records repeated song loops while
ratios and other FasTracks controls are performed in real time. CP04.6 uses one
destination row per replayer tick and TPL 1 whenever FasTracks timing is
present. Full behavior and limits are documented in
[`COMPOSITION_BAKER.md`](COMPOSITION_BAKER.md).

## Regression checklist

Before accepting transport changes, test:

- all 17 live ratios at several BPM and TPL values;
- synchronized `1:1` for visible or audible drift;
- forward and reverse Pattern traversal;
- forward and reverse Song traversal across patterns of different lengths;
- ratio changes with private phase preserved;
- per-track clutch and global transmission clutch;
- master suspend/resume;
- dirty randomize and randomize-plus-sync;
- `Z00-Z18` and `Z20-Z2B`, including the two compatibility aliases;
- effects encountered by private control tracks;
- Pattern Matrix performance while Song transports freewheel;
- module loading with retained session setup and reset positional state;
- stock FT2 Clone load, display, playback, save, and Tapehead reopen compatibility.
- Fast Bake and Live Bake at synchronized `1:1`, mixed ratios, and `5:1`.
