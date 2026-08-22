#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ft2_universal_palette.h"

static void joinPath(char *destination, size_t capacity, const char *directory,
	const char *filename)
{
	const int written = snprintf(destination, capacity, "%s/%s", directory,
		filename);
	assert(written >= 0 && (size_t)written < capacity);
}

static void writeText(const char *path, const char *text)
{
	FILE *file = fopen(path, "wb");
	assert(file != NULL);
	assert(fputs(text, file) >= 0);
	assert(fclose(file) == 0);
}

static bool fileContains(const char *path, const char *needle)
{
	FILE *file = fopen(path, "rb");
	assert(file != NULL);
	char line[256];
	while (fgets(line, sizeof (line), file) != NULL)
	{
		if (strstr(line, needle) != NULL)
		{
			fclose(file);
			return true;
		}
	}
	fclose(file);
	return false;
}

static int countColorKeys(const char *path)
{
	FILE *file = fopen(path, "rb");
	assert(file != NULL);
	char line[256];
	int count = 0;
	while (fgets(line, sizeof (line), file) != NULL)
	{
		for (int color = 0; color < TAPEHEAD_UNIVERSAL_COLOR_COUNT; color++)
		{
			const char *key = tapeheadUniversalPaletteColorKey(
				(tapeheadUniversalColor_t)color);
			const size_t length = strlen(key);
			if (strncmp(line, key, length) == 0 && line[length] == '=')
			{
				count++;
				break;
			}
		}
	}
	fclose(file);
	return count;
}

static tapeheadUniversalPalette_t loadPalette(const char *path)
{
	char error[160];
	FILE *file = fopen(path, "rb");
	assert(file != NULL);
	tapeheadUniversalPalette_t palette;
	assert(tapeheadUniversalPaletteLoadStream(&palette, file, error,
		sizeof (error)));
	assert(fclose(file) == 0);
	return palette;
}

static void testTapeSisterSchemaAndRoundTrip(const char *assetPath,
	const char *temporaryDirectory)
{
	tapeheadUniversalPalette_t palette = loadPalette(assetPath);
	assert(palette.definedColors == TAPEHEAD_UNIVERSAL_ALL_COLORS_MASK);
	assert(palette.colors[TAPEHEAD_UNIVERSAL_PATTERN_TEXT] == 0xFF1C00);
	assert(palette.colors[TAPEHEAD_UNIVERSAL_PATTERN_TUNING] == 0x147DFF);
	assert(palette.colors[TAPEHEAD_UNIVERSAL_TRACK_LENGTH_PLAYHEAD] == 0x41D7FF);
	assert(palette.colors[TAPEHEAD_UNIVERSAL_FASTTRACKS_LENGTH_PLAYHEAD] ==
		0xD061FF);
	assert(palette.colors[TAPEHEAD_UNIVERSAL_WAVE_SELECTION] == 0x2D0039);
	assert(palette.colors[TAPEHEAD_UNIVERSAL_ACTIVE_TILE] == 0xFFD265);
	assert(palette.desktopContrast == 52 && palette.buttonsContrast == 57);

	palette.colors[TAPEHEAD_UNIVERSAL_WAVE_SELECTION] = 0x123456;
	palette.colors[TAPEHEAD_UNIVERSAL_ACTIVE_TILE] = 0xABCDEF;
	palette.colors[TAPEHEAD_UNIVERSAL_FASTTRACKS_PHASE] = 0x112233;
	char savedPath[1024];
	joinPath(savedPath, sizeof (savedPath), temporaryDirectory, "palette.pal");
	char error[160];
	FILE *saved = fopen(savedPath, "wb");
	assert(saved != NULL);
	assert(tapeheadUniversalPaletteSaveStream(&palette, saved, error,
		sizeof (error)));
	assert(fclose(saved) == 0);
	assert(fileContains(savedPath, "[Palette]"));
	assert(fileContains(savedPath, "WaveSelection=#123456"));
	assert(fileContains(savedPath, "ActiveTile=#ABCDEF"));
	assert(fileContains(savedPath, "FastTracksPhase=#112233"));
	assert(countColorKeys(savedPath) == TAPEHEAD_UNIVERSAL_COLOR_COUNT);

	const tapeheadUniversalPalette_t reopened = loadPalette(savedPath);
	assert(reopened.definedColors == TAPEHEAD_UNIVERSAL_ALL_COLORS_MASK);
	assert(memcmp(reopened.colors, palette.colors, sizeof (palette.colors)) == 0);
	assert(reopened.desktopContrast == palette.desktopContrast);
	assert(reopened.buttonsContrast == palette.buttonsContrast);
}

static void testLegacyAndEyedropper(const char *temporaryDirectory)
{
	char legacyPath[1024];
	joinPath(legacyPath, sizeof (legacyPath), temporaryDirectory,
		"tapehead.pal");
	writeText(legacyPath,
		"[TapeheadPalette]\n"
		"PatternText=#102030\n"
		"BlockMark=#203040\n"
		"TextOnBlock=#304050\n"
		"Mouse=#405060\n"
		"Desktop=#506070\n"
		"Buttons=#607080\n"
		"DesktopContrast=44\n"
		"ButtonsContrast=66\n");
	tapeheadUniversalPalette_t palette = loadPalette(legacyPath);
	assert(palette.colors[TAPEHEAD_UNIVERSAL_PATTERN_NOTE] == 0x102030);
	assert(palette.colors[TAPEHEAD_UNIVERSAL_PATTERN_EMPTY] == 0x102030);
	assert(palette.colors[TAPEHEAD_UNIVERSAL_WAVE_SELECTION] == 0x203040);
	assert(palette.colors[TAPEHEAD_UNIVERSAL_ACTIVE_TILE] == 0x405060);
	assert(tapeheadUniversalPaletteColorIsDefined(&palette,
		TAPEHEAD_UNIVERSAL_PATTERN_TEXT));
	assert(!tapeheadUniversalPaletteColorIsDefined(&palette,
		TAPEHEAD_UNIVERSAL_PATTERN_NOTE));
	assert(!tapeheadUniversalPaletteColorIsDefined(&palette,
		TAPEHEAD_UNIVERSAL_WAVE_SELECTION));
	assert(tapeheadUniversalPaletteTapeSisterSwatchDisplayColor(&palette, 12) ==
		TAPEHEAD_UNIVERSAL_UNSET_SWATCH_RGB);

	const tapeheadUniversalColor_t destination =
		tapeheadUniversalPaletteTapeheadColor(18);
	const uint32_t before = palette.colors[destination];
	assert(!tapeheadUniversalPaletteSampleTapeSister(&palette, 18, 12));
	assert(palette.colors[destination] == before);
	assert(tapeheadUniversalPaletteSampleTapeSister(&palette, 18, 0));
	assert(palette.colors[destination] == 0x102030);
	assert(tapeheadUniversalPaletteColorIsDefined(&palette, destination));
	{
		tapeheadUniversalPalette_t suggestions = palette;
		tapeheadUniversalPalette_t edited = palette;
		suggestions.colors[TAPEHEAD_UNIVERSAL_PATTERN_TEXT] = 0xA1B2C3;
		edited.colors[TAPEHEAD_UNIVERSAL_PATTERN_TEXT] = 0x010203;
		assert(tapeheadUniversalPaletteSampleTapeSisterFrom(&edited,
			&suggestions, 18, 0));
		assert(edited.colors[destination] == 0xA1B2C3);
		assert(suggestions.colors[TAPEHEAD_UNIVERSAL_PATTERN_TEXT] ==
			0xA1B2C3);
	}
	assert(!tapeheadUniversalPaletteSampleTapeSister(&palette,
		TAPEHEAD_UNIVERSAL_TAPEHEAD_COLOR_COUNT, 0));
	assert(tapeheadUniversalPaletteTapeheadColor(18) ==
		TAPEHEAD_UNIVERSAL_FASTTRACKS_LENGTH_PLAYHEAD);
	assert(tapeheadUniversalPaletteTapeheadColor(19) ==
		(tapeheadUniversalColor_t)-1);
	assert(strcmp(tapeheadUniversalPaletteTapeSisterSwatchName(6),
		"WAVEFORM") == 0);
	assert(strcmp(tapeheadUniversalPaletteTapeSisterSwatchName(13),
		"ACTIVE TILE") == 0);
}

static void testCanonicalSectionAndPaths(const char *temporaryDirectory)
{
	char canonicalPath[1024];
	joinPath(canonicalPath, sizeof (canonicalPath), temporaryDirectory,
		"canonical-minimal.pal");
	writeText(canonicalPath,
		"[Palette]\n"
		"PatternText=#010203\nBlockMark=#020304\n"
		"TextOnBlock=#030405\nMouse=#040506\n"
		"Desktop=#050607\nButtons=#060708\n");
	const tapeheadUniversalPalette_t canonical = loadPalette(canonicalPath);
	assert(canonical.colors[TAPEHEAD_UNIVERSAL_PATTERN_TEXT] == 0x010203);
	assert(canonical.colors[TAPEHEAD_UNIVERSAL_ACTIVE_TILE] == 0x040506);

	char path[1024];
	assert(tapeheadUniversalPaletteResolvePath(path, sizeof (path),
		"/tmp/tape exchange", "/home/user/.config/FT2.CFG", "palette.pal"));
	assert(strcmp(path, "/tmp/tape exchange/palette.pal") == 0);
	assert(tapeheadUniversalPaletteResolvePath(path, sizeof (path), "",
		"/home/user/.config/FT2.CFG", "palette.pal"));
	assert(strcmp(path, "/home/user/.config/palette.pal") == 0);
	assert(tapeheadUniversalPaletteResolvePath(path, sizeof (path), NULL,
		"C:\\Users\\Artist\\FT2.CFG", "tapehead.pal"));
	assert(strcmp(path, "C:\\Users\\Artist\\tapehead.pal") == 0);
	assert(tapeheadUniversalPaletteResolvePath(path, sizeof (path),
		"C:\\Exchange\\", "C:\\Users\\Artist\\FT2.CFG", "palette.pal"));
	assert(strcmp(path, "C:\\Exchange\\palette.pal") == 0);
	assert(tapeheadUniversalPaletteResolvePath(path, sizeof (path),
		"C:\\Exchange", "C:\\Users\\Artist\\FT2.CFG", "palette.pal"));
	assert(strcmp(path, "C:\\Exchange\\palette.pal") == 0);
}

int main(int argc, char **argv)
{
	assert(argc == 3);
	testTapeSisterSchemaAndRoundTrip(argv[1], argv[2]);
	testLegacyAndEyedropper(argv[2]);
	testCanonicalSectionAndPaths(argv[2]);
	puts("Universal palette schema, legacy, eyedropper, and path tests passed.");
	return 0;
}
