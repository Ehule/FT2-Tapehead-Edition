# TapeSister source incubation

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
Up/Down select recipes, Enter plays the root, G toggles gating, Space stops all,
and Escape exits. Because the requested G gate control overlaps the G note key,
pressing G both toggles the audition mode and plays its mapped note. Recipe rows
and piano keys are clickable.

The preview resampler is intentionally linear. SDL requests native-endian
32-bit float stereo; the mono buffer is copied equally to both channels. The
callback only mixes pre-rendered immutable buffers. Parsing, rendering, I/O,
allocation, logging, SDL locking, and UI work happen outside the callback.

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
