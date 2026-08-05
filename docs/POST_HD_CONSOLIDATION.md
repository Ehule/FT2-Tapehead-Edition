# Post-HD consolidation checkpoint

> **Historical source-lineage note.** The CP04.6 tree is now preserved on
> `baker-experimental` and tagged `cp04.6`; use the current documentation index
> in [`README.md`](README.md) for user-facing behavior.

This tree is a later experimental descendant of Tapehead RC1. It combines the
Poly Matrix/Q shared-tunnel work, multichannel Pass 5, folder sample import and
the optional HD presentation layer. It is not a replacement for the frozen RC1
branch.

## Recommended branch lineage

Create `post-hd-consolidation` from the exact experimental commit represented by
this source tree. Keep `workmode-structural-refactor` and the `rc1` tag frozen.
Do not merge this source snapshot directly into RC1: it contains several
independent experimental systems whose hardware and visual behavior still need
separate validation.

The ZIP used to reconstruct this checkpoint did not contain `.git`, so its
original commit identity cannot be proven from the archive alone. Match the
source files against the local experimental branch before creating or merging
the branch.

## Consolidation fixes

- Restored the Poly Matrix native test link seam and added a Q-owned-tunnel
  collision case.
- Corrected generated instrument numbers to decimal (`Instrument 10`, not
  `Instrument 0A`).
- Corrected formatted loader messages so their arguments reach the dialog.
- Added the missing standard string header to the HD video translation unit.
- Added `scripts/test_all_native.py` as the single regression entry point.
- Returned the checked-in Tapehead configuration to hardware-neutral defaults.
- Ignored machine-local audio/MIDI selection files and patch/editor leftovers.

## Validation boundary

The standalone native suites cover FasTracks clocks and transport, multichannel
routing and delivery, the dynamically loaded JACK backend, Poly Matrix/Q
ownership, handoffs and effect isolation. They do not replace interactive tests
for the Disk Op folder-import dialog, live JACK hardware routing, Windows audio,
or HD appearance on the X220/X230 and 2K display.

Run all standalone suites with:

```bash
python3 scripts/test_all_native.py
```

## Deliberately deferred

The HD scaler is kept unchanged in this checkpoint. Its current code is a
presentation-only Scale2x/Scale3x pass, but visual preference between the
rounder and sharper treatments must be settled from screenshots before the
algorithm is revised again.
