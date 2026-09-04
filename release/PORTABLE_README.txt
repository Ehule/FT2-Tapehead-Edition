TAPEHEAD PORTABLE RELEASE
=========================

Tapehead is portable software. It does not use an installer and does not need
administrator access. Keep the distributed files together in one writable
folder so that the application, settings, palette, and visual assets remain a
single self-contained unit.

WINDOWS
-------
1. Extract the complete Tapehead-Windows archive.
2. Open the extracted folder and double-click Tapehead.exe.
3. Do not run the executable from inside the ZIP file.

Tapehead saves FT2.CFG, tapehead.ini, palette.pal, and device selections beside
the executable. SDL2.dll must remain in the same folder as Tapehead.exe.

For REAPER and VB-CABLE routing, the proven Windows configuration is WASAPI in
both applications, preferably Shared Mode, with matching sample rates.

LINUX
-----
1. Make the AppImage executable: chmod +x Tapehead-*.AppImage
2. Run the AppImage directly.

On first launch, the AppImage creates a writable Tapehead-data folder beside
it. Keep that folder with the AppImage when moving or backing up Tapehead. No
system-wide installation is performed.

Ordinary tracker/block WAV renders auto-create a Captures folder beside
FT2.CFG (inside Tapehead-data for AppImage builds). This is separate from the
TapeSister exchange inbox; only an explicit TapeSister render destination is
published there.

During Ctrl+L Block Loop, F8 captures one clean offline cycle. F7 arms a live
performance capture at the next loop boundary; change the block with
Shift+Arrow, then press F7 again to stop at the following boundary. These WAVs
also go to Captures and never require TapeSister.

LICENSES
--------
Tapehead is derived from ft2-clone and FastTracker II. Complete source and
third-party licensing information is included with the release and repository.
