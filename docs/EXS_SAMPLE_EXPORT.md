# EXS — Export XM Samples

EXS exports populated samples from the loaded module as uncompressed mono WAV files without normalizing, resampling, applying envelopes, or changing the module. Open Disk Op, select **Sample**, select **EXS**, choose the destination name, and press **Save**.

The export dialog offers two modes:

- **Used only** scans only patterns reached through the active song order list. An instrument-number event counts even without a note. Every populated sample slot in each used instrument is exported so multisample instruments remain complete.
- **All** exports every populated sample slot in every instrument, including instruments not referenced by the song.

EXS creates a numbered destination when necessary (`SongName_EXS`, `SongName_EXS_02`, and so on). A single-sample instrument is written directly in the export root so it is immediately available in the file list. Only instruments with two or more populated samples receive their own instrument folder. WAV names always begin with a stable instrument/sample identity such as `I01_S00_`; the sample slot is intentionally zero-based.

`EXS_manifest.ini` records format version 1, export mode, authoritative instrument and sample indexes, original names, relative WAV paths, source precision, length, tuning, volume, panning, loop information, and original sample flags. This file is the contract for a future sample-replacement workflow. Reverse import is not implemented yet.

Shift-click the Disk Op **Sample** selector and choose **This folder** or **Subfolders too** before choosing the existing instrument-import mode. Ctrl+Shift-click offers the same scope choice for loading samples directly into the Deck Matrix. **Subfolders too** gathers the root-level single samples and the contents of all multi-sample instrument folders, so a complete EXS export can be reused as one palette without opening every folder separately.

Keep the manifest alongside externally edited WAVs if the material may later be returned to its original XM slots.
