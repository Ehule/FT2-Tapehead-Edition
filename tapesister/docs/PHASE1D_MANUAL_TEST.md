# TapeSister Phase 1D manual test

## Build and launch (SDL2 Linux)
```sh
cmake -S tapesister -B build/tapesister -DCMAKE_BUILD_TYPE=Release -DTAPESISTER_BUILD_SDL=ON
cmake --build build/tapesister --parallel
ctest --test-dir build/tapesister --output-on-failure
./build/tapesister/tapesister
```
## Checklist
1. Edit and hear one parameter on SOURCE, CONTOUR, FILTER, COLOR, SPACE, and SAMPLE.
2. Drag rapidly; verify responsiveness and one Undo step per drag.
3. Hold a note during an edit; the held note keeps the old sound and the next uses the new render.
4. Verify sliders, toggles, enums, wheel, and direct numeric/name entry.
5. Exercise Tab navigation and all documented shortcuts.
6. Verify plain `G` is a note and `Ctrl+G` only toggles audition mode.
7. Test undo/redo, including one undo per drag.
8. Commit Parent, edit, undo to it, and explicitly confirm Update Parent.
9. Save, reload, and compare sound.
10. Bake a WAV/recipe pair and import the WAV into Tapehead.
11. Verify overwrite confirmations and canceled dialogs.
12. Resize/maximize and repeat edge clicks.
13. Switch presets while audio is active.
14. Close while a render is pending.
15. Confirm no clicks, stuck notes, stale renders, hangs, ownership faults, or shutdown errors.

Schema v1 has no independent color/delay/ambience bypasses, damping, fades,
octave/ratio, or filter-envelope-decay fields. They must not be fabricated.
