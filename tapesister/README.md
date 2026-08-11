# TapeSister source incubation

The SDL-independent `ts_editor.h` parameter catalog is the authority for six
renderer-1 pages, formatting, mappings, safe access, canonical identity, and a
bounded 128-snapshot undo/redo history. Schema v1 has no separate color,
delay, or ambience bypasses, damping, fades, octave/ratio, or filter-envelope
decay; the editor does not invent or serialize future parameters.

TapeSister is a standalone sample-instrument forge being incubated in the
Tapehead source tree. It does not link to FT2 tracker state or modify the
tracker executable.

## Phases 1A and 1B

This checkpoint contains the deterministic offline rendering core only. The
render graph has explicit source/excitation, noise, amplitude and pitch
contours, a multimode state-variable filter, nonlinear shaping, delay,
reverberation, DC removal, and peak normalization stages. The six compiled
recipes in `src/ts_recipes.c` cover clean sustain, percussive pluck, noisy
metal, unstable drone, digital bass, and spacious decay targets.

Phase 1B adds the closed, canonical JSON recipe boundary, deterministic mono
PCM16 conversion, minimal RIFF/WAVE export, collision-safe atomic files, and
all-or-nothing recipe/WAV pair publication. See `docs/RECIPE_FORMAT.md`.
Phase 1C adds the first SDL2 audition executable,
16-voice preview mixer, indexed 632x400 UI, waveform and piano-key interaction.

## Build and test

Configure this directory independently from the tracker:

```sh
cmake -S tapesister -B build/tapesister -DCMAKE_BUILD_TYPE=Release
cmake --build build/tapesister --parallel
ctest --test-dir build/tapesister --output-on-failure
```

SDL2 is optional at configuration time so headless core tests remain usable:

```sh
cmake -S tapesister -B build/tapesister -DTAPESISTER_BUILD_SDL=ON
cmake --build build/tapesister --parallel
./build/tapesister/tapesister
```

When SDL2 is not installed, CMake prints a warning and omits only the executable.
Use `-DTAPESISTER_BUILD_SDL=OFF` explicitly for a headless build.

Factory recipes are copied to `resources/recipes` beside the executable. At
development time the app checks `--resource-dir`, that installed/build-tree
location, then the configured source fixture directory. Options are
`--recipe PATH`, `--palette-file PATH`, `--palette default|dark`,
`--resource-dir PATH`, and `--smoke-test`.

Keyboard notes are `ZSXDCVGBHNJM` and `Q2W3ER5T6Y7U`. `[`/`]` change octave,
Tab/Shift+Tab and Page Up/Down cycle pages, Up/Down select rows, and
Left/Right edit. Ctrl+G toggles gating, Ctrl+Z/Ctrl+Y undo/redo,
Ctrl+P commits Parent, Ctrl+Shift+P updates Parent, Space stops all, and Escape
exits. G is only its mapped chromatic note. Recipe rows, tabs, controls, and
piano keys are clickable.

Save, Load, and Bake use the shared internal file browser. It shows the current
absolute directory, directories before compatible recipe files, a separate
filename/base-name field, and Home, filesystem Root, Parent, and New Directory
controls. Successful operations remember their directory; Escape cancels
without changing session or file identities.

The 632x400 framebuffer is fractionally scaled to the largest aspect-correct
rectangle that fits the renderer output, with nearest-neighbor filtering and
letterboxing only on the unused axis. The same drawable-aware transform maps
window mouse coordinates back to logical pixels, including high-DPI windows.

The preview resampler is intentionally linear. SDL requests native-endian
32-bit float stereo; the mono buffer is copied equally to both channels. The
callback only mixes pre-rendered immutable buffers. Parsing, rendering, I/O,
allocation, logging, SDL locking, and UI work happen outside the callback.
Overload blocks increment an atomic event generation; the UI keeps the warning
visible for 750ms after the newest event and then clears it automatically.

Palette-file compatibility accepts the six required Tapehead keys
`PatternText`, `BlockMark`, `TextOnBlock`, `Mouse`, `Desktop`, and `Buttons`
with `#rrggbb` values. Additional Tapehead pattern fields and contrast values
are ignored in Phase 1C; malformed/incomplete files retain the built-in palette.

The core uses portable C11. Deterministic targets disable floating-point
contraction and do not use the tracker's `-ffast-math` setting. The tests
render every fixture twice and compare the float buffers byte-for-byte, then
check finiteness, peak bounds, silence, DC offset, feature spread, and pairwise
waveform correlation.

The current deterministic guarantee is for repeated renders made by the same
binary. Cross-compiler byte identity will be measured before the renderer
format is frozen because system transcendental functions can differ between
toolchains.
