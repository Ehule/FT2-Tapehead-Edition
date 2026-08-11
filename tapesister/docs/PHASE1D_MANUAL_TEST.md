# TapeSister Phase 1D manual test

## Build and launch (SDL2 Linux)
```sh
cmake -S tapesister -B build/tapesister -DCMAKE_BUILD_TYPE=Release -DTAPESISTER_BUILD_SDL=ON -DBUILD_TESTING=ON
cmake --build build/tapesister --parallel
ctest --test-dir build/tapesister --output-on-failure
./build/tapesister/tapesister
```
Dummy-driver smoke: `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/tapesister/tapesister --smoke-test`.
## Checklist
1. Edit and hear one parameter on SOURCE, CONTOUR, FILTER, COLOR, SPACE, and SAMPLE.
2. Drag rapidly while holding a note; verify one Undo step, the old held sound, then the new sound.
3. Verify slider, wheel, toggle, enum, numeric, and UTF-8 recipe-name editing.
4. Exercise Undo/Redo shortcuts and confirm one Undo per completed drag.
5. Commit Parent, edit, undo to Parent, and test Update Parent confirm and cancel.
6. Verify plain `G`, `Ctrl+G`, clickable audition mode, Tab page navigation, Up/Down row navigation, and repeat suppression.
7. Exercise Save, Save As, Load, cancel, malformed-load retention, and collision feedback.
8. Bake the exact WAV/recipe pair, import WAV into Tapehead, then test baked invalidation/restoration.
9. Switch factory presets with an old note active and test discard behavior.
10. If practical, disable audio and verify offline edit/save/load/bake.
11. Check cursor, Stop All, gated/one-shot, focus loss, and absence of stuck notes.
12. Check clipping of long audio, path, recipe-name, and error strings.
13. Resize/maximize and test high-DPI, letterbox edges, and sub-632x400 scaling.
14. Close during a pending render and during a modal; verify clean shutdown.
15. Confirm no stale publication, click, hang, temporary leak, ownership fault, or half-written pair.

Schema v1 has no independent color/delay/ambience bypasses, damping, fades,
octave/ratio, or filter-envelope-decay fields. They must not be fabricated.
