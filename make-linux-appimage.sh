#!/bin/bash

set -eu

LINUXDEPLOY="linuxdeploy-$(uname -m).AppImage"
BUILDDIR="release/linux"
APPDIR="$BUILDDIR/Tapehead.AppDir"
DESKTOP_FILE="$BUILDDIR/tapehead.desktop"
DEFAULTS_DIR="$APPDIR/usr/share/tapehead/defaults"
DOC_DIR="$APPDIR/usr/share/doc/tapehead"

rm -f "$BUILDDIR/$LINUXDEPLOY" "$DESKTOP_FILE"
rm -rf "$APPDIR"
echo Compiling, please wait patiently...

mkdir -p "$APPDIR/usr/bin" "$DEFAULTS_DIR" "$DOC_DIR"

gcc -DNDEBUG -DHAS_MIDI -D__LINUX_ALSA__ src/rtmidi/*.cpp src/gfxdata/*.c src/mixer/*.c src/scopes/*.c src/modloaders/*.c src/smploaders/*.c src/*.c -ffast-math -lSDL2 -lpthread -lasound -lstdc++ -ldl -lm -Wshadow -Winit-self -Wall -Wno-missing-field-initializers -Wno-unused-result -Wno-strict-aliasing -Wextra -Wunused -Wunreachable-code -Wswitch-default -Wno-stringop-overflow -O3 -o "$APPDIR/usr/bin/Tapehead"

scripts/copy-tapehead-runtime-assets.sh "$DEFAULTS_DIR"
cp "$DEFAULTS_DIR/tapeheadSplash.png" "$APPDIR/usr/bin/"
cp "$DEFAULTS_DIR/fastTracksLogoBadges.bmp" "$APPDIR/usr/bin/"
cp release/linux/AppRun "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"
cp LICENSE "$DOC_DIR/LICENSE.txt"
cp release/LICENSES.txt "$DOC_DIR/LICENSES.txt"
cp release/PORTABLE_README.txt "$DOC_DIR/README.txt"

rm -f src/rtmidi/*.o src/gfxdata/*.o src/mixer/*.o src/scopes/*.o src/modloaders/*.o src/smploaders/*.o src/*.o

curl "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/$LINUXDEPLOY" -L -o "$BUILDDIR/$LINUXDEPLOY"
chmod +x "$BUILDDIR/$LINUXDEPLOY"
cp release/other/tapehead.desktop "$DESKTOP_FILE"

ROOTDIR="$PWD"
APP_ICON="$ROOTDIR/src/gfxdata/icon/tapehead/png/tapehead-512.png"
cd "$BUILDDIR"
"./$LINUXDEPLOY" --appdir Tapehead.AppDir --output appimage --custom-apprun Tapehead.AppDir/AppRun --icon-file "$APP_ICON" --icon-filename "tapehead" --desktop-file "tapehead.desktop"

echo "Done. The Tapehead AppImage is in '$BUILDDIR'."
