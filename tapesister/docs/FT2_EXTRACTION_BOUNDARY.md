# FT2 UI extraction boundary (foundation pass)

## Provenance and audited sources

The shared code is derived from FT2 Clone's indexed rendering conventions and
the geometry/state transitions in `src/ft2_gui.c`, `src/ft2_scrollbars.c`, and
`src/ft2_pushbuttons.c`. Browser policy comes from the directory scan, sorting,
selection, and navigation behavior in `src/ft2_diskop.c`. The audit also covered
`ft2_diskop.h`, `ft2_sample_ed.c/.h`, `ft2_gui.h`, `ft2_scrollbars.h`,
`ft2_pushbuttons.h`, `ft2_video.c/.h`, `ft2_unicode.c/.h`, `ft2_structs.h`, and
the font/button/loop-pin sources and BMPs in `src/gfxdata`. FT2 Clone's original
copyright and license remain authoritative; see `src/LICENSE.txt`, `LICENSE`,
and `tapesister/NOTICE.md`.

## Boundary

* **Directly reusable:** the 632x400 indexed surface contract, palette-indexed
  pixels, bitmap glyph drawing, bevel rules, dynamic-thumb calculations, arrow,
  track/page and drag behavior, and directory-first ordering.
* **Reusable with explicit context:** widgets now receive `ft2_ui_surface` and
  `ft2_ui_scrollbar`; the browser owns path, entries, selection and viewport.
  This replaces dependencies on FT2's `video.frameBuffer`, `editor`, `ui`, and
  `mouse` globals. Application actions remain callbacks/event-adapter decisions.
* **Renderer/platform adapters:** SDL converts indexed pixels through the chosen
  palette and presents them using nearest-neighbor scaling. Directory enumeration
  remains a small POSIX/Win32 adapter in `ts_file_browser.c`; Unicode conversion
  and the tracker's SDL texture/window ownership are intentionally not shared.
* **Tracker-only:** module/instrument banks, Disk Op module/sample modes, tracker
  dialogs, mouse repeat timing, global pushbutton tables, audio/replayer locks,
  and Sample Editor globals remain in FT2. FT2 sources are unchanged in this pass,
  avoiding behavior risk. The old TapeSister modal list drawing is replaced by
  the shared primitives; its general (non-browser) UI remains temporary.
* **Next extraction:** waveform drawing, zoom/scroll, ranges, loop pins, playback
  controls, destructive edits, sample effects, and import/export adapters.

## Sample-document seam for the next pass

The document will own interleaved/mono PCM storage, sample rate, and frame count.
Its canonical editing representation is signed 16-bit PCM (matching FT2 editing
semantics); floating-point preview/render output is converted only when a render
is accepted. An adapter exposes data without copying into a second `sample_t`.
Metadata comprises loop start, loop length, and none/forward/ping-pong loop type,
plus waveform viewport start/length and selection endpoints.

Publishing a completed asynchronous render is one document transaction. The
document/history layer owns mutation and Undo; the worker never mutates published
storage. Audition receives an immutable generation-qualified view. Import/export
and Bake consume snapshots, so current staging, backup, rollback, and identity
rules remain outside the widget/sample adapter. This seam can support FT2's
waveform, zoom, selection, loop pins, playback, editing, and effects without
making tracker globals or TapeSister DSP dependencies part of the shared layer.

## Synchronization invariant

`ts_file_browser_ensure_visible()` clamps selection, moves the viewport just
enough to include it, then derives scrollbar position and thumb geometry. Every
keyboard movement and directory rescan passes through that invariant. Arrow,
wheel, page, and drag adapters update the same position rather than maintaining
parallel list state.
