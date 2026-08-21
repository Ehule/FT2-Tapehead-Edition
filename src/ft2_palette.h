#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "ft2_universal_palette.h"

enum
{
	// for setPalette()
	DONT_REDRAW_SCREEN = false,
	REDRAW_SCREEN = true
};

#define RGB32_R(x) (((x) >> 16) & 0xFF)
#define RGB32_G(x) (((x) >> 8) & 0xFF)
#define RGB32_B(x) ((x) & 0xFF)
#define RGB32(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define COLOR_6BIT_TO_8BIT(x) ((((x) * 255) + (63/2)) / 63)

#define TAPEHEAD_PATTERN_FIELD_COLOR_COUNT 6
#define TAPEHEAD_TRANSPORT_COLOR_COUNT 7
#define TAPEHEAD_CUSTOM_COLOR_COUNT \
	(TAPEHEAD_PATTERN_FIELD_COLOR_COUNT + TAPEHEAD_TRANSPORT_COLOR_COUNT)
#define TAPEHEAD_PALETTE_EDIT_COUNT TAPEHEAD_UNIVERSAL_TAPEHEAD_COLOR_COUNT

// palette entry for transparency
#define PAL_TRANSPR 127

enum
{
	// FT2 palette (exact order as original FT2)
	PAL_BCKGRND   = 0,
	PAL_PATTEXT   = 1,
	PAL_BLCKMRK   = 2,
	PAL_BLCKTXT   = 3,
	PAL_DESKTOP   = 4,
	PAL_FORGRND   = 5,
	PAL_BUTTONS   = 6,
	PAL_BTNTEXT   = 7,
	PAL_DSKTOP2   = 8,
	PAL_DSKTOP1   = 9,
	PAL_BUTTON2   = 10,
	PAL_BUTTON1   = 11,
	PAL_MOUSEPT   = 12,
	PAL_PIANOXOR1 = 13, // for mouse pointer when hovering over instr. ed. piano
	PAL_PIANOXOR2 = 14, // ---
	PAL_PIANOXOR3 = 15, // ---

	// ---- custom FT2 clone palette entries ----
	PAL_LOOPPIN   = 16,
	PAL_TEXTMRK   = 17,
	PAL_BOXSLCT   = 18,
	PAL_CURSOR_NAV = 19,
	PAL_CURSOR_EDIT = 20,
	PAL_TRACKTRIM_GREEN = 21,
	PAL_PATTERN_NOTE = 22,
	PAL_PATTERN_INSTRUMENT = 23,
	PAL_PATTERN_VOLUME = 24,
	PAL_PATTERN_TUNING = 25,
	PAL_PATTERN_EFFECT = 26,
	PAL_PATTERN_EMPTY = 27,
	PAL_TRACK_LENGTH_PLAYHEAD = 28,
	PAL_FASTTRACKS_PLAYHEAD = 29,
	PAL_CONTROL_PLAYHEAD = 30,
	PAL_FASTTRACKS_SYNC = 31,
	PAL_FASTTRACKS_PHASE = 32,
	PAL_FASTTRACKS_SONG = 33,
	PAL_FASTTRACKS_LENGTH_PLAYHEAD = 34,

	PAL_NUM
};

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct pal16_t
{
	uint8_t r, g, b;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
pal16;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

void setPalette(pal16 *p, bool redrawScreen);

void sbPalRPos(uint32_t pos);
void sbPalGPos(uint32_t pos);
void sbPalBPos(uint32_t pos);
void sbPalContrastPos(uint32_t pos);
void configPalRDown(void);
void configPalRUp(void);
void configPalGDown(void);
void configPalGUp(void);
void configPalBDown(void);
void configPalBUp(void);
void configPalContDown(void);
void configPalContUp(void);
void configPalLoadShared(void);
void configPalSaveShared(void);
void loadTapeheadPaletteOnStartup(void);
void showPaletteEditor(void);

void rbConfigPalPatternText(void);
void rbConfigPalBlockMark(void);
void rbConfigPalTextOnBlock(void);
void rbConfigPalMouse(void);
void rbConfigPalDesktop(void);
void rbConfigPalButttons(void);
void rbConfigPalArctic(void);
void rbConfigPalLitheDark(void);
void rbConfigPalAuroraBorealis(void);
void rbConfigPalRose(void);
void rbConfigPalBlues(void);
void rbConfigPalDarkMode(void);
void rbConfigPalGold(void);
void rbConfigPalViolent(void);
void rbConfigPalHeavyMetal(void);
void rbConfigPalWhyColors(void);
void rbConfigPalJungle(void);
void rbConfigPalUserDefined(void);

extern uint8_t cfg_ColorNum;
uint32_t patternFieldColor(uint8_t field, bool populated);
bool patternFieldColorsActive(void);
bool paletteListMouseDown(int32_t x, int32_t y);
bool paletteListMouseWheel(bool directionUp, int32_t x, int32_t y);
void cyclePatternColorMode(void);
void cyclePalettePreset(void);
void sbPalListPos(uint32_t pos);
void getUserPatternColors(uint32_t colors[TAPEHEAD_CUSTOM_COLOR_COUNT]);
void setUserPatternColor(uint8_t field, uint32_t rgb);
