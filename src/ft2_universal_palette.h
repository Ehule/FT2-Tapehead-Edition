#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TAPEHEAD_UNIVERSAL_COLOR_COUNT 21
#define TAPEHEAD_UNIVERSAL_TAPEHEAD_COLOR_COUNT 19
#define TAPEHEAD_UNIVERSAL_TAPESISTER_SWATCH_COUNT 14
#define TAPEHEAD_UNIVERSAL_ALL_COLORS_MASK \
	((UINT32_C(1) << TAPEHEAD_UNIVERSAL_COLOR_COUNT) - UINT32_C(1))
#define TAPEHEAD_UNIVERSAL_UNSET_SWATCH_RGB UINT32_C(0x5A5A5A)

typedef enum tapeheadUniversalColor_t
{
	TAPEHEAD_UNIVERSAL_PATTERN_TEXT = 0,
	TAPEHEAD_UNIVERSAL_BLOCK_MARK,
	TAPEHEAD_UNIVERSAL_TEXT_ON_BLOCK,
	TAPEHEAD_UNIVERSAL_MOUSE,
	TAPEHEAD_UNIVERSAL_DESKTOP,
	TAPEHEAD_UNIVERSAL_BUTTONS,
	TAPEHEAD_UNIVERSAL_PATTERN_NOTE,
	TAPEHEAD_UNIVERSAL_PATTERN_INSTRUMENT,
	TAPEHEAD_UNIVERSAL_PATTERN_VOLUME,
	TAPEHEAD_UNIVERSAL_PATTERN_TUNING,
	TAPEHEAD_UNIVERSAL_PATTERN_EFFECT,
	TAPEHEAD_UNIVERSAL_PATTERN_EMPTY,
	TAPEHEAD_UNIVERSAL_WAVE_SELECTION,
	TAPEHEAD_UNIVERSAL_ACTIVE_TILE,
	TAPEHEAD_UNIVERSAL_TRACK_LENGTH_PLAYHEAD,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_PLAYHEAD,
	TAPEHEAD_UNIVERSAL_CONTROL_PLAYHEAD,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_SYNC,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_PHASE,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_SONG,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_LENGTH_PLAYHEAD
} tapeheadUniversalColor_t;

typedef struct tapeheadUniversalPalette_t
{
	uint32_t colors[TAPEHEAD_UNIVERSAL_COLOR_COUNT];
	uint32_t definedColors;
	uint8_t desktopContrast;
	uint8_t buttonsContrast;
} tapeheadUniversalPalette_t;

void tapeheadUniversalPaletteDefault(tapeheadUniversalPalette_t *palette);
const char *tapeheadUniversalPaletteColorKey(tapeheadUniversalColor_t color);
bool tapeheadUniversalPaletteLoadStream(tapeheadUniversalPalette_t *palette,
	FILE *file, char *error, size_t errorSize);
bool tapeheadUniversalPaletteSaveStream(const tapeheadUniversalPalette_t *palette,
	FILE *file, char *error, size_t errorSize);

bool tapeheadUniversalPaletteColorIsDefined(
	const tapeheadUniversalPalette_t *palette, tapeheadUniversalColor_t color);
tapeheadUniversalColor_t tapeheadUniversalPaletteTapeheadColor(int32_t index);
tapeheadUniversalColor_t tapeheadUniversalPaletteTapeSisterSwatchColor(int32_t swatch);
const char *tapeheadUniversalPaletteTapeSisterSwatchName(int32_t swatch);
uint32_t tapeheadUniversalPaletteTapeSisterSwatchDisplayColor(
	const tapeheadUniversalPalette_t *palette, int32_t swatch);
bool tapeheadUniversalPaletteSampleTapeSisterFrom(
	tapeheadUniversalPalette_t *destinationPalette,
	const tapeheadUniversalPalette_t *sourcePalette,
	int32_t tapeheadDestination, int32_t swatch);
bool tapeheadUniversalPaletteSampleTapeSister(tapeheadUniversalPalette_t *palette,
	int32_t tapeheadDestination, int32_t swatch);

/* Build the canonical/legacy palette path. A configured exchange directory
** wins. Otherwise filename is placed beside the supplied FT2 config path. */
bool tapeheadUniversalPaletteResolvePath(char *path, size_t pathSize,
	const char *exchangeDirectory, const char *configFilePath,
	const char *filename);
