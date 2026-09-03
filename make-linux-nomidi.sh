#!/bin/bash

set -eu

OUTPUT="release/other/Tapehead"
TEMP_OUTPUT="${OUTPUT}.new"

cleanup()
{
	rm -f "$TEMP_OUTPUT"
	rm -f src/gfxdata/*.o src/mixer/*.o src/scopes/*.o src/modloaders/*.o \
		src/smploaders/*.o src/*.o 2>/dev/null || true
}

trap cleanup EXIT
mkdir -p release/other
rm -f "$TEMP_OUTPUT"

echo Compiling \(with no MIDI functionality\), please wait patiently...

gcc -DNDEBUG src/gfxdata/*.c src/mixer/*.c src/scopes/*.c src/modloaders/*.c src/smploaders/*.c src/*.c -ffast-math -lSDL2 -ldl -lm -Wshadow -Winit-self -Wall -Wno-missing-field-initializers -Wno-unused-result -Wno-strict-aliasing -Wextra -Wunused -Wunreachable-code -Wswitch-default -Wno-stringop-overflow -O3 -o "$TEMP_OUTPUT"

scripts/copy-tapehead-runtime-assets.sh release/other || exit 1

mv -f "$TEMP_OUTPUT" "$OUTPUT"

echo "Done. The Tapehead executable is 'release/other/Tapehead'."
