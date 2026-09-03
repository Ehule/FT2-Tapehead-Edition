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

Every shared save writes these 26 color keys:

- Shared interface colors: `PatternText`, `BlockMark`, `TextOnBlock`, `Mouse`,
  `Desktop`, `Buttons`, `PatternNote`, `PatternInstrument`, `PatternVolume`,
  `PatternTuning`, `PatternEffect`, `PatternEmpty`, and `WaveSelection`.
- Tapehead transport colors: `TrackLengthPlayhead`, `FastTracksPlayhead`,
  `ControlPlayhead`, `FastTracksSync`, `FastTracksPhase`, `FastTracksSong`, and
  `FastTracksLengthPlayhead`.
- TapeSister-only colors: `ActiveTile`, `StereoWaveLeft`, `StereoWaveRight`,
  `StereoWaveSum`, `SisterSourceHorizontal`, and `SisterSourceVertical`.
- Contrast values: `DesktopContrast` and `ButtonsContrast`, each from 1 to 100.

Colors use `#RRGGBB`. Tapehead exposes 20 friendly editor names. `PAT Text`
controls ordinary pattern text, `Note / Wave` controls both colored pattern
notes and the Sample Editor waveform, and `Wave Select` supplies the
translucent Sample Editor range tint. The persisted keys remain stable. The six
TapeSister-only fields stay in memory and are written unchanged, so a Tapehead
save cannot erase TapeSister's active-tile, stereo-wave, or Sister-source
colors.

The bottom of Tapehead's Layout palette panel contains 19 tiny TapeSister
source swatches: the 12 shared interface colors, Wave Selection, Active Tile,
the three stereo-wave colors, and the two Sister-source edge colors. Click a
defined source to copy it into the currently selected Tapehead color. If an
older file omitted a source, its safe fallback remains usable by the
applications, but its eyedropper swatch is neutral and clicking it does
nothing. The short label to the right identifies the sampled source or marks
it `UNSET`. Sampling a built-in Tapehead preset promotes the result into
**User defined** instead of modifying the factory preset.

Saving promotes every safe fallback into an explicit key, producing a complete
file that either application can reopen without losing fields.
