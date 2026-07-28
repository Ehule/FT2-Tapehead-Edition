#!/bin/bash

set -eu

OUTPUT="release/other/ft2-clone"
TEMP_OUTPUT="${OUTPUT}.new"

cleanup()
{
	rm -f "$TEMP_OUTPUT"
	rm -f src/rtmidi/*.o src/gfxdata/*.o src/mixer/*.o src/scopes/*.o \
		src/modloaders/*.o src/smploaders/*.o src/*.o 2>/dev/null || true
}

trap cleanup EXIT
mkdir -p release/other
rm -f "$TEMP_OUTPUT"

echo Compiling, please wait patiently...

gcc -DNDEBUG -DHAS_MIDI -D__LINUX_ALSA__ src/rtmidi/*.cpp src/gfxdata/*.c src/mixer/*.c src/scopes/*.c src/modloaders/*.c src/smploaders/*.c src/*.c -ffast-math -lSDL2 -lpthread -lasound -lstdc++ -lm -Wshadow -Winit-self -Wall -Wno-missing-field-initializers -Wno-unused-result -Wno-strict-aliasing -Wextra -Wunused -Wunreachable-code -Wswitch-default -Wno-stringop-overflow -march=native -mtune=native -O3 -o "$TEMP_OUTPUT"

scripts/copy-tapehead-runtime-assets.sh release/other || exit 1

mv -f "$TEMP_OUTPUT" "$OUTPUT"

echo "Done. The executable can be found in 'release/other'."
