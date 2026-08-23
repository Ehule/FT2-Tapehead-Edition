#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 DESTINATION_DIR" >&2
    exit 1
fi

SOURCE_DIR="release/other"
DEST_DIR="$1"
ASSETS="FT2.CFG tapehead.ini palette.pal fastTracksLogoBadges.bmp tapeheadSplash.png"

mkdir -p "$DEST_DIR"

for asset in $ASSETS; do
    if [ ! -f "$SOURCE_DIR/$asset" ]; then
        echo "Missing required Tapehead runtime asset: $SOURCE_DIR/$asset" >&2
        exit 1
    fi

    if [ "$SOURCE_DIR/$asset" != "$DEST_DIR/$asset" ]; then
        cp "$SOURCE_DIR/$asset" "$DEST_DIR/$asset"
    fi
done

OPTIONAL_APP_ICON="src/gfxdata/icon/tapehead/tapehead-icon.bmp"
if [ -f "$OPTIONAL_APP_ICON" ] && [ "$OPTIONAL_APP_ICON" != "$DEST_DIR/tapehead-icon.bmp" ]; then
    cp "$OPTIONAL_APP_ICON" "$DEST_DIR/tapehead-icon.bmp"
fi
