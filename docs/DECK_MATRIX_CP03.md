# Deck Matrix command cheatsheet (CP04.2)

Deck Matrix is a full-window performance surface over the same XM, instruments,
samples, transports, MIDI output, and multichannel mixer used by the tracker.
The **Tracker** button returns to ordinary FT2 editing without unloading or
stopping the decks.

## Open and navigate

| Control | Action |
|---|---|
| `[Launcher] Enabled=true` and `Standalone=true` | Open directly into Deck Matrix at startup |
| `Ctrl+Alt+left-click` compact Matrix **Exit / Patt.** or **Exit / Samp.** | Open Deck Matrix from the tracker |
| Pattern bank `00`, `20` ... `E0` | Select one of eight 32-pattern pages |
| Sample bank `00`, `20` ... `E0` | Select one of eight 32-sample pages |
| **EDIT SMP** | Open the visual Sample Matrix Editor without leaving Deck Matrix |
| **TRACKER** | Return to the ordinary FT2 editor |

## Pattern Deck: Q transport

| Gesture | Feedback | Action |
|---|---|---|
| Left-click an inactive tile | Green | Start it in Q, or add it to the four-item boundary queue |
| Left-click a numbered waiting tile | Queue number disappears | Remove that individual pending cue |
| Left-click the active Q tile | Yellow | At the boundary, stop or return to the interrupted song at its saved/current order |
| `Shift+left-click` the active Q tile | Orange | At the boundary, stop or hand back to the song at the next order |
| `Ctrl+left-click` the active Q tile | Red | At the boundary, stop both Pattern Q and the native song transport |
| Repeat the same yellow/orange/red gesture | Green | Disarm that pending boundary exit |

Q owns one Pattern transport. Waiting cues are numbered `1`–`4` and run FIFO.
Ordinary Q gestures do not stop independent Poly spools.

## Pattern Deck: Poly and handoff

| Gesture | Action |
|---|---|
| Middle-click an inactive tile | Start it as one of four independent Poly spools |
| Middle-click an active Poly tile | Pull that spool at its pattern boundary |
| `Shift+middle-click` an active Poly tile | Pull that spool immediately |
| Middle-click the active Q tile | Hand Q to Poly at Q's next loop boundary |
| `Ctrl+Shift+left-click` an active Poly tile | Hand Poly to Q atomically at a safe boundary |
| `Ctrl+Alt+left-click` any tile | Immediately kill/cancel that tile's Q and Poly instances, including waiting/pending starts |

Hard-stopping the active Pattern Q clears its waiting Q queue and immediately
resumes an interrupted song. Hard-stopping a numbered waiting tile removes only
that queue item. An inactive tile is unchanged.

## Pattern Deck: exposure and data management

| Gesture | Action |
|---|---|
| `Alt+left-click` | Mask or expose the tile without changing pattern data |
| `Ctrl+right-click` | Confirm, then clear pattern events while keeping all song-order references |
| `Shift+right-click` | Confirm, then clear the pattern and remove all of its song-order references |

Empty, populated Matrix-only, populated song-used, and masked patterns use
separate monochrome treatments. Strong colors and queue/slot numbers are
reserved for transport feedback.

## Sample Deck

| Gesture | Action |
|---|---|
| Left-click an inactive tile | Start it in Sample Q, or add it to the boundary queue |
| Left-click the active Q tile | Stop Sample Q at the next pattern boundary |
| Middle-click an inactive tile | Start it as one of four independent Sample Poly loops |
| Middle-click an active Poly tile | Stop that loop at the next pattern boundary |
| `Alt+left-click` | Cycle the tile's available audio output |
| `Shift+left-click` | Return to Tracker with that native instrument/sample selected in Sample Editor |
| `Ctrl+right-click` | Copy the currently selected native FT2 sample into this exact tile |
| `Ctrl+Alt+left-click` | Immediately kill/cancel this tile's Q and Poly instances |

`Ctrl+right-click` copies the sample's audio, name, tuning, loop, volume, and
panning. Empty destinations are filled immediately. Occupied tiles ask before
replacement, while keeping their existing output assignment. If the required
bank half does not exist, Tapehead creates and tags one free native instrument
automatically. The source sample remains unchanged.

## Native Sample Banks and folder import

### Visual Sample Matrix Editor

Click **EDIT SMP** in the upper right. The Pattern side becomes a source
browser while the Sample Matrix remains visible and bank-switchable. In this
mode, Sample-tile clicks select a destination and cannot start or stop Q/Poly.
The outlined tile is the current destination.

| Source/control | Action |
|---|---|
| **DISK** | Browse supported sample files outside the module |
| **UPDATE** | Rescan the current disk folder |
| Mouse wheel over the DISK list | Scroll the file browser three rows per notch |
| Click a disk file | Select that file |
| `Ctrl+click` files | Toggle individual files in the selection |
| `Shift+click` | Select the range from the selection anchor |
| Folder row / **UP** | Enter that folder / move to its parent |
| **IMPORT ONE** | Import the first selected disk file into the exact selected tile; confirm replacement |
| **FILL SEL** | Import selected files into successive open tiles |
| **FILL DIR** | Import every supported sample in the folder into successive open tiles |
| **MODULE** | Browse the XM's existing native instruments and samples |
| Instrument `<` / `>` | Select another native instrument |
| Sample row | Select one of that instrument's 16 native sample slots |
| **ASSIGN** | Point the selected tile to that native sample without copying audio |
| **ASSIGN ALL** | Point successive open tiles to every populated sample in the selected instrument |
| **UNASSIGN** | Empty only the selected tile; keep the native sample |
| **CLEAR BNK** | Detach all 32 tiles, keep every native sample in FT2, and make the Matrix bank reusable |
| **DELETE** | Confirm, then delete the referenced native sample; all references to it are affected |
| **DONE** | Return to Deck Matrix performance mode |

Disk fills begin at the selected tile, skip occupied tiles, continue through
later Sample banks, and never overwrite during **FILL SEL** or **FILL DIR**.
Tapehead reports the number imported and omitted. It preflights native
instrument capacity before decoding files.

The DISK browser starts from FT2's Sample Disk Op folder the first time it is
opened. After navigating elsewhere, switching back and forth between the
Sample Matrix Editor and Deck Matrix returns to that last directory. The
editor's bottom actions briefly use the same alternate-color/inset feedback as
the Deck Matrix transport strip; **DONE** shows the acknowledgement before the
performance surface returns.

**CLEAR BNK** removes the structural `SBxxA/B` prefixes from that bank's
backing instruments. Their samples remain in the normal FT2 instrument/sample
selectors, but the old Deck storage no longer reserves those 32 tiles. The
next disk fill can therefore begin again at the first selected tile.

**DISK** and **MODULE** deliberately do different things. A Disk import embeds
the audio once in normal FT2 instrument/sample storage. A Module assignment is
only a live `{instrument, sample}` reference, so the same native sample can be
used by several Matrix tiles without increasing the XM's audio data. Editing
that native sample in FT2 immediately changes every referencing tile.

Tapehead saves arbitrary tile references and explicit unassignments in a small
metadata block appended after the standard XM payload. Compatible trackers
still read the ordinary XM; Tapehead restores the visual Matrix relationships.

### Shortcut and tagged-bank workflows

In Disk Op Sample mode, `Ctrl+Shift+left-click` **Sample** imports the current
folder into the Sample bank selected in Deck Matrix. Files are naturally
sorted and continue across successive banks, using one standard FT2 instrument
for every 16 samples. All required destinations are preflighted before anything
is replaced.

Each 32-tile page is a logical bank backed by two ordinary instruments:

```text
SB00A 00-0F NAME
SB00B 10-1F NAME
```

Later pages use `SB20A`/`SB20B`, through `SBE0A`/`SBE0B`. Deck Matrix reads
only the five-character structural prefix, so the descriptive suffix can be
renamed. Manually naming an existing instrument with the appropriate prefix
adopts its 16 native sample slots into that Deck range after the bank map is
rediscovered.

The Disk Op folder importer and `Ctrl+right-click` direct-copy gesture remain
available as power-user shortcuts. The visual editor is the primary workflow
for assembling or reorganizing Sample banks.

When the native song is stopped, the Sample Deck maintains its own silent
pattern-length clock at the current BPM/TPL. The first Q or Poly sample starts
immediately; later Q clicks retain the full four-item queue and all pending
starts/stops commit at that clock's next pattern boundary. When native playback
is running, the Sample Deck follows the real tracker pattern boundaries.

Ordinary native sample deletion empties one tile. `Shift+right-click` either
tagged instrument offers to clear the complete paired 32-tile bank. Editing or
replacing bank memory safely stops Sample Deck voices; the native song and
Pattern Poly remain separate.

## Song-order and transport strip

| Control | Action |
|---|---|
| `<` / `>` | Select the previous/next real XM song-order position |
| `Oxx Pxx` | Show the selected order and pattern; follow the audible position during native Song playback |
| **PLAY SNG** | Play the native song from the selected order |
| **PLAY PAT** | Loop the selected order's pattern through native FT2 transport |
| **STOP SNG** | Stop only native Song/Pattern playback; leave deck voices active |
| **STOP DECK** | Stop Pattern and Sample Q/Poly; resume a song interrupted by Pattern Q |
| **STOP ALL** | Stop every tracker and Deck Matrix transport |
| Spacebar | Same scope as **STOP SNG**, not global panic |

The order arrows and transport controls briefly flash the selected bank color
and inset bevel when clicked. The upper-right status block shows Pattern and
Sample Q identities, Poly usage, placement results, and non-modal queue/lane
warnings.
