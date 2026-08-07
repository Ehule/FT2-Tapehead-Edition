# Undo implementation notes

This build adds the first module-wide transaction history foundation.

## Shortcuts
- Alt+Backspace: Undo
- Shift+Alt+Backspace: Redo

## Limits
- 128 transactions maximum
- 32 MB default memory ceiling
- Configurable through `[Undo]` in `tapehead.ini`: `UndoMemoryMB=32`
- Oldest entries are evicted first
- Redo history is discarded after a new committed edit

## Covered in this pass
- Pattern note/line insert and delete
- Track, pattern, and block cut/paste
- Note/volume/effect interpolation and Melodic Walk commit as one step
- Right-button sample drawing, grouped by mouse stroke
- Sample replacement
- Instrument replacement

## Validation
The modified C translation units pass GCC syntax checking against the bundled SDL2 headers. A complete Linux link was not possible in the sandbox because the system ALSA/SDL development packages are absent.

## Loader coverage

- Disk-loaded sample and instrument overwrites are captured after file decoding succeeds and immediately before the destination slot is changed.
- Loading a sample in instrument mode snapshots the entire destination instrument.
- Instrument snapshots include the separate song instrument-name field.
