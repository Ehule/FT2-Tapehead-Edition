#include "ft2_universal_palette.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define UNIVERSAL_RGB(r, g, b) \
	(((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

static const char *const universalColorKeys[TAPEHEAD_UNIVERSAL_COLOR_COUNT] =
{
	"PatternText", "BlockMark", "TextOnBlock", "Mouse", "Desktop", "Buttons",
	"PatternNote", "PatternInstrument", "PatternVolume", "PatternTuning",
	"PatternEffect", "PatternEmpty", "WaveSelection", "ActiveTile",
	"TrackLengthPlayhead", "FastTracksPlayhead", "ControlPlayhead",
	"FastTracksSync", "FastTracksPhase", "FastTracksSong",
	"FastTracksLengthPlayhead"
};

static const tapeheadUniversalColor_t universalTapeheadColors[
	TAPEHEAD_UNIVERSAL_TAPEHEAD_COLOR_COUNT] =
{
	TAPEHEAD_UNIVERSAL_PATTERN_TEXT, TAPEHEAD_UNIVERSAL_BLOCK_MARK,
	TAPEHEAD_UNIVERSAL_TEXT_ON_BLOCK, TAPEHEAD_UNIVERSAL_MOUSE,
	TAPEHEAD_UNIVERSAL_DESKTOP, TAPEHEAD_UNIVERSAL_BUTTONS,
	TAPEHEAD_UNIVERSAL_PATTERN_NOTE, TAPEHEAD_UNIVERSAL_PATTERN_INSTRUMENT,
	TAPEHEAD_UNIVERSAL_PATTERN_VOLUME, TAPEHEAD_UNIVERSAL_PATTERN_TUNING,
	TAPEHEAD_UNIVERSAL_PATTERN_EFFECT, TAPEHEAD_UNIVERSAL_PATTERN_EMPTY,
	TAPEHEAD_UNIVERSAL_TRACK_LENGTH_PLAYHEAD,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_PLAYHEAD,
	TAPEHEAD_UNIVERSAL_CONTROL_PLAYHEAD, TAPEHEAD_UNIVERSAL_FASTTRACKS_SYNC,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_PHASE, TAPEHEAD_UNIVERSAL_FASTTRACKS_SONG,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_LENGTH_PLAYHEAD
};

static const tapeheadUniversalColor_t universalTapeSisterSwatches[
	TAPEHEAD_UNIVERSAL_TAPESISTER_SWATCH_COUNT] =
{
	TAPEHEAD_UNIVERSAL_PATTERN_TEXT, TAPEHEAD_UNIVERSAL_BLOCK_MARK,
	TAPEHEAD_UNIVERSAL_TEXT_ON_BLOCK, TAPEHEAD_UNIVERSAL_MOUSE,
	TAPEHEAD_UNIVERSAL_DESKTOP, TAPEHEAD_UNIVERSAL_BUTTONS,
	TAPEHEAD_UNIVERSAL_PATTERN_NOTE, TAPEHEAD_UNIVERSAL_PATTERN_INSTRUMENT,
	TAPEHEAD_UNIVERSAL_PATTERN_VOLUME, TAPEHEAD_UNIVERSAL_PATTERN_TUNING,
	TAPEHEAD_UNIVERSAL_PATTERN_EFFECT, TAPEHEAD_UNIVERSAL_PATTERN_EMPTY,
	TAPEHEAD_UNIVERSAL_WAVE_SELECTION, TAPEHEAD_UNIVERSAL_ACTIVE_TILE
};

static const char *const universalTapeSisterSwatchNames[
	TAPEHEAD_UNIVERSAL_TAPESISTER_SWATCH_COUNT] =
{
	"TITLE/TEXT", "ACTIVE CTRL", "ACTIVE TEXT", "POINTER", "DESKTOP",
	"CONTROLS", "WAVEFORM", "PRIMARY", "EDGE/ZERO", "LOOP/DRIFT",
	"EFFECT", "SPARE", "WAVE SELECT", "ACTIVE TILE"
};

static const tapeheadUniversalColor_t universalSaveOrder[
	TAPEHEAD_UNIVERSAL_COLOR_COUNT] =
{
	TAPEHEAD_UNIVERSAL_PATTERN_TEXT, TAPEHEAD_UNIVERSAL_BLOCK_MARK,
	TAPEHEAD_UNIVERSAL_TEXT_ON_BLOCK, TAPEHEAD_UNIVERSAL_MOUSE,
	TAPEHEAD_UNIVERSAL_DESKTOP, TAPEHEAD_UNIVERSAL_BUTTONS,
	TAPEHEAD_UNIVERSAL_PATTERN_NOTE, TAPEHEAD_UNIVERSAL_PATTERN_INSTRUMENT,
	TAPEHEAD_UNIVERSAL_PATTERN_VOLUME, TAPEHEAD_UNIVERSAL_PATTERN_TUNING,
	TAPEHEAD_UNIVERSAL_PATTERN_EFFECT, TAPEHEAD_UNIVERSAL_PATTERN_EMPTY,
	TAPEHEAD_UNIVERSAL_TRACK_LENGTH_PLAYHEAD,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_PLAYHEAD,
	TAPEHEAD_UNIVERSAL_CONTROL_PLAYHEAD, TAPEHEAD_UNIVERSAL_FASTTRACKS_SYNC,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_PHASE, TAPEHEAD_UNIVERSAL_FASTTRACKS_SONG,
	TAPEHEAD_UNIVERSAL_FASTTRACKS_LENGTH_PLAYHEAD,
	TAPEHEAD_UNIVERSAL_WAVE_SELECTION, TAPEHEAD_UNIVERSAL_ACTIVE_TILE
};

static void setError(char *error, size_t errorSize, const char *message)
{
	if (error != NULL && errorSize > 0)
		snprintf(error, errorSize, "%s", message != NULL ? message : "");
}

static char *trimText(char *text)
{
	while (*text != '\0' && isspace((unsigned char)*text)) text++;
	char *end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1])) end--;
	*end = '\0';
	return text;
}

static bool equalNoCase(const char *left, const char *right)
{
	while (*left != '\0' && *right != '\0')
	{
		if (tolower((unsigned char)*left) != tolower((unsigned char)*right))
			return false;
		left++;
		right++;
	}
	return *left == *right;
}

static bool parseColor(const char *text, uint32_t *color)
{
	char *end;
	unsigned long value;
	if (text == NULL || color == NULL)
		return false;
	if (text[0] == '#')
		text++;
	else if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
		text += 2;
	if (strlen(text) != 6)
		return false;
	for (int32_t i = 0; i < 6; i++)
	{
		if (!isxdigit((unsigned char)text[i]))
			return false;
	}
	errno = 0;
	value = strtoul(text, &end, 16);
	if (errno != 0 || *end != '\0' || value > 0xFFFFFFUL)
		return false;
	*color = (uint32_t)value;
	return true;
}

static bool parseContrast(const char *text, uint8_t *contrast)
{
	char *end;
	unsigned long value;
	if (text == NULL || contrast == NULL)
		return false;
	errno = 0;
	value = strtoul(text, &end, 10);
	while (*end != '\0' && isspace((unsigned char)*end)) end++;
	if (errno != 0 || end == text || *end != '\0' || value < 1 || value > 100)
		return false;
	*contrast = (uint8_t)value;
	return true;
}

void tapeheadUniversalPaletteDefault(tapeheadUniversalPalette_t *palette)
{
	static const uint32_t defaults[TAPEHEAD_UNIVERSAL_COLOR_COUNT] =
	{
		UNIVERSAL_RGB(255, 28, 0), UNIVERSAL_RGB(45, 0, 57),
		UNIVERSAL_RGB(0, 158, 227), UNIVERSAL_RGB(255, 210, 101),
		UNIVERSAL_RGB(28, 28, 28), UNIVERSAL_RGB(93, 85, 93),
		UNIVERSAL_RGB(255, 231, 0), UNIVERSAL_RGB(24, 255, 0),
		UNIVERSAL_RGB(255, 28, 231), UNIVERSAL_RGB(20, 125, 255),
		UNIVERSAL_RGB(53, 255, 255), UNIVERSAL_RGB(89, 0, 255),
		UNIVERSAL_RGB(45, 0, 57), UNIVERSAL_RGB(255, 210, 101),
		UNIVERSAL_RGB(65, 215, 255), UNIVERSAL_RGB(255, 174, 32),
		UNIVERSAL_RGB(255, 49, 49), UNIVERSAL_RGB(0, 206, 65),
		UNIVERSAL_RGB(255, 49, 49), UNIVERSAL_RGB(255, 174, 32),
		UNIVERSAL_RGB(208, 97, 255)
	};
	if (palette == NULL)
		return;
	memcpy(palette->colors, defaults, sizeof (defaults));
	palette->definedColors = TAPEHEAD_UNIVERSAL_ALL_COLORS_MASK;
	palette->desktopContrast = 52;
	palette->buttonsContrast = 57;
}

const char *tapeheadUniversalPaletteColorKey(tapeheadUniversalColor_t color)
{
	return color >= 0 && color < TAPEHEAD_UNIVERSAL_COLOR_COUNT ?
		universalColorKeys[color] : "Color";
}

bool tapeheadUniversalPaletteLoadStream(tapeheadUniversalPalette_t *palette,
	FILE *file, char *error, size_t errorSize)
{
	if (palette == NULL || file == NULL)
	{
		setError(error, errorSize, "Invalid palette source");
		return false;
	}

	tapeheadUniversalPalette_t loaded;
	tapeheadUniversalPaletteDefault(&loaded);
	loaded.definedColors = 0;
	bool found[TAPEHEAD_UNIVERSAL_COLOR_COUNT] = { false };
	bool inPaletteSection = true;
	bool valid = true;
	int32_t lineNumber = 0;
	char line[256];
	while (fgets(line, sizeof (line), file) != NULL)
	{
		lineNumber++;
		char *text = trimText(line);
		if (*text == '\0' || *text == ';' || *text == '#')
			continue;
		if (*text == '[')
		{
			char *close = strchr(text, ']');
			if (close == NULL)
			{
				valid = false;
				break;
			}
			*close = '\0';
			inPaletteSection = equalNoCase(text + 1, "Palette") ||
				equalNoCase(text + 1, "TapeheadPalette");
			continue;
		}
		if (!inPaletteSection)
			continue;

		char *equals = strchr(text, '=');
		if (equals == NULL)
		{
			valid = false;
			break;
		}
		*equals = '\0';
		char *key = trimText(text);
		char *value = trimText(equals + 1);
		bool recognized = false;
		for (int32_t color = 0; color < TAPEHEAD_UNIVERSAL_COLOR_COUNT; color++)
		{
			if (equalNoCase(key, universalColorKeys[color]))
			{
				recognized = true;
				if (!parseColor(value, &loaded.colors[color]))
					valid = false;
				else
				{
					found[color] = true;
					loaded.definedColors |= UINT32_C(1) << color;
				}
				break;
			}
		}
		if (!recognized && equalNoCase(key, "DesktopContrast"))
			valid = parseContrast(value, &loaded.desktopContrast);
		else if (!recognized && equalNoCase(key, "ButtonsContrast"))
			valid = parseContrast(value, &loaded.buttonsContrast);
		if (!valid)
			break;
	}
	if (ferror(file))
	{
		setError(error, errorSize, "Could not finish reading palette");
		return false;
	}
	for (int32_t color = 0; valid && color < 6; color++)
		valid = found[color];
	if (!valid)
	{
		if (error != NULL && errorSize > 0)
			snprintf(error, errorSize, "Malformed or incomplete palette line %d",
				lineNumber);
		return false;
	}

	for (int32_t color = TAPEHEAD_UNIVERSAL_PATTERN_NOTE;
		color <= TAPEHEAD_UNIVERSAL_PATTERN_EMPTY; color++)
	{
		if (!found[color])
			loaded.colors[color] = loaded.colors[TAPEHEAD_UNIVERSAL_PATTERN_TEXT];
	}
	if (!found[TAPEHEAD_UNIVERSAL_WAVE_SELECTION])
		loaded.colors[TAPEHEAD_UNIVERSAL_WAVE_SELECTION] =
			loaded.colors[TAPEHEAD_UNIVERSAL_BLOCK_MARK];
	if (!found[TAPEHEAD_UNIVERSAL_ACTIVE_TILE])
		loaded.colors[TAPEHEAD_UNIVERSAL_ACTIVE_TILE] =
			loaded.colors[TAPEHEAD_UNIVERSAL_MOUSE];

	*palette = loaded;
	setError(error, errorSize, "");
	return true;
}

bool tapeheadUniversalPaletteSaveStream(const tapeheadUniversalPalette_t *palette,
	FILE *file, char *error, size_t errorSize)
{
	if (palette == NULL || file == NULL)
	{
		setError(error, errorSize, "Invalid palette destination");
		return false;
	}
	bool failed = fprintf(file,
		"; Shared TapeSister / Tapehead palette\n"
		"; Both applications preserve every key in this file.\n\n"
		"[Palette]\n") < 0;
	for (int32_t i = 0; i < TAPEHEAD_UNIVERSAL_COLOR_COUNT; i++)
	{
		const tapeheadUniversalColor_t color = universalSaveOrder[i];
		const uint32_t value = palette->colors[color];
		failed |= fprintf(file, "%s=#%02X%02X%02X\n", universalColorKeys[color],
			(unsigned)((value >> 16) & 0xFF),
			(unsigned)((value >> 8) & 0xFF),
			(unsigned)(value & 0xFF)) < 0;
	}
	failed |= fprintf(file, "DesktopContrast=%u\nButtonsContrast=%u\n",
		palette->desktopContrast, palette->buttonsContrast) < 0;
	if (failed || ferror(file))
	{
		setError(error, errorSize, "Could not finish writing palette");
		return false;
	}
	setError(error, errorSize, "");
	return true;
}

bool tapeheadUniversalPaletteColorIsDefined(
	const tapeheadUniversalPalette_t *palette, tapeheadUniversalColor_t color)
{
	return palette != NULL && color >= 0 &&
		color < TAPEHEAD_UNIVERSAL_COLOR_COUNT &&
		(palette->definedColors & (UINT32_C(1) << color)) != 0;
}

tapeheadUniversalColor_t tapeheadUniversalPaletteTapeheadColor(int32_t index)
{
	return index >= 0 && index < TAPEHEAD_UNIVERSAL_TAPEHEAD_COLOR_COUNT ?
		universalTapeheadColors[index] : (tapeheadUniversalColor_t)-1;
}

tapeheadUniversalColor_t tapeheadUniversalPaletteTapeSisterSwatchColor(int32_t swatch)
{
	return swatch >= 0 && swatch < TAPEHEAD_UNIVERSAL_TAPESISTER_SWATCH_COUNT ?
		universalTapeSisterSwatches[swatch] : (tapeheadUniversalColor_t)-1;
}

const char *tapeheadUniversalPaletteTapeSisterSwatchName(int32_t swatch)
{
	return swatch >= 0 && swatch < TAPEHEAD_UNIVERSAL_TAPESISTER_SWATCH_COUNT ?
		universalTapeSisterSwatchNames[swatch] : "TAPESISTER";
}

uint32_t tapeheadUniversalPaletteTapeSisterSwatchDisplayColor(
	const tapeheadUniversalPalette_t *palette, int32_t swatch)
{
	const tapeheadUniversalColor_t color =
		tapeheadUniversalPaletteTapeSisterSwatchColor(swatch);
	return tapeheadUniversalPaletteColorIsDefined(palette, color) ?
		palette->colors[color] : TAPEHEAD_UNIVERSAL_UNSET_SWATCH_RGB;
}

bool tapeheadUniversalPaletteSampleTapeSister(tapeheadUniversalPalette_t *palette,
	int32_t tapeheadDestination, int32_t swatch)
{
	if (palette == NULL || tapeheadDestination < 0 ||
		tapeheadDestination >= TAPEHEAD_UNIVERSAL_TAPEHEAD_COLOR_COUNT)
	{
		return false;
	}
	const tapeheadUniversalColor_t source =
		tapeheadUniversalPaletteTapeSisterSwatchColor(swatch);
	if (!tapeheadUniversalPaletteColorIsDefined(palette, source))
		return false;
	const tapeheadUniversalColor_t destination =
		tapeheadUniversalPaletteTapeheadColor(tapeheadDestination);
	palette->colors[destination] = palette->colors[source];
	palette->definedColors |= UINT32_C(1) << destination;
	return true;
}

bool tapeheadUniversalPaletteResolvePath(char *path, size_t pathSize,
	const char *exchangeDirectory, const char *configFilePath,
	const char *filename)
{
	if (path == NULL || pathSize == 0 || filename == NULL || *filename == '\0')
		return false;
	const char *base = exchangeDirectory;
	size_t baseLength = base != NULL ? strlen(base) : 0;
	if (baseLength == 0)
	{
		base = configFilePath;
		baseLength = base != NULL ? strlen(base) : 0;
		while (baseLength > 0 && base[baseLength - 1] != '/' &&
			base[baseLength - 1] != '\\')
		{
			baseLength--;
		}
	}
	const bool addSeparator = exchangeDirectory != NULL &&
		*exchangeDirectory != '\0' && baseLength > 0 &&
		base[baseLength - 1] != '/' && base[baseLength - 1] != '\\';
	const size_t filenameLength = strlen(filename);
	if (baseLength + (addSeparator ? 1 : 0) + filenameLength >= pathSize)
		return false;
	if (baseLength > 0)
		memcpy(path, base, baseLength);
	size_t offset = baseLength;
	if (addSeparator)
	{
		/* Keep native-looking Windows paths intact, especially before they are
		** promoted to the extended-length \\?\ form by the UI integration. */
		path[offset++] = strchr(base, '\\') != NULL && strchr(base, '/') == NULL ?
			'\\' : '/';
	}
	memcpy(path + offset, filename, filenameLength + 1);
	return true;
}
