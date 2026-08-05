# Deck Matrix — CP04.6 quick start

Deck Matrix is a full-window performance surface over the same XM, clock,
instruments, samples, MIDI output, and multichannel mixer used by the tracker.
It is not a second module player. **TRACKER** returns to ordinary FT2 editing
without unloading the module or stopping independent deck voices.

For the exhaustive gesture and state table, use the maintained
[`DECK_MATRIX_CP03.md`](DECK_MATRIX_CP03.md) command reference. Its filename is
historical; the document includes the Sample Matrix Editor refinements through
CP04.2.

## Surface at a glance

- Left side: 32 Pattern tiles from one of eight pattern pages
- Right side: 32 Sample tiles from one of eight sample banks
- Pattern left-click: one Q foreground plus a four-item boundary queue
- Pattern middle-click: up to four independent Poly spools
- Sample left-click: one Sample Q voice plus a four-item boundary queue
- Sample middle-click: up to four independent Sample Poly voices
- Bottom strip: real song-order selection and separate Song, Deck, and global
  transport controls
- **EDIT SMP**: visual disk/module Sample Matrix Editor
- **TRACKER**: return to the ordinary editor

Pattern and Sample decks share BPM, TPL, and pattern boundaries, but they keep
independent voice ownership. Starting one deck does not evict the other.

## Open Deck Matrix

The supplied `tapehead.ini` enables the full-window surface at startup:

```ini
[Launcher]
Enabled=true
Standalone=true
```

From the compact Matrix surface, `Ctrl+Alt+left-click` **Exit / Patt.** or
**Exit / Samp.** opens the full-window Deck Matrix. Select **TRACKER** to leave
it.

## Minimal performance gestures

| Gesture | Pattern tile | Sample tile |
|---|---|---|
| Left-click | Start/cue Q | Start/cue Sample Q |
| Middle-click | Start or gracefully pull Poly | Start or gracefully pull Sample Poly |
| `Shift+middle-click` | Pull active Poly immediately | — |
| `Alt+left-click` | Mask/expose pattern | Cycle output destination |
| `Ctrl+Alt+left-click` | Immediately kill/cancel this tile's Q and Poly instances | Same |

Pattern Q and Poly also support seamless handoffs. See the detailed command
reference before using those gestures in a live set.

## Sample Matrix Editor

Select **EDIT SMP**. In editor mode, Sample tiles choose destinations instead
of launching audio.

- **DISK** browses sample files outside the module.
- **IMPORT ONE**, **FILL SEL**, and **FILL DIR** decode files into ordinary FT2
  instrument/sample storage.
- **MODULE** browses samples already inside the XM.
- **ASSIGN** and **ASSIGN ALL** create zero-copy `{instrument, sample}` tile
  references.
- **UNASSIGN** empties a tile but keeps its native sample.
- **CLEAR BNK** detaches all 32 tiles but keeps their native samples.
- **DELETE** is the separate destructive native-sample operation.
- **DONE** returns to performance mode.

Disk fills begin at the selected tile, skip occupied tiles, and continue across
later banks. The DISK list supports click, `Ctrl+click`, `Shift+click`, folder
navigation, and three-row mouse-wheel scrolling. Its last directory is
remembered when moving between Deck Matrix and the editor during the same run.

## Native Sample Bank storage

Each 32-tile page can be backed by two ordinary 16-sample instruments whose
names begin with:

```text
SB00A / SB00B
SB20A / SB20B
...
SBE0A / SBE0B
```

Tapehead only reserves the five-character prefix; descriptive text may follow
it. Arbitrary module-sample assignments and explicit unassignments are saved in
a small Tapehead metadata block after the standard XM payload. The underlying
XM remains readable by compatible players.

## Transport strip

| Control | Scope |
|---|---|
| **PLAY SNG** | Native Song playback from selected order |
| **PLAY PAT** | Native Pattern loop for selected order |
| **STOP SNG** | Native Song/Pattern only |
| **STOP DECK** | Pattern and Sample Q/Poly only |
| **STOP ALL** | Every native and Deck transport |
| Spacebar | Same scope as **STOP SNG** |

This separation is deliberate. It lets the native song stop while latched deck
voices continue, or clears the deck without treating every stop as a panic.

## Baker boundary

The CP04.6 Composition Baker records the Song replayer and FasTracks. It does
not yet capture Pattern or Sample Deck Q/Poly performance. Stop deck activity
before arming either bake mode.
