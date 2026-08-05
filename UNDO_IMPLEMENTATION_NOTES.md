# Undo / Redo — CP04.6

Tapehead provides a bounded module-edit transaction history.

## Shortcuts

- `Alt+Backspace` — Undo
- `Shift+Alt+Backspace` — Redo

Undo and Redo are intentionally distinct from ordinary FT2 navigation. A new
committed edit after Undo clears the redo branch.

## Limits

- Up to 128 transactions
- 32 MB default memory ceiling
- Configurable through `[Undo]` in `tapehead.ini`
- Accepted memory range: 4–1024 MB
- Oldest transactions evicted first when the ceiling is reached
- Memory allocated only when edits are recorded; the configured ceiling is not
  reserved at startup

```ini
[Undo]
UndoMemoryMB=32
```

The conservative default supports older systems. Pattern-only transactions are
small; destructive sample or instrument operations consume history more
quickly.

## Covered transactions

- Pattern note/line insert and delete
- Track, pattern, and block cut/paste
- Pattern Matrix data clear and pattern/order deletion
- `Shift+INP` pattern duplication and inserted order entry as one transaction
- Note, volume, and compatible effect interpolation commits
- Melodic Walk commit as one transaction
- Sample Editor right-button drawing, grouped by mouse stroke
- Internal sample replacement
- Internal instrument replacement, including the separate instrument-slot name
- Disk-loaded sample overwrite after successful decoding
- Disk-loaded instrument overwrite after successful decoding
- Deck Matrix direct sample replacement

Loading a sample in instrument mode snapshots the complete destination
instrument. Loader transactions are captured after decoding succeeds and
immediately before the destination changes, so a failed decode does not create
a false history entry.

## Practical boundary

Undo/Redo protects editing operations, not live transport history. FasTracks
phase changes, Q/Poly launches, Deck Matrix routing state, MIDI output, and
Composition Baker performances are not rewound by module Undo.

Run the complete regression suite after changing transaction ownership:

```bash
python3 scripts/test_all_native.py
```
