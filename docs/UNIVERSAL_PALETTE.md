# Universal TapeSister / Tapehead palette

TapeSister and Tapehead share one text palette named `palette.pal`. The
canonical section is `[Palette]`; both readers also accept the legacy
`[TapeheadPalette]` section.

When Tapehead's **Exchange** directory is configured, **Load** and **Save** in
**Configuration → Layout** use `palette.pal` in that shared directory. Without
an exchange directory, Tapehead uses `palette.pal` beside its normal `FT2.CFG`.
At startup Tapehead tries that canonical path, then the legacy local
`tapehead.pal`, then the complete bundled `palette.pal`. Loading a legacy file
never rewrites it. Only an explicit shared save writes the canonical file.

Every shared save writes these 21 color keys:

- Shared interface colors: `PatternText`, `BlockMark`, `TextOnBlock`, `Mouse`,
  `Desktop`, `Buttons`, `PatternNote`, `PatternInstrument`, `PatternVolume`,
  `PatternTuning`, `PatternEffect`, and `PatternEmpty`.
- Tapehead transport colors: `TrackLengthPlayhead`, `FastTracksPlayhead`,
  `ControlPlayhead`, `FastTracksSync`, `FastTracksPhase`, `FastTracksSong`, and
  `FastTracksLengthPlayhead`.
- TapeSister-only colors: `WaveSelection` and `ActiveTile`.
- Contrast values: `DesktopContrast` and `ButtonsContrast`, each from 1 to 100.

Colors use `#RRGGBB`. Tapehead keeps its existing 19 editable names and does
not expose `WaveSelection` or `ActiveTile` as ordinary destinations. It retains
both fields in memory and writes them unchanged, so a Tapehead save cannot
erase TapeSister's selection or active-tile colors.

The bottom of Tapehead's Layout palette panel contains 14 tiny TapeSister
source swatches: the 12 shared interface colors, Wave Selection, and Active
Tile. Click a defined source to copy it into the currently selected Tapehead
color. If an older file omitted a source, its safe fallback remains usable by
the applications, but its eyedropper swatch is neutral and clicking it does
nothing. The short label to the right identifies the sampled source or marks
it `UNSET`. Sampling a built-in Tapehead preset promotes the result into
**User defined** instead of modifying the factory preset.

Saving promotes every safe fallback into an explicit key, producing a complete
file that either application can reopen without losing fields.
