#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "ft2_header.h"
#include "ft2_palette.h"
#include "ft2_gui.h"
#include "ft2_config.h"
#include "ft2_video.h"
#include "ft2_tables.h"
#include "ft2_bmp.h"
#include "ft2_pattern_ed.h"
#include "ft2_replayer.h"
#include "ft2_structs.h"
#include "ft2_textboxes.h"

uint8_t cfg_ColorNum = 0; // globalized
static uint8_t paletteListOffset;
static pal16 patternColors[12][TAPEHEAD_CUSTOM_COLOR_COUNT];
static bool patternColorsInitialized;

static uint8_t cfg_Red, cfg_Green, cfg_Blue, cfg_Contrast;

#define PAL_LIST_FRAME_X 396
#define PAL_LIST_FRAME_Y 2
#define PAL_LIST_FRAME_W 106
#define PAL_LIST_FRAME_H 82
#define PAL_LIST_X 398
#define PAL_LIST_Y 4
#define PAL_LIST_TEXT_W 86
#define PAL_LIST_ROW_H 13
#define PAL_LIST_VISIBLE_ROWS 6

static const uint8_t FTC_EditOrder[TAPEHEAD_PALETTE_EDIT_COUNT] =
{
	PAL_PATTEXT, PAL_BLCKMRK, PAL_BLCKTXT, PAL_MOUSEPT, PAL_DESKTOP,
	PAL_BUTTONS, PAL_PATTERN_NOTE, PAL_PATTERN_INSTRUMENT,
	PAL_PATTERN_VOLUME, PAL_PATTERN_TUNING, PAL_PATTERN_EFFECT,
	PAL_PATTERN_EMPTY, PAL_TRACK_LENGTH_PLAYHEAD, PAL_FASTTRACKS_PLAYHEAD,
	PAL_CONTROL_PLAYHEAD, PAL_FASTTRACKS_SYNC, PAL_FASTTRACKS_PHASE,
	PAL_FASTTRACKS_SONG, PAL_FASTTRACKS_LENGTH_PLAYHEAD
};
static const uint8_t scaleOrder[3] = { 8, 4, 9 };
static const char *paletteFileKeys[TAPEHEAD_PALETTE_EDIT_COUNT] =
{
	"PatternText", "BlockMark", "TextOnBlock", "Mouse", "Desktop", "Buttons",
	"PatternNote", "PatternInstrument", "PatternVolume", "PatternTuning",
	"PatternEffect", "PatternEmpty", "TrackLengthPlayhead",
	"FastTracksPlayhead", "ControlPlayhead", "FastTracksSync",
	"FastTracksPhase", "FastTracksSong", "FastTracksLengthPlayhead"
};
static const char *paletteEntryNames[TAPEHEAD_PALETTE_EDIT_COUNT] =
{
	"PAT Text", "Block Mark", "Block Text", "Mouse", "Desktop", "Buttons",
	"PAT Note", "PAT Inst.", "PAT Volume", "PAT Tuning", "PAT Effect",
	"PAT Empty", "LEN Head", "FT Head", "CONTROL Head", "FT Sync LED",
	"FT Phase LED", "FT Song Badge", "FT+LEN Head"
};

static uint8_t color8To6(uint8_t color);

static void initPatternColors(void)
{
	if (patternColorsInitialized) return;
	for (int32_t layout = 0; layout < 12; layout++)
	{
		const pal16 text = palTable[layout][PAL_PATTEXT];
		const int8_t delta[6][3] = {{3,3,3}, {0,3,5}, {4,2,0}, {1,5,3}, {5,1,2}, {-10,-10,-10}};
		for (int32_t field = 0; field < 6; field++)
		{
			const int8_t *d = layout == PAL_USER_DEFINED ? (const int8_t[3]){0, 0, 0} : delta[field];
			patternColors[layout][field].r = (uint8_t)CLAMP((int32_t)text.r + d[0], 0, 63);
			patternColors[layout][field].g = (uint8_t)CLAMP((int32_t)text.g + d[1], 0, 63);
			patternColors[layout][field].b = (uint8_t)CLAMP((int32_t)text.b + d[2], 0, 63);
		}

		static const uint32_t transportDefaults[TAPEHEAD_TRANSPORT_COLOR_COUNT] =
		{
			0x40D8FF, 0xFFB020, 0xFF3030,
			0x00D040, 0xFF3030, 0xFFB020, 0xD060FF
		};
		for (int32_t field = 0; field < TAPEHEAD_TRANSPORT_COLOR_COUNT; field++)
		{
			const uint32_t rgb = transportDefaults[field];
			pal16 *dst = &patternColors[layout]
				[TAPEHEAD_PATTERN_FIELD_COLOR_COUNT + field];
			dst->r = color8To6(RGB32_R(rgb));
			dst->g = color8To6(RGB32_G(rgb));
			dst->b = color8To6(RGB32_B(rgb));
		}
	}
	patternColorsInitialized = true;
}

static uint8_t palContrast[12][2] = // palette desktop/button contrasts
{
	{59, 55}, {59, 53}, {56, 59}, {68, 55}, {57, 59}, {48, 55},
	{66, 62}, {68, 57}, {58, 42}, {57, 55}, {62, 57}, {52, 57}
};

static UNICHAR *getPaletteFilePathU(void)
{
	if (editor.configFileLocationU == NULL)
		return NULL;

	const size_t configPathLen = UNICHAR_STRLEN(editor.configFileLocationU);
#ifdef _WIN32
	const size_t configNameLen = UNICHAR_STRLEN(L"FT2.CFG");
	const size_t paletteNameLen = UNICHAR_STRLEN(L"tapehead.pal");
#else
	const size_t configNameLen = UNICHAR_STRLEN("FT2.CFG");
	const size_t paletteNameLen = UNICHAR_STRLEN("tapehead.pal");
#endif
	if (configPathLen < configNameLen)
		return NULL;

	UNICHAR *filePathU = (UNICHAR *)malloc((configPathLen - configNameLen + paletteNameLen + 1) * sizeof (UNICHAR));
	if (filePathU == NULL)
		return NULL;

	UNICHAR_STRCPY(filePathU, editor.configFileLocationU);
	filePathU[configPathLen - configNameLen] = 0;
#ifdef _WIN32
	UNICHAR_STRCAT(filePathU, L"tapehead.pal");
#else
	UNICHAR_STRCAT(filePathU, "tapehead.pal");
#endif
	return filePathU;
}

static char *trimPaletteText(char *s)
{
	while (isspace((unsigned char)*s)) s++;
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1])) end--;
	*end = '\0';
	return s;
}

static bool parsePaletteHex(const char *text, uint32_t *color)
{
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

	*color = (uint32_t)strtoul(text, NULL, 16);
	return true;
}

static bool parsePaletteContrast(const char *text, uint8_t *contrast)
{
	char *end;
	const unsigned long value = strtoul(text, &end, 10);
	while (isspace((unsigned char)*end)) end++;
	if (text == end || *end != '\0' || value < 1 || value > 100)
		return false;

	*contrast = (uint8_t)value;
	return true;
}

static uint8_t color8To6(uint8_t color)
{
	return (uint8_t)(((uint32_t)color * 63 + 127) / 255);
}

void setPalette(pal16 *p, bool redrawScreen)
{
#define LOOP_PIN_COL_SUB 96
#define TEXT_MARK_COLOR 0x0078D7
#define BOX_SELECT_COLOR 0x7F7F7F

	int16_t r8, g8, b8;

	// set main palette (w/ 6-bit -> 8-bit conversion)
	for (int32_t i = 0; i < 16; i++)
	{
		r8 = COLOR_6BIT_TO_8BIT(p[i].r);
		g8 = COLOR_6BIT_TO_8BIT(p[i].g);
		b8 = COLOR_6BIT_TO_8BIT(p[i].b);

		// MSB (0xXX000000) is used for palette number
		video.palette[i] = (i << 24) | RGB32(r8, g8, b8);
	}

	initPatternColors();
	for (int32_t field = 0; field < TAPEHEAD_CUSTOM_COLOR_COUNT; field++)
	{
		const pal16 c = patternColors[config.cfg_StdPalNum][field];
		video.palette[PAL_PATTERN_NOTE + field] = ((PAL_PATTERN_NOTE + field) << 24) |
			RGB32(COLOR_6BIT_TO_8BIT(c.r), COLOR_6BIT_TO_8BIT(c.g), COLOR_6BIT_TO_8BIT(c.b));
	}

	// set custom FT2 clone palette entries

	video.palette[PAL_TEXTMRK] = (PAL_TEXTMRK << 24) | TEXT_MARK_COLOR;
	video.palette[PAL_BOXSLCT] = (PAL_BOXSLCT << 24) | BOX_SELECT_COLOR;

	/*
	** Dedicated pattern cursor colors. Use a darker pair on bright themes and
	** a luminous pair on dark themes so the cursor remains obvious without
	** replacing or recoloring the pattern text inside the cell.
	*/
	const uint32_t patternBg = video.palette[PAL_DESKTOP];
	const uint32_t patternBgLuma = (RGB32_R(patternBg) * 54) +
	                               (RGB32_G(patternBg) * 183) +
	                               (RGB32_B(patternBg) * 19);
	if (patternBgLuma >= (160 * 256))
	{
		video.palette[PAL_CURSOR_NAV] = (PAL_CURSOR_NAV << 24) | RGB32(0, 104, 176);
		video.palette[PAL_CURSOR_EDIT] = (PAL_CURSOR_EDIT << 24) | RGB32(176, 104, 0);
	}
	else
	{
		video.palette[PAL_CURSOR_NAV] = (PAL_CURSOR_NAV << 24) | RGB32(40, 224, 255);
		video.palette[PAL_CURSOR_EDIT] = (PAL_CURSOR_EDIT << 24) | RGB32(255, 224, 32);
	}

	r8 = RGB32_R(video.palette[PAL_PATTEXT]);
	g8 = RGB32_G(video.palette[PAL_PATTEXT]);
	b8 = RGB32_B(video.palette[PAL_PATTEXT]);

	r8 = MAX(r8 - LOOP_PIN_COL_SUB, 0);
	g8 = MAX(g8 - LOOP_PIN_COL_SUB, 0);
	b8 = MAX(b8 - LOOP_PIN_COL_SUB, 0);

	video.palette[PAL_LOOPPIN] = (PAL_LOOPPIN << 24) | RGB32(r8, g8, b8);
	/* A theme-derived green keeps the trim strip inside the configurable
	** palette rather than baking a display-specific RGB value into scopes. */
	video.palette[PAL_TRACKTRIM_GREEN] = (PAL_TRACKTRIM_GREEN << 24) |
		RGB32(b8, r8, g8);

	/* Repaint grayscale custom artwork through the newly selected theme. */
	refreshFastTracksLogoTheme();

	// update framebuffer pixels with new palette
	if (redrawScreen && video.frameBuffer != NULL)
	{
		for (int32_t i = 0; i < SCREEN_W*SCREEN_H; i++)
		{
			const uint8_t paletteIndex = (uint8_t)(video.frameBuffer[i] >> 24);
			/* Index zero is valid. Values outside our tagged palette are true-color
			** pixels and must not be interpreted as palette offsets. */
			if (paletteIndex < PAL_NUM)
				video.frameBuffer[i] = video.palette[paletteIndex];
		}

		/* The custom logo is true-color, so draw its refreshed themed copy again. */
		changeLogoType(0);
	}
}

static void showColorErrorMsg(void)
{
	okBox(0, "System message", "Default colors cannot be modified.", NULL);
}

static void showMouseColorErrorMsg(void)
{
	okBox(0, "System message", "Mouse color can only be changed when \"Software mouse\" is enabled.", NULL);
}

static void drawCurrentPaletteColor(void)
{
	const uint8_t palIndex = FTC_EditOrder[cfg_ColorNum];

	const uint8_t r8 = COLOR_6BIT_TO_8BIT(cfg_Red);
	const uint8_t g8 = COLOR_6BIT_TO_8BIT(cfg_Green);
	const uint8_t b8 = COLOR_6BIT_TO_8BIT(cfg_Blue);

	textOutShadow(516, 3, PAL_FORGRND, PAL_DSKTOP2, "Palette:");
	hexOutBg(573, 3, PAL_FORGRND, PAL_DESKTOP, RGB32(r8, g8, b8) & 0xFFFFFF, 6);
	clearRect(616, 2, 12, 10);
	fillRect(617, 3, 10, 8, palIndex);
}

static void updatePaletteEditor(void)
{
	const uint8_t colorNum = FTC_EditOrder[cfg_ColorNum];
	const pal16 color = cfg_ColorNum < 6 ? palTable[config.cfg_StdPalNum][colorNum] :
		patternColors[config.cfg_StdPalNum][cfg_ColorNum - 6];
	cfg_Red = color.r; cfg_Green = color.g; cfg_Blue = color.b;

	if (cfg_ColorNum == 4 || cfg_ColorNum == 5)
		cfg_Contrast = palContrast[config.cfg_StdPalNum][cfg_ColorNum-4];
	else
		cfg_Contrast = 0;

	setScrollBarPos(SB_PAL_R, cfg_Red, DONT_TRIGGER_CALLBACK);
	setScrollBarPos(SB_PAL_G, cfg_Green, DONT_TRIGGER_CALLBACK);
	setScrollBarPos(SB_PAL_B, cfg_Blue, DONT_TRIGGER_CALLBACK);
	setScrollBarPos(SB_PAL_CONTRAST, cfg_Contrast, DONT_TRIGGER_CALLBACK);

	drawCurrentPaletteColor();
}

static float fPalPow(float x, float y)
{
	if (y == 1.0f)
		return x;

	y *= logf(fabsf(x));
	y = CLAMP(y, -86.0f, 86.0f);

	return expf(y);
}

static void applyPaletteContrast(uint8_t layout, uint8_t editIndex, uint8_t contrast)
{
	if (contrast < 1)
		contrast = 1;

	const uint8_t colorNum = FTC_EditOrder[editIndex];
	const float fR = (float)palTable[layout][colorNum].r;
	const float fG = (float)palTable[layout][colorNum].g;
	const float fB = (float)palTable[layout][colorNum].b;
	const float fContrast = (float)contrast * (1.0f / 40.0f);

	for (int32_t i = 0; i < 3; i++)
	{
		const int32_t pal = scaleOrder[i] + (editIndex - 4) * 2;
		const float fMul = fPalPow((float)(i + 1) * 0.5f, fContrast);

		const int32_t r6 = (int32_t)((fR * fMul) + 0.5f);
		const int32_t g6 = (int32_t)((fG * fMul) + 0.5f);
		const int32_t b6 = (int32_t)((fB * fMul) + 0.5f);

		palTable[layout][pal].r = (uint8_t)CLAMP(r6, 0, 63);
		palTable[layout][pal].g = (uint8_t)CLAMP(g6, 0, 63);
		palTable[layout][pal].b = (uint8_t)CLAMP(b6, 0, 63);
	}

	palContrast[layout][editIndex - 4] = contrast;
}

static void paletteDragMoved(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
	{
		updatePaletteEditor(); // resets colors/contrast vars
		showColorErrorMsg();
		return;
	}

	if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
	{
		updatePaletteEditor(); // resets colors/contrast vars
		showMouseColorErrorMsg();
		return;
	}

	const uint8_t colorNum = FTC_EditOrder[cfg_ColorNum];
	const uint8_t layout = (uint8_t)config.cfg_StdPalNum;

	pal16 *editedColor = cfg_ColorNum < 6 ? &palTable[layout][colorNum] :
		&patternColors[layout][cfg_ColorNum - 6];
	editedColor->r = cfg_Red; editedColor->g = cfg_Green; editedColor->b = cfg_Blue;

	if (cfg_ColorNum == 4 || cfg_ColorNum == 5)
	{
		applyPaletteContrast(layout, cfg_ColorNum, cfg_Contrast);
	}
	else
	{
		cfg_Contrast = 0;

		setScrollBarPos(SB_PAL_R, cfg_Red, DONT_TRIGGER_CALLBACK);
		setScrollBarPos(SB_PAL_G, cfg_Green, DONT_TRIGGER_CALLBACK);
		setScrollBarPos(SB_PAL_B, cfg_Blue, DONT_TRIGGER_CALLBACK);
	}

	setScrollBarPos(SB_PAL_CONTRAST, cfg_Contrast, DONT_TRIGGER_CALLBACK);
	drawCurrentPaletteColor();

	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
}

void sbPalRPos(uint32_t pos)
{
	if (cfg_Red != (uint8_t)pos)
	{
		cfg_Red = (uint8_t)pos;
		paletteDragMoved();
	}
}

void sbPalGPos(uint32_t pos)
{
	if (cfg_Green != (uint8_t)pos)
	{
		cfg_Green = (uint8_t)pos;
		paletteDragMoved();
	}
}

void sbPalBPos(uint32_t pos)
{
	if (cfg_Blue != (uint8_t)pos)
	{
		cfg_Blue = (uint8_t)pos;
		paletteDragMoved();
	}
}

void sbPalContrastPos(uint32_t pos)
{
	if (cfg_Contrast != (uint8_t)pos)
	{
		cfg_Contrast = (uint8_t)pos;
		paletteDragMoved();
	}
}

void configPalRDown(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_R, 1);
}

void configPalRUp(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_R, 1);
}

void configPalGDown(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_G, 1);
}

void configPalGUp(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_G, 1);
}

void configPalBDown(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_B, 1);
}

void configPalBUp(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_B, 1);
}

void configPalContDown(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollLeft(SB_PAL_CONTRAST, 1);
}

void configPalContUp(void)
{
	if (config.cfg_StdPalNum != PAL_USER_DEFINED)
		showColorErrorMsg();
	else if ((config.specialFlags2 & HARDWARE_MOUSE) && cfg_ColorNum == 3)
		showMouseColorErrorMsg();
	else
		scrollBarScrollRight(SB_PAL_CONTRAST, 1);
}

void configPalImport(void)
{
	UNICHAR *filePathU = getPaletteFilePathU();
	if (filePathU == NULL)
	{
		okBox(0, "System message", "Couldn't locate the palette file directory.", NULL);
		return;
	}

	FILE *f = UNICHAR_FOPEN(filePathU, "r");
	free(filePathU);
	if (f == NULL)
	{
		okBox(0, "System message", "Couldn't open tapehead.pal for reading.", NULL);
		return;
	}

	uint32_t colors[TAPEHEAD_PALETTE_EDIT_COUNT] = { 0 };
	bool colorFound[TAPEHEAD_PALETTE_EDIT_COUNT] = { false };
	uint8_t contrasts[2] =
	{
		palContrast[PAL_USER_DEFINED][0], palContrast[PAL_USER_DEFINED][1]
	};
	bool valid = true;
	bool inPaletteSection = true;
	char line[256];

	while (fgets(line, sizeof (line), f) != NULL)
	{
		char *text = trimPaletteText(line);
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
			inPaletteSection = !_stricmp(text + 1, "TapeheadPalette") ||
				!_stricmp(text + 1, "Palette");
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
		char *key = trimPaletteText(text);
		char *value = trimPaletteText(equals + 1);
		bool recognized = false;

		for (int32_t i = 0; i < TAPEHEAD_PALETTE_EDIT_COUNT; i++)
		{
			if (!_stricmp(key, paletteFileKeys[i]))
			{
				recognized = true;
				if (!parsePaletteHex(value, &colors[i]))
					valid = false;
				else
					colorFound[i] = true;
				break;
			}
		}

		if (!recognized && !_stricmp(key, "DesktopContrast"))
		{
			recognized = true;
			valid = parsePaletteContrast(value, &contrasts[0]);
		}
		else if (!recognized && !_stricmp(key, "ButtonsContrast"))
		{
			recognized = true;
			valid = parsePaletteContrast(value, &contrasts[1]);
		}

		if (!valid)
			break;
	}

	fclose(f);
	/* The original six keys remain mandatory. Missing Phase-2 fields inherit
	** PatternText, making every older tapehead.pal valid and monochrome-safe. */
	for (int32_t i = 0; i < 6; i++)
	{
		if (!colorFound[i])
			valid = false;
	}

	if (!valid)
	{
		okBox(0, "System message", "tapehead.pal is invalid or incomplete.", NULL);
		return;
	}

	for (int32_t i = 0; i < TAPEHEAD_PALETTE_EDIT_COUNT; i++)
	{
		if (i >= 6 && i < 12 && !colorFound[i]) colors[i] = colors[0];
		if (i >= 12 && !colorFound[i]) continue; /* retain new color defaults */
		pal16 *dst = i < 6 ? &palTable[PAL_USER_DEFINED][FTC_EditOrder[i]] :
			&patternColors[PAL_USER_DEFINED][i - 6];
		dst->r = color8To6((uint8_t)(colors[i] >> 16));
		dst->g = color8To6((uint8_t)(colors[i] >> 8));
		dst->b = color8To6((uint8_t)colors[i]);
	}

	applyPaletteContrast(PAL_USER_DEFINED, 4, contrasts[0]);
	applyPaletteContrast(PAL_USER_DEFINED, 5, contrasts[1]);
	rbConfigPalUserDefined();
	okBox(0, "System message", "Imported Tapehead palette from tapehead.pal.", NULL);
}

void configPalExport(void)
{
	UNICHAR *filePathU = getPaletteFilePathU();
	if (filePathU == NULL)
	{
		okBox(0, "System message", "Couldn't locate the palette file directory.", NULL);
		return;
	}

	FILE *f = UNICHAR_FOPEN(filePathU, "w");
	free(filePathU);
	if (f == NULL)
	{
		okBox(0, "System message", "Couldn't open tapehead.pal for writing.", NULL);
		return;
	}

	const uint8_t layout = (uint8_t)config.cfg_StdPalNum;
	fputs("; Tapehead Edition palette\n", f);
	fputs("; Copy this file between installations or edit the hex values manually.\n", f);
	fputs("; Press I in Config > Layout to import it into User defined.\n\n", f);
	fputs("[TapeheadPalette]\n", f);
	for (int32_t i = 0; i < TAPEHEAD_PALETTE_EDIT_COUNT; i++)
	{
		const pal16 color = i < 6 ? palTable[layout][FTC_EditOrder[i]] : patternColors[layout][i - 6];
		fprintf(f, "%s=#%02X%02X%02X\n", paletteFileKeys[i],
			COLOR_6BIT_TO_8BIT(color.r), COLOR_6BIT_TO_8BIT(color.g),
			COLOR_6BIT_TO_8BIT(color.b));
	}
	fprintf(f, "DesktopContrast=%u\n", palContrast[layout][0]);
	fprintf(f, "ButtonsContrast=%u\n", palContrast[layout][1]);

	bool writeFailed = ferror(f) != 0;
	if (fclose(f) != 0)
		writeFailed = true;
	if (writeFailed)
		okBox(0, "System message", "General I/O error while writing tapehead.pal.", NULL);
	else
		okBox(0, "System message", "Exported Tapehead palette to tapehead.pal.", NULL);
}

void showPaletteEditor(void)
{
	drawFramework(PAL_LIST_FRAME_X, PAL_LIST_FRAME_Y, PAL_LIST_FRAME_W, PAL_LIST_FRAME_H, FRAMEWORK_TYPE2);
	clearRect(PAL_LIST_X, PAL_LIST_Y, PAL_LIST_TEXT_W, PAL_LIST_ROW_H * PAL_LIST_VISIBLE_ROWS);
	for (int32_t slot = 0; slot < PAL_LIST_VISIBLE_ROWS; slot++)
	{
		const uint8_t entry = paletteListOffset + slot;
		const uint16_t y = (uint16_t)(PAL_LIST_Y + 2 + slot * PAL_LIST_ROW_H);
		if (entry == cfg_ColorNum) fillRect(398, y - 1, 86, 11, PAL_BOXSLCT);
		textOutClipX(400, y, PAL_FORGRND, paletteEntryNames[entry], 482);
	}
	clearRect(398, 87, 232, 86);
	static const char *presetNames[12] = { "Arctic", "LiTHe dark", "Aurora Borealis", "Rose", "Blues", "Dark mode", "Gold", "Violent", "Heavy Metal", "Why colors?", "Jungle", "User defined" };
	static const char *modeNames[3] = { "Edit", "Always", "Mono" };
	textOutShadow(400,  92, PAL_FORGRND, PAL_DSKTOP2, "Preset:");
	textOutShadow(400, 109, PAL_FORGRND, PAL_DSKTOP2, "PAT Colors:");
	textOutShadow(400, 126, PAL_FORGRND, PAL_DSKTOP2, "Exchange:");
	textOutShadow(400, 143, PAL_FORGRND, PAL_DSKTOP2, "Program:");
	drawFramework(474, 122, 156, 14, FRAMEWORK_TYPE2);
	drawFramework(474, 139, 156, 14, FRAMEWORK_TYPE2);
	showTextBox(TB_CONF_TAPESISTER_EXCHANGE);
	showTextBox(TB_CONF_TAPESISTER_EXECUTABLE);
	drawTextBox(TB_CONF_TAPESISTER_EXCHANGE);
	drawTextBox(TB_CONF_TAPESISTER_EXECUTABLE);
	textOutShadow(400, 158, PAL_FORGRND, PAL_DSKTOP2, "Double-click a path to browse");
	pushButtons[PB_CONFIG_PAL_PRESET].caption = (char *)presetNames[config.cfg_StdPalNum];
	pushButtons[PB_CONFIG_PAL_COLOR_MODE].caption = (char *)modeNames[MIN(tapeheadConfig.patternColorMode, 2)];
	charOutShadow(503, 17, PAL_FORGRND, PAL_DSKTOP2, 'R');
	charOutShadow(503, 31, PAL_FORGRND, PAL_DSKTOP2, 'G');
	charOutShadow(503, 45, PAL_FORGRND, PAL_DSKTOP2, 'B');

	showScrollBar(SB_PAL_R);
	showScrollBar(SB_PAL_G);
	showScrollBar(SB_PAL_B);
	showPushButton(PB_CONFIG_PAL_R_DOWN);
	showPushButton(PB_CONFIG_PAL_R_UP);
	showPushButton(PB_CONFIG_PAL_G_DOWN);
	showPushButton(PB_CONFIG_PAL_G_UP);
	showPushButton(PB_CONFIG_PAL_B_DOWN);
	showPushButton(PB_CONFIG_PAL_B_UP);

	setScrollBarPos(SB_PAL_LIST, paletteListOffset, DONT_TRIGGER_CALLBACK);
	showScrollBar(SB_PAL_LIST);
	showPushButton(PB_CONFIG_PAL_PRESET);
	showPushButton(PB_CONFIG_PAL_COLOR_MODE);

	textOutShadow(503, 59, PAL_FORGRND, PAL_DSKTOP2, "Contrast:");
	showScrollBar(SB_PAL_CONTRAST);
	showPushButton(PB_CONFIG_PAL_CONT_DOWN);
	showPushButton(PB_CONFIG_PAL_CONT_UP);
	showPushButton(PB_CONFIG_PAL_IMPORT);
	showPushButton(PB_CONFIG_PAL_EXPORT);

	updatePaletteEditor();
}

void rbConfigPalPatternText(void)
{
	cfg_ColorNum = paletteListOffset;
	checkRadioButton(RB_CONFIG_PAL_PATTERNTEXT);
	updatePaletteEditor();
}

void rbConfigPalBlockMark(void)
{
	cfg_ColorNum = paletteListOffset + 1;
	checkRadioButton(RB_CONFIG_PAL_BLOCKMARK);
	updatePaletteEditor();
}

void rbConfigPalTextOnBlock(void)
{
	cfg_ColorNum = paletteListOffset + 2;
	checkRadioButton(RB_CONFIG_PAL_TEXTONBLOCK);
	updatePaletteEditor();
}

void rbConfigPalMouse(void)
{
	cfg_ColorNum = paletteListOffset + 3;
	checkRadioButton(RB_CONFIG_PAL_MOUSE);
	updatePaletteEditor();
}

void rbConfigPalDesktop(void)
{
	cfg_ColorNum = paletteListOffset + 4;
	checkRadioButton(RB_CONFIG_PAL_DESKTOP);
	updatePaletteEditor();
}

void rbConfigPalButttons(void)
{
	cfg_ColorNum = paletteListOffset + 5;
	checkRadioButton(RB_CONFIG_PAL_BUTTONS);
	updatePaletteEditor();
}

void rbConfigPalArctic(void)
{
	config.cfg_StdPalNum = PAL_ARCTIC;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_ARCTIC);
}

void rbConfigPalLitheDark(void)
{
	config.cfg_StdPalNum = PAL_LITHE_DARK;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_LITHE_DARK);
}

void rbConfigPalAuroraBorealis(void)
{
	config.cfg_StdPalNum = PAL_AURORA_BOREALIS;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_AURORA_BOREALIS);
}

void rbConfigPalRose(void)
{
	config.cfg_StdPalNum = PAL_ROSE;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_ROSE);
}

void rbConfigPalBlues(void)
{
	config.cfg_StdPalNum = PAL_BLUES;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_BLUES);
}

void rbConfigPalDarkMode(void)
{
	config.cfg_StdPalNum = PAL_DARK_MODE;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_DARK_MODE);
}

void rbConfigPalGold(void)
{
	config.cfg_StdPalNum = PAL_GOLD;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_GOLD);
}

void rbConfigPalViolent(void)
{
	config.cfg_StdPalNum = PAL_VIOLENT;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_VIOLENT);
}

void rbConfigPalHeavyMetal(void)
{
	config.cfg_StdPalNum = PAL_HEAVY_METAL;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_HEAVY_METAL);
}

void rbConfigPalWhyColors(void)
{
	config.cfg_StdPalNum = PAL_WHY_COLORS;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_WHY_COLORS);
}

void rbConfigPalJungle(void)
{
	config.cfg_StdPalNum = PAL_JUNGLE;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_JUNGLE);
}

void rbConfigPalUserDefined(void)
{
	config.cfg_StdPalNum = PAL_USER_DEFINED;
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	checkRadioButton(RB_CONFIG_PAL_USER_DEFINED);
}

bool patternFieldColorsActive(void)
{
	bool colored = tapeheadConfig.patternColorMode == PATTERN_COLOR_ALWAYS;
	if (tapeheadConfig.patternColorMode == PATTERN_COLOR_EDIT)
	{
		/* These are the canonical pattern-writing states. Normal SONG/PATT
		** transport is deliberately excluded; recording playback is included. */
		colored = playMode == PLAYMODE_EDIT || playMode == PLAYMODE_RECPATT ||
			playMode == PLAYMODE_RECSONG;
	}
	return colored;
}

uint32_t patternFieldColor(uint8_t field, bool populated)
{
	if (!patternFieldColorsActive())
		return video.palette[PAL_PATTEXT];
	if (!populated)
		return video.palette[PAL_PATTERN_EMPTY];
	return video.palette[PAL_PATTERN_NOTE + MIN(field, 4)];
}

bool paletteListMouseWheel(bool directionUp, int32_t x, int32_t y)
{
	if (!ui.configScreenShown || editor.currConfigScreen != CONFIG_SCREEN_LAYOUT ||
		x < PAL_LIST_X || x >= 501 || y < PAL_LIST_Y || y >= PAL_LIST_Y + (PAL_LIST_ROW_H * PAL_LIST_VISIBLE_ROWS))
		return false;

	if (directionUp && paletteListOffset > 0) paletteListOffset--;
	else if (!directionUp && paletteListOffset < TAPEHEAD_PALETTE_EDIT_COUNT -
		PAL_LIST_VISIBLE_ROWS) paletteListOffset++;
	setScrollBarPos(SB_PAL_LIST, paletteListOffset, DONT_TRIGGER_CALLBACK);
	showPaletteEditor();
	return true;
}

bool paletteListMouseDown(int32_t x, int32_t y)
{
	if (!ui.configScreenShown || editor.currConfigScreen != CONFIG_SCREEN_LAYOUT)
		return false;
	if (x >= PAL_LIST_X && x < 484 && y >= PAL_LIST_Y && y < PAL_LIST_Y + (PAL_LIST_ROW_H * PAL_LIST_VISIBLE_ROWS))
	{
		const uint8_t row = (uint8_t)((y - PAL_LIST_Y) / PAL_LIST_ROW_H);
		cfg_ColorNum = (uint8_t)MIN(paletteListOffset + row,
			TAPEHEAD_PALETTE_EDIT_COUNT - 1);
		updatePaletteEditor();
		showPaletteEditor();
		return true;
	}
	return false;
}

void sbPalListPos(uint32_t pos)
{
	paletteListOffset = (uint8_t)MIN(pos, TAPEHEAD_PALETTE_EDIT_COUNT -
		PAL_LIST_VISIBLE_ROWS);
	showPaletteEditor();
}

void cyclePalettePreset(void)
{
	config.cfg_StdPalNum = (int16_t)((config.cfg_StdPalNum + 1) % 12);
	updatePaletteEditor();
	setPalette(palTable[config.cfg_StdPalNum], REDRAW_SCREEN);
	showPaletteEditor();
}

void cyclePatternColorMode(void)
{
	tapeheadConfig.patternColorMode = (uint8_t)((tapeheadConfig.patternColorMode + 1) % 3);
	ui.updatePatternEditor = true;
	saveTapeheadPatternColorMode();
	showPaletteEditor();
}

void getUserPatternColors(uint32_t colors[TAPEHEAD_CUSTOM_COLOR_COUNT])
{
	initPatternColors();
	for (int32_t i = 0; i < TAPEHEAD_CUSTOM_COLOR_COUNT; i++)
		colors[i] = RGB32(COLOR_6BIT_TO_8BIT(patternColors[PAL_USER_DEFINED][i].r),
			COLOR_6BIT_TO_8BIT(patternColors[PAL_USER_DEFINED][i].g), COLOR_6BIT_TO_8BIT(patternColors[PAL_USER_DEFINED][i].b));
}

void setUserPatternColor(uint8_t field, uint32_t rgb)
{
	if (field >= TAPEHEAD_CUSTOM_COLOR_COUNT) return;
	initPatternColors();
	patternColors[PAL_USER_DEFINED][field].r = color8To6(RGB32_R(rgb));
	patternColors[PAL_USER_DEFINED][field].g = color8To6(RGB32_G(rgb));
	patternColors[PAL_USER_DEFINED][field].b = color8To6(RGB32_B(rgb));
}
