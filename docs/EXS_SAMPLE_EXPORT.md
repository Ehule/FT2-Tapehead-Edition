# EXS — XM Sample Extraction and Round Trip

EXS exports populated samples from the loaded module as uncompressed mono WAV files without normalizing, resampling, applying envelopes, or changing the module. Open Disk Op, select **Sample**, select **EXS**, choose the destination name, and press **Save**.

The filename field names the new export directory. Selecting or auditioning a
sample while EXS mode is active does not change that directory name. The export
dialog offers two modes:

- **Used only** scans only patterns reached through the active song order list. An instrument-number event counts even without a note. Every populated sample slot in each used instrument is exported so multisample instruments remain complete.
- **All** exports every populated sample slot in every instrument, including instruments not referenced by the song.

EXS creates a numbered destination when necessary (`SongName_EXS`, `SongName_EXS_02`, and so on). A single-sample instrument is written directly in the export root so it is immediately available in the file list. Only instruments with two or more populated samples receive their own instrument folder. WAV names always begin with a stable instrument/sample identity such as `I01_S00_`; the sample slot is intentionally zero-based. A recognized source-audio extension already present in an XM sample name is removed before the single output `.wav` extension is added.

Every export also creates an empty `Processed` directory. Render externally
edited versions there while leaving the source WAVs and the single manifest in
the export root unchanged. REAPER may write either a flat set of WAVs directly
inside `Processed`, or mirror the manifest's instrument subdirectories. The
`Ixx_Sxx` identities make the normal flattened filenames globally unique.

`EXS_manifest.ini` records format version 1, export mode, authoritative instrument and sample indexes, original names, relative WAV paths, source precision, length, tuning, volume, panning, loop information, and original sample flags. Instrument and sample indexes—not filenames or display names—are the authoritative round-trip identities.

## Round-trip replacement

1. Export **Used only** or **All** and keep `EXS_manifest.ini` in the export root.
2. Edit or batch-process the WAV files in TapeSister, REAPER, or another audio tool. Render the results into `Processed` using the exported filenames. Do not copy, rename, or replace the manifest. Successive renders may overwrite the files in `Processed`.
3. Load the target XM in Tapehead.
4. Open Disk Op, select **Sample**, and enter the original EXS export folder so its contents are the current listing.
5. Press **Ctrl+R**, or right-click the **EXS** format row.
6. Inspect whether Tapehead selected **Processed** or **Original export**, the export source, current module, the first 12 exact destination mappings, original/new frame lengths, the remaining mapping count, and any loop warnings. A source-name mismatch is called out explicitly. Confirm only when those destinations are correct; the manifest contains the complete list for a larger export.
7. Save the result as a new XM. One **Undo** reverses the complete accepted replacement.

When `Processed` is empty, Tapehead reads the untouched original export. Once
anything has been placed in `Processed`, Tapehead requires a complete valid
processed WAV set and never silently mixes processed and original files. It
accepts the manifest-relative directory layout or one flat set of uniquely
named WAVs. Every manifest WAV must decode successfully before confirmation.
Tapehead then replaces only the listed `InstrumentIndex`/`SampleIndex` slots;
unlisted instruments, unlisted sample slots, note maps, envelopes, and other
instrument settings remain intact. Cancellation, an incomplete processed set,
an invalid path, malformed metadata, missing/invalid WAV, allocation failure,
or an import larger than the configured Undo memory limit changes nothing.

Replacement WAVs may change length, precision, or sample rate. The decoded WAV precision and sample-rate tuning are adopted so an externally resampled sound retains its intended pitch and duration. The manifest restores its XM sample name, default volume, and panning. An original loop is restored at its exact frame coordinates when it still fits the edited WAV; if it no longer fits, the preview warns and the loop is disabled rather than silently moved or clamped.

EXS v1 accepts mono or stereo WAV input. Stereo uses Tapehead's deterministic folder-import mix-to-mono policy, and 24/32-bit integer or 32/64-bit float WAVs are converted through the existing 16-bit XM sample loader. The manifest records source precision for reference; the replacement's decoded precision is what is stored.

Shift-click the Disk Op **Sample** selector and choose **This folder** or **Subfolders too** before choosing the existing instrument-import mode. Ctrl+Shift-click offers the same scope choice for loading samples directly into the Deck Matrix. **Subfolders too** gathers the root-level single samples and the contents of all multi-sample instrument folders, so a complete EXS export can be reused as one palette without opening every folder separately.

The generic folder-import gestures above remain useful when the WAVs should become a new palette. Use EXS replacement only when they should return to their original XM slots.
