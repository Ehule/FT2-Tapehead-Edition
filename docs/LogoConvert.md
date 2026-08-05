# Runtime FasTracks logo replacement

The source tree contains the FasTracks logo/badge bitmap at:

```text
src/gfxdata/bmp/fastTracksLogoBadges.bmp
```

To test a replacement from the repository root:

```bash
cp ~/Downloads/my-logo.bmp src/gfxdata/bmp/fastTracksLogoBadges.bmp
./release/other/ft2-clone
```

Keep a backup or use Git to review the replacement before committing it. The
embedded artwork remains the fallback when a valid runtime bitmap is not
available. Verify the image in the normal theme and with alternate palettes;
the surrounding FasTracks event, clutch, and phase indicators are
palette-sensitive even when the bitmap itself looks correct.

This note describes developer asset replacement, not a user-facing module
feature. Logo changes do not affect XM data or playback.
