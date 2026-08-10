// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ft2_header.h"
#include "ft2_pattern_ed.h"
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_tables.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"
#include "ft2_replayer.h"
#include "ft2_interpolation.h"

#define CURSOR_BREATHE_FRAMES 120

static uint8_t cursorBreatheFrame;

static uint8_t getCursorBreatheLevel(void)
{
	/*
	** Two-second triangular breathing curve at 60Hz:
	** 255 -> 140 -> 255. It never becomes faint enough to disappear.
	*/
	const uint16_t halfCycle = CURSOR_BREATHE_FRAMES / 2;
	uint16_t distance = cursorBreatheFrame;
	if (distance > halfCycle)
		distance = CURSOR_BREATHE_FRAMES - distance;

	return (uint8_t)(255 - ((distance * 115) / halfCycle));
}

static uint32_t breatheColorToward(uint32_t color, uint32_t background)
{
	const uint16_t level = getCursorBreatheLevel();
	const uint16_t inverse = 255 - level;

	const uint8_t r = (uint8_t)(((RGB32_R(color) * level) + (RGB32_R(background) * inverse)) / 255);
	const uint8_t g = (uint8_t)(((RGB32_G(color) * level) + (RGB32_G(background) * inverse)) / 255);
	const uint8_t b = (uint8_t)(((RGB32_B(color) * level) + (RGB32_B(background) * inverse)) / 255);
	return RGB32(r, g, b);
}

void resetPatternCursorBlink(void)
{
	cursorBreatheFrame = 0;

	if (ui.patternEditorShown)
		ui.updatePatternEditor = true;
}

void updatePatternCursorBlink(void)
{
	if (!ui.patternEditorShown || ui.sysReqShown || ui.configScreenShown || ui.helpScreenShown)
	{
		cursorBreatheFrame = 0;
		return;
	}

	cursorBreatheFrame++;
	if (cursorBreatheFrame >= CURSOR_BREATHE_FRAMES)
		cursorBreatheFrame = 0;

	ui.updatePatternEditor = true;
}

static note_t emptyPattern[MAX_CHANNELS * MAX_PATT_LEN];

static const uint8_t *font4Ptr, *font5Ptr;
static const uint8_t vol2charTab1[16] = { 39, 0, 1, 2, 3, 4, 36, 52, 53, 54, 28, 31, 25, 58, 59, 22 };
static const uint8_t vol2charTab2[16] = { 42, 0, 1, 2, 3, 4, 36, 37, 38, 39, 28, 31, 25, 40, 41, 22 };
static const uint8_t columnModeTab[12] = { 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 3 };

typedef struct pattLayout_t
{
	uint8_t x[11], width[11], fontType, charW, noteSize;
} pattLayout_t;

/* One geometry entry per original adaptive channel-width mode. The tuning lane
** is deliberately part of each entry instead of being overlaid at fixed x positions. */
static const pattLayout_t pattLayouts[2][4] =
{
	{ // volume hidden: 4, 6, 8 and 12 visible channels
		{{ 3, 59, 67,  0,  0, 83, 91, 99,115,123,131}, {48,8,8,0,0,8,8,8,8,8,8}, FONT_TYPE4, 8, 2},
		{{ 3, 31, 35,  0,  0, 43, 47, 51, 59, 63, 67}, {24,4,4,0,0,4,4,4,4,4,4}, FONT_TYPE3, 4, 1},
		{{ 3, 27, 31,  0,  0, 39, 43, 47, 55, 59, 63}, {24,4,4,0,0,4,4,4,4,4,4}, FONT_TYPE3, 4, 1},
		{{ 2, 20, 23,  0,  0, 27, 30, 33, 37, 40, 43}, {18,3,3,0,0,3,3,3,3,3,3}, FONT_TYPE3, 3, 0}
	},
	{ // volume shown: 4, 6 and 8 visible channels (mode 3 aliases mode 2)
		{{ 3, 55, 63, 75, 83, 95,103,111,119,127,135}, {48,8,8,8,8,8,8,8,8,8,8}, FONT_TYPE4, 8, 2},
		{{ 3, 27, 31, 39, 43, 51, 55, 59, 67, 71, 75}, {24,4,4,4,4,4,4,4,4,4,4}, FONT_TYPE3, 4, 1},
		{{ 3, 27, 31, 35, 39, 43, 47, 51, 55, 59, 63}, {24,4,4,4,4,4,4,4,4,4,4}, FONT_TYPE3, 4, 1},
		{{ 3, 27, 31, 35, 39, 43, 47, 51, 55, 59, 63}, {24,4,4,4,4,4,4,4,4,4,4}, FONT_TYPE3, 4, 1}
	}
};
static const uint8_t sharpNote1Char_small[12] = {  8*6,  8*6,  9*6,  9*6, 10*6, 11*6, 11*6, 12*6, 12*6, 13*6, 13*6, 14*6 };
static const uint8_t sharpNote2Char_small[12] = { 16*6, 15*6, 16*6, 15*6, 16*6, 16*6, 15*6, 16*6, 15*6, 16*6, 15*6, 16*6 };
static const uint8_t flatNote1Char_small[12] = {  8*6,  9*6,  9*6, 10*6, 10*6, 11*6, 12*6, 12*6, 13*6, 13*6, 14*6, 14*6 };
static const uint8_t flatNote2Char_small[12] = { 16*6, 17*6, 16*6, 17*6, 16*6, 16*6, 17*6, 16*6, 17*6, 16*6, 17*6, 16*6 };
static const uint8_t sharpNote1Char_med[12] = { 12*8, 12*8, 13*8, 13*8, 14*8, 15*8, 15*8, 16*8, 16*8, 10*8, 10*8, 11*8 };
static const uint16_t sharpNote2Char_med[12] = { 36*8, 37*8, 36*8, 37*8, 36*8, 36*8, 37*8, 36*8, 37*8, 36*8, 37*8, 36*8 };
static const uint8_t flatNote1Char_med[12] = { 12*8, 13*8, 13*8, 14*8, 14*8, 15*8, 16*8, 16*8, 10*8, 10*8, 11*8, 11*8 };
static const uint16_t flatNote2Char_med[12] = { 36*8, 38*8, 36*8, 38*8, 36*8, 36*8, 38*8, 36*8, 38*8, 36*8, 38*8, 36*8 };
static const uint16_t sharpNote1Char_big[12] = { 12*16, 12*16, 13*16, 13*16, 14*16, 15*16, 15*16, 16*16, 16*16, 10*16, 10*16, 11*16 };
static const uint16_t sharpNote2Char_big[12] = { 36*16, 37*16, 36*16, 37*16, 36*16, 36*16, 37*16, 36*16, 37*16, 36*16, 37*16, 36*16 };
static const uint16_t flatNote1Char_big[12] = { 12*16, 13*16, 13*16, 14*16, 14*16, 15*16, 16*16, 16*16, 10*16, 10*16, 11*16, 11*16 };
static const uint16_t flatNote2Char_big[12] = { 36*16, 38*16, 36*16, 38*16, 36*16, 36*16, 38*16, 36*16, 38*16, 36*16, 38*16, 36*16 };

static void pattCharOut(uint32_t xPos, uint32_t yPos, uint8_t chr, uint8_t fontType, uint32_t color);
void pattTwoHexOut(uint32_t xPos, uint32_t yPos, uint8_t val, uint32_t color);
static void drawEmptyNoteSmall(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawKeyOffSmall(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawNoteSmall(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color);
static void drawEmptyNoteMedium(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawKeyOffMedium(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawNoteMedium(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color);
static void drawEmptyNoteBig(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawKeyOffBig(uint32_t xPos, uint32_t yPos, uint32_t color);
static void drawNoteBig(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color);

void updatePattFontPtrs(void)
{
	//config.ptnFont is pre-clamped and safe to use
	font4Ptr = &bmp.font4[config.ptnFont * (FONT4_WIDTH * FONT4_CHAR_H)];
	font5Ptr = &bmp.font4[(4 + config.ptnFont) * (FONT4_WIDTH * FONT4_CHAR_H)];
}

void drawPatternBorders(void)
{
	// get heights/pos/rows depending on configuration
	const pattCoord2_t *pattCoord = &pattCoord2Table[config.ptnStretch][ui.pattChanScrollShown][getPatternEditorView()];

	// set pattern cursor Y position
	editor.ptnCursorY = pattCoord->lowerRowsY - 9;

	int32_t chans = ui.numChannelsShown;
	if (chans > ui.maxVisibleChannels)
		chans = ui.maxVisibleChannels;

	// in some configurations, there will be two empty channels to the right, fix that
	if (chans == 2)
		chans = 4;
	else if (chans == 10 && !config.ptnShowVolColumn)
		chans = 12;

	ASSERT(chans >= 2 && chans <= 12);

	const uint16_t chanWidth = chanWidths[(chans >> 1) - 1] + 2;

	// fill scrollbar framework (if needed)
	if (ui.pattChanScrollShown)
		drawFramework(0, 383, 632, 17, FRAMEWORK_TYPE1);

	if (config.ptnFrmWrk)
	{
		// pattern editor w/ framework

		if (ui.patternEditorOnly)
		{
			vLine(0,   1, 398, PAL_DSKTOP1);
			vLine(631, 0, 399, PAL_DSKTOP2);

			vLine(1,   1, 398, PAL_DESKTOP);
			vLine(630, 1, 398, PAL_DESKTOP);

			hLine(0, 0, 631, PAL_DSKTOP1);
			hLine(1, 1, 630, PAL_DESKTOP);

			if (!ui.pattChanScrollShown)
			{
				hLine(1, 398, 630, PAL_DESKTOP);
				hLine(0, 399, 632, PAL_DSKTOP2);
			}
		}
		else if (ui.extendedPatternEditor)
		{
			vLine(0,   69, 330, PAL_DSKTOP1);
			vLine(631, 68, 331, PAL_DSKTOP2);

			vLine(1,   69, 330, PAL_DESKTOP);
			vLine(630, 69, 330, PAL_DESKTOP);

			hLine(0, 68, 631, PAL_DSKTOP1);
			hLine(1, 69, 630, PAL_DESKTOP);

			if (!ui.pattChanScrollShown)
			{
				hLine(1, 398, 630, PAL_DESKTOP);
				hLine(0, 399, 632, PAL_DSKTOP2);
			}
		}
		else
		{
			vLine(0,   174, 225, PAL_DSKTOP1);
			vLine(631, 173, 226, PAL_DSKTOP2);

			vLine(1,   174, 225, PAL_DESKTOP);
			vLine(630, 174, 225, PAL_DESKTOP);

			hLine(0, 173, 631, PAL_DSKTOP1);
			hLine(1, 174, 630, PAL_DESKTOP);

			if (!ui.pattChanScrollShown)
			{
				hLine(1, 398, 630, PAL_DESKTOP);
				hLine(0, 399, 632, PAL_DSKTOP2);
			}
		}

		// fill middle (current row)
		fillRect(2, pattCoord->lowerRowsY - 9, 628, 9, PAL_DESKTOP);

		// fill row number boxes
		drawFramework(2, pattCoord->upperRowsY, 25, pattCoord->upperRowsH, FRAMEWORK_TYPE2); // top left
		drawFramework(604, pattCoord->upperRowsY, 26, pattCoord->upperRowsH, FRAMEWORK_TYPE2); // top right
		drawFramework(2, pattCoord->lowerRowsY, 25, pattCoord->lowerRowsH, FRAMEWORK_TYPE2); // bottom left
		drawFramework(604, pattCoord->lowerRowsY, 26, pattCoord->lowerRowsH, FRAMEWORK_TYPE2); // bottom right

		// draw channels
		uint16_t xOffs = 28;
		for (int32_t i = 0; i < chans; i++)
		{
			vLine(xOffs - 1, pattCoord->upperRowsY, pattCoord->upperRowsH, PAL_DESKTOP);
			vLine(xOffs - 1, pattCoord->lowerRowsY, pattCoord->lowerRowsH + 1, PAL_DESKTOP);

			drawFramework(xOffs, pattCoord->upperRowsY, chanWidth, pattCoord->upperRowsH, FRAMEWORK_TYPE2); // top part
			drawFramework(xOffs, pattCoord->lowerRowsY, chanWidth, pattCoord->lowerRowsH, FRAMEWORK_TYPE2); // bottom part

			xOffs += chanWidth+1;
		}

		vLine(xOffs-1, pattCoord->upperRowsY, pattCoord->upperRowsH, PAL_DESKTOP);
		vLine(xOffs-1, pattCoord->lowerRowsY, pattCoord->lowerRowsH+1, PAL_DESKTOP);
	}
	else
	{
		// pattern editor without framework

		if (ui.patternEditorOnly)
		{
			const int32_t clearRows = ui.pattChanScrollShown ? 383 : SCREEN_H;
			memset(video.frameBuffer, 0, SCREEN_W * sizeof (int32_t) * clearRows);
		}
		else if (ui.extendedPatternEditor)
		{
			const int32_t clearSize = ui.pattChanScrollShown ? (SCREEN_W * sizeof (int32_t) * 315) : (SCREEN_W * sizeof (int32_t) * 332);
			memset(&video.frameBuffer[68 * SCREEN_W], 0, clearSize);
		}
		else
		{
			const int32_t clearSize = ui.pattChanScrollShown ? (SCREEN_W * sizeof(int32_t) * 210) : (SCREEN_W * sizeof(int32_t) * 227);
			memset(&video.frameBuffer[173 * SCREEN_W], 0, clearSize);
		}

		drawFramework(0, pattCoord->lowerRowsY - 10, SCREEN_W, 11, FRAMEWORK_TYPE1);
	}

	if (ui.pattChanScrollShown)
	{
		showScrollBar(SB_CHAN_SCROLL);
		showPushButton(PB_CHAN_SCROLL_LEFT);
		showPushButton(PB_CHAN_SCROLL_RIGHT);
	}
	else
	{
		hideScrollBar(SB_CHAN_SCROLL);
		hidePushButton(PB_CHAN_SCROLL_LEFT);
		hidePushButton(PB_CHAN_SCROLL_RIGHT);

		setScrollBarPos(SB_CHAN_SCROLL, 0, DONT_TRIGGER_CALLBACK);
	}
}

static const pattLayout_t *getPatternLayout(void)
{
	const uint8_t mode = columnModeTab[ui.numChannelsShown-1];
	return &pattLayouts[config.ptnShowVolColumn != 0][mode];
}

uint8_t patternXToCursorObject(int32_t x)
{
	const pattLayout_t *layout = getPatternLayout();
	uint8_t bestObject = CURSOR_NOTE;
	int32_t bestDistance = 0x7FFFFFFF;

	for (uint8_t object = CURSOR_NOTE; object <= CURSOR_EFX2; object++)
	{
		if (!config.ptnShowVolColumn && (object == CURSOR_VOL1 || object == CURSOR_VOL2))
			continue;

		const int32_t center = layout->x[object] + (layout->width[object] / 2);
		const int32_t distance = ABS(x - center);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			bestObject = object;
		}
	}

	return bestObject;
}

static void writeCursor(void)
{
	const pattLayout_t *layout = getPatternLayout();
	int32_t xPos = 29 + layout->x[cursor.object];
	const int32_t width = layout->width[cursor.object];

	ASSERT(editor.ptnCursorY > 0 && xPos > 0 && width > 0);
	xPos += ((cursor.ch - ui.channelOffset) * ui.patternChannelWidth);

	/*
	** Keep the pattern data untouched and draw a one-pixel locator around the
	** active field. The color also communicates whether keyboard input will
	** edit pattern data, without requiring the performer to look at the status
	** readout above the editor. Recording modes use the edit color as well.
	*/
	const bool editCursor = playMode == PLAYMODE_EDIT ||
	                        playMode == PLAYMODE_RECPATT ||
	                        playMode == PLAYMODE_RECSONG;
	uint32_t cursorColor = video.palette[editCursor ? PAL_CURSOR_EDIT : PAL_CURSOR_NAV];
	cursorColor = breatheColorToward(cursorColor, video.palette[PAL_DESKTOP]);

	uint32_t *topPtr = &video.frameBuffer[(editor.ptnCursorY * SCREEN_W) + xPos];
	uint32_t *bottomPtr = topPtr + (8 * SCREEN_W);
	for (int32_t x = 0; x < width; x++)
	{
		topPtr[x] = cursorColor;
		bottomPtr[x] = cursorColor;
	}

	for (int32_t y = 1; y < 8; y++)
	{
		uint32_t *rowPtr = topPtr + (y * SCREEN_W);
		rowPtr[0] = cursorColor;
		if (width > 1)
			rowPtr[width-1] = cursorColor;
	}
}

static void pattLayoutCharOut(uint32_t x, uint32_t y, uint8_t chr,
	const pattLayout_t *layout, uint32_t color)
{
	if (layout->charW != 3)
	{
		pattCharOut(x, y, chr, layout->fontType, color);
		return;
	}

	/* The twelve-channel mode has 48 pixels per cell. Clip the small glyph to
	** three columns so adjacent instrument/tuning/effect characters never overlap. */
	const uint8_t *src = &bmp.font3[chr * FONT3_CHAR_W];
	uint32_t *dst = &video.frameBuffer[(y * SCREEN_W) + x];
	for (int32_t row = 0; row < FONT3_CHAR_H; row++)
	{
		/* Preserve both outside strokes and merge the two center columns. This
		** keeps M/N and hexadecimal glyphs recognizable instead of truncating
		** their right edge. */
		if (src[0] != 0) dst[0] = color;
		if (src[1] != 0 || src[2] != 0) dst[1] = color;
		if (src[3] != 0) dst[2] = color;

		src += FONT3_WIDTH;
		dst += SCREEN_W;
	}
}

static void drawAdaptiveCell(uint32_t x, uint32_t y, const note_t *n,
	uint32_t noteColor, uint32_t instrColor, uint32_t volColor,
	uint32_t tuneColor, uint32_t effectColor)
{
	const pattLayout_t *layout = getPatternLayout();
	const uint8_t *px = layout->x;

	if (layout->noteSize == 2)
	{
		if (n->note == NOTE_OFF) drawKeyOffBig(x + px[0], y, noteColor);
		else if (n->note > 0 && n->note <= 96) drawNoteBig(x + px[0], y, n->note, noteColor);
		else drawEmptyNoteBig(x + px[0], y, noteColor);
	}
	else if (layout->noteSize == 1)
	{
		if (n->note == NOTE_OFF) drawKeyOffMedium(x + px[0], y, noteColor);
		else if (n->note > 0 && n->note <= 96) drawNoteMedium(x + px[0], y, n->note, noteColor);
		else drawEmptyNoteMedium(x + px[0], y, noteColor);
	}
	else
	{
		if (n->note == NOTE_OFF) drawKeyOffSmall(x + px[0], y, noteColor);
		else if (n->note > 0 && n->note <= 96) drawNoteSmall(x + px[0], y, n->note, noteColor);
		else drawEmptyNoteSmall(x + px[0], y, noteColor);
	}

	/* Empty instrument numbers intentionally retain FT2's 00 convention. */
	pattLayoutCharOut(x + px[1], y, n->instr >> 4, layout, instrColor);
	pattLayoutCharOut(x + px[2], y, n->instr & 15, layout, instrColor);

	if (config.ptnShowVolColumn)
	{
		const uint8_t dot = layout->fontType == FONT_TYPE3 ? 42 : 39;
		const uint8_t vol1 = n->vol < 0x10 ? dot :
			(layout->fontType == FONT_TYPE3 ? vol2charTab2[n->vol >> 4] : vol2charTab1[n->vol >> 4]);
		const uint8_t vol2 = n->vol < 0x10 ? dot : n->vol & 15;
		pattLayoutCharOut(x + px[3], y, vol1, layout, volColor);
		pattLayoutCharOut(x + px[4], y, vol2, layout, volColor);
	}

	const uint8_t dot = layout->fontType == FONT_TYPE3 ? 42 : 39;
	if (n->tuneType == 0)
	{
		for (int32_t i = 5; i <= 7; i++)
			pattLayoutCharOut(x + px[i], y, dot, layout, tuneColor);
	}
	else
	{
		pattLayoutCharOut(x + px[5], y, n->tuneType, layout, tuneColor);
		pattLayoutCharOut(x + px[6], y, n->tuneData >> 4, layout, tuneColor);
		pattLayoutCharOut(x + px[7], y, n->tuneData & 15, layout, tuneColor);
	}

	if (n->efx == 0 && n->efxData == 0)
	{
		for (int32_t i = 8; i <= 10; i++)
			pattLayoutCharOut(x + px[i], y, dot, layout, effectColor);
	}
	else
	{
		pattLayoutCharOut(x + px[8], y, n->efx, layout, effectColor);
		pattLayoutCharOut(x + px[9], y, n->efxData >> 4, layout, effectColor);
		pattLayoutCharOut(x + px[10], y, n->efxData & 15, layout, effectColor);
	}
}

static void writePatternBlockMark(int32_t currRow, uint32_t rowHeight, const pattCoord_t *pattCoord)
{
	int32_t y1, y2;

	// this can happen (buggy FT2 code), treat like no mark
	if (pattMark.markY1 > pattMark.markY2)
		return;

	const int32_t startCh = ui.channelOffset;
	const int32_t endCh = ui.channelOffset + (ui.numChannelsShown - 1);
	const int32_t startRow = currRow - pattCoord->numUpperRows;
	const int32_t endRow = currRow + pattCoord->numLowerRows;

	// test if pattern marking is outside of visible area (don't draw)
	if (pattMark.markX1 > endCh || pattMark.markX2 < startCh || pattMark.markY1 > endRow || pattMark.markY2 < startRow)
		return;

	const markCoord_t *markCoord = &markCoordTable[config.ptnStretch][ui.pattChanScrollShown][getPatternEditorView()];
	const int32_t pattYStart = markCoord->upperRowsY;

	// X1

	int32_t x1 = 32 + ((pattMark.markX1 - ui.channelOffset) * ui.patternChannelWidth);
	if (x1 < 32)
		x1 = 32;

	// X2

	int32_t x2 = (32 - 8) + (((pattMark.markX2 + 1) - ui.channelOffset) * ui.patternChannelWidth);
	if (x2 > 608)
		x2 = 608;

	// Y1

	if (pattMark.markY1 < currRow)
	{
		y1 = pattYStart + ((pattMark.markY1 - startRow) * rowHeight);
		if (y1 < pattYStart)
			y1 = pattYStart;
	}
	else if (pattMark.markY1 == currRow)
	{
		y1 = markCoord->midRowY;
	}
	else
	{
		y1 = markCoord->lowerRowsY + ((pattMark.markY1 - (currRow + 1)) * rowHeight);
	}

	// Y2

	if (pattMark.markY2-1 < currRow)
	{
		y2 = pattYStart + ((pattMark.markY2 - startRow) * rowHeight);
	}
	else if (pattMark.markY2-1 == currRow)
	{
		y2 = markCoord->midRowY + 11;
	}
	else
	{
		const int32_t pattYEnd = markCoord->lowerRowsY + (pattCoord->numLowerRows * rowHeight);

		y2 = markCoord->lowerRowsY + ((pattMark.markY2 - (currRow + 1)) * rowHeight);
		if (y2 > pattYEnd)
			y2 = pattYEnd;
	}

	// kludge! (some mark situations could overwrite illegal areas)
	if (config.ptnStretch && ui.pattChanScrollShown)
	{
		if (y1 == pattCoord->upperRowsY-1 || y1 == pattCoord->lowerRowsY-1)
			y1++;

		if (y2 == 384)
			y2--;

		// this can actually happen here, don't render in that case
		if (y1 >= y2)
			return;
	}

	ASSERT(x1 > 0 && x1 < SCREEN_W && x2 > 0 && x2 < SCREEN_W &&
	       y1 > 0 && y1 < SCREEN_H && y2 > 0 && y2 < SCREEN_H);

	// pattern mark drawing

	const int32_t w = x2 - x1;
	const int32_t h = y2 - y1;

	ASSERT(x1+w <= SCREEN_W && y1+h <= SCREEN_H);

	uint32_t *ptr32 = &video.frameBuffer[(y1 * SCREEN_W) + x1];
	for (int32_t y = 0; y < h; y++)
	{
		for (int32_t x = 0; x < w; x++)
			ptr32[x] = video.palette[(ptr32[x] >> 24) ^ (interpolationPreviewActive() ? 6 : 2)]; // preview uses a distinct temporary-selection palette

		ptr32 += SCREEN_W;
	}
}

static void drawChannelNumbering(uint16_t yPos)
{
	uint16_t xPos = 30;
	uint8_t ch = ui.channelOffset + 1;

	for (uint8_t i = 0; i < ui.numChannelsShown; i++)
	{
		if (ch < 10)
		{
			charOutOutlined(xPos, yPos, PAL_MOUSEPT, '0' + (char)ch);
		}
		else
		{
			charOutOutlined(xPos, yPos, PAL_MOUSEPT, '0' + (ch / 10));
			charOutOutlined(xPos + (FONT1_CHAR_W + 1), yPos, PAL_MOUSEPT, '0' + (ch % 10));
		}

		ch++;
		xPos += ui.patternChannelWidth;
	}
}

static void drawRowNums(int32_t yPos, uint8_t row, bool selectedRowFlag)
{
#define LEFT_ROW_XPOS 8
#define RIGHT_ROW_XPOS 608

	uint32_t pixVal;

	// set color based on some conditions

	if (selectedRowFlag)
		pixVal = video.palette[PAL_FORGRND];
	else if (config.ptnLineLight && !(row & 3))
		pixVal = video.palette[PAL_BLCKTXT];
	else
		pixVal = video.palette[PAL_PATTEXT];

	if (!config.ptnHex)
		row = hex2Dec[row];

	const uint8_t *src1Ptr = &font4Ptr[(row   >> 4) * FONT4_CHAR_W];
	const uint8_t *src2Ptr = &font4Ptr[(row & 0x0F) * FONT4_CHAR_W];
	uint32_t *dst1Ptr = &video.frameBuffer[(yPos * SCREEN_W) + LEFT_ROW_XPOS];
	uint32_t *dst2Ptr = dst1Ptr + (RIGHT_ROW_XPOS - LEFT_ROW_XPOS);

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (src1Ptr[x])
			{
				dst1Ptr[x] = pixVal; // left side
				dst2Ptr[x] = pixVal; // right side
			}

			if (src2Ptr[x])
			{
				dst1Ptr[FONT4_CHAR_W+x] = pixVal; // left side
				dst2Ptr[FONT4_CHAR_W+x] = pixVal; // right side
			}
		}

		src1Ptr += FONT4_WIDTH;
		src2Ptr += FONT4_WIDTH;
		dst1Ptr += SCREEN_W;
		dst2Ptr += SCREEN_W;
	}
}

// DRAWING ROUTINES (WITH VOLUME COLUMN)

static void showNoteNum(uint32_t xPos, uint32_t yPos, int16_t note, uint32_t color)
{
	xPos += 3;

	ASSERT(note >= 0 && note <= 97);

	if (ui.numChannelsShown <= 4)
	{
		if (note <= 0 || note > 97)
			drawEmptyNoteBig(xPos, yPos, color);
		else if (note == NOTE_OFF)
			drawKeyOffBig(xPos, yPos, color);
		else
			drawNoteBig(xPos, yPos, note, color);
	}
	else
	{
		if (note <= 0 || note > 97)
			drawEmptyNoteMedium(xPos, yPos, color);
		else if (note == NOTE_OFF)
			drawKeyOffMedium(xPos, yPos, color);
		else
			drawNoteMedium(xPos, yPos, note, color);
	}
}

static void showInstrNum(uint32_t xPos, uint32_t yPos, uint8_t ins, uint32_t color)
{
	uint8_t charW, fontType;
	uint32_t xStart, xDelta;

	if (ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xStart = 4;
		xDelta = 8;

		xPos += 67;
	}
	else if (ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xStart = 4;
		xDelta = 8;

		xPos += 27;
	}
	else
	{
		fontType = FONT_TYPE3;
		charW = FONT3_CHAR_W;
		xStart = 1;
		xDelta = 4;

		xPos += 31;
	}

	if (config.ptnAlternativeLayout && ins == 0)
	{
		yPos += 3;
		xPos += xStart;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
		xPos += xDelta;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
	}
	else
	{
		if (config.ptnAlternativeLayout || config.ptnInstrZero)
		{
			pattCharOut(xPos,         yPos, ins >> 4,   fontType, color);
			pattCharOut(xPos + charW, yPos, ins & 0x0F, fontType, color);
		}
		else
		{
			const uint8_t chr1 = ins >> 4;
			const uint8_t chr2 = ins & 0x0F;

			if (chr1 > 0)
				pattCharOut(xPos, yPos, chr1, fontType, color);

			if (chr1 > 0 || chr2 > 0)
				pattCharOut(xPos + charW, yPos, chr2, fontType, color);
		}
	}
}

static void showVolEfx(uint32_t xPos, uint32_t yPos, uint8_t vol, uint32_t color)
{
	uint8_t char1, char2, fontType, charW;
	uint32_t xStart, xDelta;

	if (ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xStart = 4;
		xDelta = 8;

		xPos += 91;

		char1 = vol2charTab1[vol >> 4];
		if (vol < 0x10)
			char2 = 39;
		else
			char2 = vol & 0x0F;
	}
	else if (ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xStart = 4;
		xDelta = 8;

		xPos += 51;

		char1 = vol2charTab1[vol >> 4];
		if (vol < 0x10)
			char2 = 39;
		else
			char2 = vol & 0x0F;
	}
	else
	{
		fontType = FONT_TYPE3;
		charW = FONT3_CHAR_W;
		xStart = 1;
		xDelta = 4;

		xPos += 43;

		char1 = vol2charTab2[vol >> 4];
		if (vol < 0x10)
			char2 = 42;
		else
			char2 = vol & 0x0F;
	}

	if (config.ptnAlternativeLayout && vol == 0)
	{
		yPos += 3;
		xPos += xStart;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
		xPos += xDelta;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
	}
	else
	{
		pattCharOut(xPos,         yPos, char1, fontType, color);
		pattCharOut(xPos + charW, yPos, char2, fontType, color);
	}
}

static void showEfx(uint32_t xPos, uint32_t yPos, uint8_t efx, uint8_t efxData, uint32_t color)
{
	uint8_t fontType, charW;
	uint32_t xStart, xDelta;

	if (ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xStart = 4;
		xDelta = 8;

		xPos += 115;
	}
	else if (ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xStart = 4;
		xDelta = 8;

		xPos += 67;
	}
	else
	{
		fontType = FONT_TYPE3;
		charW = FONT3_CHAR_W;
		xStart = 1;
		xDelta = 4;

		xPos += 55;
	}

	if (config.ptnAlternativeLayout && efx == 0 && efxData == 0)
	{
		yPos += 3;
		xPos += xStart;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
		xPos += xDelta;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
		xPos += xDelta;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
	}
	else
	{
		pattCharOut(xPos,               yPos, efx,            fontType, color);
		pattCharOut(xPos +  charW,      yPos, efxData >> 4,   fontType, color);
		pattCharOut(xPos + (charW * 2), yPos, efxData & 0x0F, fontType, color);
	}
}

// DRAWING ROUTINES (WITHOUT VOLUME COLUMN)

static void showNoteNumNoVolColumn(uint32_t xPos, uint32_t yPos, int16_t note, uint32_t color)
{
	xPos += 3;

	ASSERT(note >= 0 && note <= 97);

	if (ui.numChannelsShown <= 6)
	{
		if (note <= 0 || note > 97)
			drawEmptyNoteBig(xPos, yPos, color);
		else if (note == NOTE_OFF)
			drawKeyOffBig(xPos, yPos, color);
		else
			drawNoteBig(xPos, yPos, note, color);
	}
	else if (ui.numChannelsShown <= 8)
	{
		if (note <= 0 || note > 97)
			drawEmptyNoteMedium(xPos, yPos, color);
		else if (note == NOTE_OFF)
			drawKeyOffMedium(xPos, yPos, color);
		else
			drawNoteMedium(xPos, yPos, note, color);
	}
	else
	{
		if (note <= 0 || note > 97)
			drawEmptyNoteSmall(xPos, yPos, color);
		else if (note == NOTE_OFF)
			drawKeyOffSmall(xPos, yPos, color);
		else
			drawNoteSmall(xPos, yPos, note, color);
	}
}

static void showInstrNumNoVolColumn(uint32_t xPos, uint32_t yPos, uint8_t ins, uint32_t color)
{
	uint8_t charW, fontType;
	uint32_t xStart, xDelta;

	if (ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE5;
		charW = FONT5_CHAR_W;
		xStart = 7;
		xDelta = 16;

		xPos += 59;
	}
	else if (ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xStart = 4;
		xDelta = 8;

		xPos += 51;
	}
	else if (ui.numChannelsShown <= 8)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xStart = 4;
		xDelta = 8;

		xPos += 27;
	}
	else
	{
		fontType = FONT_TYPE3;
		charW = FONT3_CHAR_W;
		xStart = 1;
		xDelta = 4;

		xPos += 23;
	}

	if (config.ptnAlternativeLayout && ins == 0)
	{
		yPos += 3;
		xPos += xStart;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
		xPos += xDelta;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
	}
	else
	{
		if (config.ptnAlternativeLayout || config.ptnInstrZero)
		{
			pattCharOut(xPos,         yPos, ins >> 4,   fontType, color);
			pattCharOut(xPos + charW, yPos, ins & 0x0F, fontType, color);
		}
		else
		{
			const uint8_t chr1 = ins >> 4;
			const uint8_t chr2 = ins & 0x0F;

			if (chr1 > 0)
				pattCharOut(xPos, yPos, chr1, fontType, color);

			if (chr1 > 0 || chr2 > 0)
				pattCharOut(xPos + charW, yPos, chr2, fontType, color);
		}
	}
}

static void showNoVolEfx(uint32_t xPos, uint32_t yPos, uint8_t vol, uint32_t color)
{
	(void)xPos;
	(void)yPos;
	(void)vol;
	(void)color;
}

static void showEfxNoVolColumn(uint32_t xPos, uint32_t yPos, uint8_t efx, uint8_t efxData, uint32_t color)
{
	uint8_t charW, fontType;
	uint32_t xStart, xDelta;

	if (ui.numChannelsShown <= 4)
	{
		fontType = FONT_TYPE5;
		charW = FONT5_CHAR_W;
		xStart = 7;
		xDelta = 16;
		xPos += 91;
	}
	else if (ui.numChannelsShown <= 6)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xStart = 4;
		xDelta = 8;

		xPos += 67;
	}
	else if (ui.numChannelsShown <= 8)
	{
		fontType = FONT_TYPE4;
		charW = FONT4_CHAR_W;
		xStart = 4;
		xDelta = 8;

		xPos += 43;
	}
	else
	{
		fontType = FONT_TYPE3;
		charW = FONT3_CHAR_W;
		xStart = 1;
		xDelta = 4;

		xPos += 31;
	}

	if (config.ptnAlternativeLayout && efx == 0 && efxData == 0)
	{
		yPos += 3;
		xPos += xStart;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
		xPos += xDelta;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
		xPos += xDelta;
		video.frameBuffer[(yPos * SCREEN_W) + xPos] = color;
	}
	else
	{
		pattCharOut(xPos,               yPos, efx,            fontType, color);
		pattCharOut(xPos +  charW,      yPos, efxData >> 4,   fontType, color);
		pattCharOut(xPos + (charW * 2), yPos, efxData & 0x0F, fontType, color);
	}
}

static void drawFastTracksPOCLed(uint16_t x, uint16_t y, uint32_t color)
{
	/* Compact 3x3 rounded LED: visible at every supported channel width. */
	video.frameBuffer[( y      * SCREEN_W) + x + 1] = color;
	video.frameBuffer[((y + 1) * SCREEN_W) + x    ] = color;
	video.frameBuffer[((y + 1) * SCREEN_W) + x + 1] = color;
	video.frameBuffer[((y + 1) * SCREEN_W) + x + 2] = color;
	video.frameBuffer[((y + 2) * SCREEN_W) + x + 1] = color;
}

static void drawFastTracksPOCStatus(uint16_t yPos, const fastTracksSnapshot_t *snapshot)
{
	for (int32_t fastTrackChannel = 0; fastTrackChannel < MAX_CHANNELS; fastTrackChannel++)
	{
		const fastTracksTrackSnapshot_t *track = &snapshot->tracks[fastTrackChannel];
		if (!track->enabled)
			continue;

		if (fastTrackChannel < ui.channelOffset || fastTrackChannel >= ui.channelOffset + ui.numChannelsShown)
			continue;

		const int32_t visibleChannel = fastTrackChannel - ui.channelOffset;
		const uint32_t xPos = 30 + (visibleChannel * ui.patternChannelWidth);
		const uint8_t numerator = track->ratioNumerator;
		const uint8_t denominator = track->ratioDenominator;
		const int32_t sourceRow = track->sourceRow;
		const fastTracksMode_t transportMode = track->mode;
		const bool songMode = transportMode == FAST_TRACKS_MODE_SONG;
		const int32_t sourceOrder = track->sourceOrder;
		const bool clutchHeld = track->clutched;
		const bool reversed = track->reversed;

		/*
		** Fixed header zones keep one- and two-digit channel numbers from
		** colliding with Fast Tracks status information:
		**
		**   [channel number]   [right-aligned ratio]   [lag sync lead]
		**
		** Channel numbering is redrawn after this panel, so its left-hand zone
		** remains authoritative at every horizontal scroll position.
		*/
		const uint16_t panelWidth = (uint16_t)(ui.patternChannelWidth - 2);
		const uint8_t panelColor = clutchHeld ? PAL_BLCKMRK : PAL_DESKTOP;
		fillRect((uint16_t)xPos, yPos, panelWidth, 8, panelColor);

		char numeratorText[4], denominatorText[4];
		snprintf(numeratorText, sizeof (numeratorText), "%u", numerator);
		snprintf(denominatorText, sizeof (denominatorText), "%u", denominator);

		const int32_t numeratorDigits = numerator >= 10 ? 2 : 1;
		const int32_t denominatorDigits = denominator >= 10 ? 2 : 1;
		const int32_t ratioWidth = (numeratorDigits + 1 + denominatorDigits) * FONT3_CHAR_W;

		/* Anchor the diagnostic cluster to the channel's right edge. This keeps
		** reverse, phase LEDs and the Song order badge clear of both one- and
		** two-digit channel numbers. The ratio sits immediately to their left. */
		const int32_t directionMarkerWidth = reversed ? 8 : 0;
		const int32_t ledBankWidth = 11; /* three 3px LEDs with 1px gaps */
		const int32_t ratioStatusGap = 2;
		const int32_t directionLedGap = reversed ? 1 : 0;
		const int32_t songBadgeGap = songMode ? 2 : 0;
		const int32_t songBadgeWidth = songMode ? 10 : 0; /* highlighted two-digit order number */
		const int32_t statusWidth = directionMarkerWidth + directionLedGap + ledBankWidth + songBadgeGap + songBadgeWidth;
		const int32_t groupWidth = ratioWidth + ratioStatusGap + statusWidth;
		const int32_t panelLeft = (int32_t)xPos;
		const int32_t panelRight = panelLeft + panelWidth - 1;
		const int32_t minimumGroupX = panelLeft + 12; /* protect channel number */

		int32_t groupX = panelRight - groupWidth - 2; /* extra inset keeps lit LEDs inside the track */
		if (groupX < minimumGroupX)
			groupX = minimumGroupX;

		const int32_t ratioX = groupX;
		const int32_t directionX = ratioX + ratioWidth + ratioStatusGap;
		const uint16_t lagLedX = (uint16_t)(directionX + directionMarkerWidth + directionLedGap);
		const uint16_t syncLedX = (uint16_t)(lagLedX + 4);
		const uint16_t leadLedX = (uint16_t)(syncLedX + 4);
		const uint16_t songBadgeX = (uint16_t)(leadLedX + 3 + songBadgeGap);

		const int32_t colonX = ratioX + (numeratorDigits * FONT3_CHAR_W);
		const uint32_t ratioColor = video.palette[PAL_BLCKTXT];
		if (reversed)
		{
			/* A solid reverse badge must be unmistakable during performance. */
			fillRect((uint16_t)directionX, yPos, 7, 8, PAL_BLCKMRK);
			textOutTiny(directionX + 1, yPos + 1, "R", video.palette[PAL_BLCKTXT]);
		}

		textOutTiny(ratioX, yPos + 1, numeratorText, ratioColor);
		video.frameBuffer[((yPos + 3) * SCREEN_W) + colonX + 1] = ratioColor;
		video.frameBuffer[((yPos + 5) * SCREEN_W) + colonX + 1] = ratioColor;
		textOutTiny(colonX + FONT3_CHAR_W, yPos + 1, denominatorText, ratioColor);

		if (songMode)
		{
			/* The solid badge identifies Song transport and reports the private
			** order-list position. This remains diagnostic when several order
			** entries point to the same pattern. */
			char orderText[3];
			snprintf(orderText, sizeof (orderText), "%02X", sourceOrder & 0xFF);
			fillRect(songBadgeX, yPos, (uint16_t)songBadgeWidth, 8, PAL_BLCKMRK);
			textOutTiny(songBadgeX + 1, yPos + 1, orderText, video.palette[PAL_BLCKTXT]);
		}

		/* Draw all three housings so the indicator reads as a tiny LED bank even
		** when only one lamp is active. */
		const uint32_t ledOffColor = breatheColorToward(video.palette[PAL_BLCKTXT], video.palette[panelColor]);
		drawFastTracksPOCLed(lagLedX,  (uint16_t)(yPos + 2), ledOffColor);
		drawFastTracksPOCLed(syncLedX, (uint16_t)(yPos + 2), ledOffColor);
		drawFastTracksPOCLed(leadLedX, (uint16_t)(yPos + 2), ledOffColor);

		const bool masterAligned = track->masterAligned;
		if (masterAligned)
		{
			/* Deliberately literal green: exact master synchronization should be
			** recognizable independently of the current FT2 palette. */
			const uint32_t syncColor = breatheColorToward(0xFF00D040, video.palette[panelColor]);
			drawFastTracksPOCLed(syncLedX, (uint16_t)(yPos + 2), syncColor);
		}
		else if (!songMode)
		{
			const int32_t numRows = song.currNumRows > 0 ? song.currNumRows : 1;
			int32_t phaseOffset = sourceRow - song.row;
			phaseOffset %= numRows;
			if (phaseOffset < 0)
				phaseOffset += numRows;
			if (phaseOffset > numRows / 2)
				phaseOffset -= numRows;

			/* Red retains the established Fast Tracks phase meaning. During clutch,
			** the configurable block-mark color remains the temporary emphasis. */
			const uint32_t phaseBaseColor = clutchHeld ? video.palette[PAL_BLCKMRK] : 0xFFFF3030;
			const uint32_t phaseColor = breatheColorToward(phaseBaseColor, video.palette[panelColor]);
			if (phaseOffset < 0)
			{
				drawFastTracksPOCLed(lagLedX, (uint16_t)(yPos + 2), phaseColor);
			}
			else if (phaseOffset > 0)
			{
				drawFastTracksPOCLed(leadLedX, (uint16_t)(yPos + 2), phaseColor);
			}
			else
			{
				/* Same displayed row but not truly transport-aligned: illuminate both
				** sides instead of falsely claiming a direction or exact sync. */
				drawFastTracksPOCLed(lagLedX,  (uint16_t)(yPos + 2), phaseColor);
				drawFastTracksPOCLed(leadLedX, (uint16_t)(yPos + 2), phaseColor);
			}
		}
	}
}

static void drawFastTracksPOCArrow(uint32_t xPos, uint32_t yPos)
{
	// Tiny right-pointing clock-hand marker in the channel separator.
	// The pattern data remains fixed; this marker shows the private row Track 8 is reading.
	hLine((uint16_t)(xPos + 2), (uint16_t)(yPos + 1), 1, PAL_FORGRND);
	hLine((uint16_t)(xPos + 1), (uint16_t)(yPos + 2), 2, PAL_FORGRND);
	hLine((uint16_t)(xPos + 0), (uint16_t)(yPos + 3), 3, PAL_FORGRND);
	hLine((uint16_t)(xPos + 1), (uint16_t)(yPos + 4), 2, PAL_FORGRND);
	hLine((uint16_t)(xPos + 2), (uint16_t)(yPos + 5), 1, PAL_FORGRND);
}

void writePattern(int32_t currRow, int32_t currPattern)
{
	uint32_t noteTextColors[2];


	/* Draw pattern framework every time (erasing existing content).
	** FT2 doesn't do this. This is quite lazy and consumes more CPU
	** time than needed (overlapped drawing), but it makes the pattern
	** mark/cursor drawing MUCH simpler to implement...
	*/
	drawPatternBorders();

	// setup variables

	uint32_t chans = ui.numChannelsShown;
	if (chans > ui.maxVisibleChannels)
		chans = ui.maxVisibleChannels;

	ASSERT(chans >= 2 && chans <= 12);

	// get channel width
	const uint32_t chanWidth = chanWidths[(chans / 2) - 1];
	ui.patternChannelWidth = (uint16_t)(chanWidth + 3);

	// get heights/pos/rows depending on configuration
	uint32_t rowHeight = config.ptnStretch ? 11 : 8;
	const pattCoord_t *pattCoord = &pattCoordTable[config.ptnStretch][ui.pattChanScrollShown][getPatternEditorView()];
	const pattCoord2_t *pattCoord2 = &pattCoord2Table[config.ptnStretch][ui.pattChanScrollShown][getPatternEditorView()];
	const int32_t midRowTextY = pattCoord->midRowTextY;
	const int32_t lowerRowsTextY = pattCoord->lowerRowsTextY;
	int32_t row = currRow - pattCoord->numUpperRows;
	const int32_t rowsOnScreen = pattCoord->numUpperRows + 1 + pattCoord->numLowerRows;
	int32_t textY = pattCoord->upperRowsTextY;
	const int32_t afterCurrRow = currRow + 1;
	const int32_t numChannels = ui.numChannelsShown;
	note_t *pattPtr = pattern[currPattern];
	const int32_t numRows = patternNumRows[currPattern];
	fastTracksSnapshot_t fastTracksSnapshot;
	fastTracksPOCGetSnapshot(&fastTracksSnapshot);

	// increment pattern data pointer by horizontal scrollbar offset/channel
	if (pattPtr != NULL)
		pattPtr += ui.channelOffset;

	noteTextColors[0] = video.palette[PAL_PATTEXT]; // not selected
	noteTextColors[1] = video.palette[PAL_FORGRND]; // selected

	// draw pattern data
	for (int32_t i = 0; i < rowsOnScreen; i++)
	{
		if (row >= 0)
		{
			const bool selectedRowFlag = (row == currRow);

			drawRowNums(textY, (uint8_t)row, selectedRowFlag);

			const note_t *p = (pattPtr == NULL) ? emptyPattern : &pattPtr[(uint32_t)row * MAX_CHANNELS];
			const int32_t xWidth = ui.patternChannelWidth;
			const uint32_t color = noteTextColors[selectedRowFlag];

			// Every enabled Fast Track is rendered as its own private scrolling tape
			// window, centered on that channel's current ratio-driven source row.
			int32_t xPos = 29;
			for (int32_t j = 0; j < numChannels; j++, p++, xPos += xWidth)
			{
				const int32_t absoluteChannel = ui.channelOffset + j;
				const fastTracksTrackSnapshot_t *fastTrack = &fastTracksSnapshot.tracks[absoluteChannel];
				const bool fastTrackVisible = fastTrack->enabled;
				const note_t *drawPtr = p;

				if (fastTrackVisible)
				{
					if (selectedRowFlag)
					{
						const uint32_t arrowX = 29 + (j * ui.patternChannelWidth) - 3;
						drawFastTracksPOCArrow(arrowX, (uint32_t)textY);
					}

					const int32_t sourcePattern = fastTrack->sourcePattern;
					const int32_t sourceNumRows =
						patternNumRows[sourcePattern] > 0 ?
						patternNumRows[sourcePattern] : 1;
					int32_t privateRow =
						fastTrack->sourceRow + (i - pattCoord->numUpperRows);
					while (privateRow < 0)
						privateRow += sourceNumRows;
					while (privateRow >= sourceNumRows)
						privateRow -= sourceNumRows;

					drawPtr = (pattern[sourcePattern] == NULL)
						? emptyPattern
						: &pattern[sourcePattern][(privateRow * MAX_CHANNELS) + absoluteChannel];
				}

				// Theme-safe Fast Tracks coloring: only populated event fields receive
				// the emphasized-row palette color. Empty dots/dashes retain FT2's
				// normal row color, making the event data look attached to the moving barrel.
				uint32_t noteColor = color;
				uint32_t instColor = color;
				uint32_t volColor = color;
				uint32_t efxColor = color;
				uint32_t tuneColor = color;

				if (fastTrackVisible)
				{
					const bool clutchHeld = fastTrack->clutched;
					const uint32_t fastTrackColor = clutchHeld
						? video.palette[PAL_BLCKMRK]
						: video.palette[PAL_BLCKTXT];

					if (drawPtr->note != 0)
						noteColor = fastTrackColor;
					if (drawPtr->instr != 0)
						instColor = fastTrackColor;
					if (drawPtr->vol != 0)
						volColor = fastTrackColor;
					if (drawPtr->efx != 0 || drawPtr->efxData != 0)
						efxColor = fastTrackColor;
					if (drawPtr->tuneType != 0)
						tuneColor = fastTrackColor;
				}

				drawAdaptiveCell(xPos, textY, drawPtr, noteColor, instColor,
					volColor, tuneColor, efxColor);
			}
		}

		// next row
		if (++row >= numRows)
			break;

		// adjust textY position
		if (row == currRow)
			textY = midRowTextY;
		else if (row == afterCurrRow)
			textY = lowerRowsTextY;
		else
			textY += rowHeight;
	}

	writeCursor();

	// draw pattern marking (if anything is marked)
	if (pattMark.markY1 != pattMark.markY2)
		writePatternBlockMark(currRow, rowHeight, pattCoord);

	// Draw the Fast Tracks panel first, then restore channel numbers over it.
	drawFastTracksPOCStatus(pattCoord2->upperRowsY+2, &fastTracksSnapshot);

	// channel numbers must be drawn lastly
	if (config.ptnChnNumbers)
		drawChannelNumbering(pattCoord2->upperRowsY+2);

	drawPatternNavPopup();
}

// ========== CHARACTER DRAWING ROUTINES FOR PATTERN EDITOR ==========

void pattTwoHexOut(uint32_t xPos, uint32_t yPos, uint8_t val, uint32_t color)
{
	const uint8_t *ch1Ptr = &font4Ptr[(val   >> 4) * FONT4_CHAR_W];
	const uint8_t *ch2Ptr = &font4Ptr[(val & 0x0F) * FONT4_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (ch1Ptr[x] != 0) dstPtr[x] = color;
			if (ch2Ptr[x] != 0) dstPtr[FONT4_CHAR_W+x] = color;
		}

		ch1Ptr += FONT4_WIDTH;
		ch2Ptr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void pattCharOut(uint32_t xPos, uint32_t yPos, uint8_t chr, uint8_t fontType, uint32_t color)
{
	const uint8_t *srcPtr;
	int32_t x, y;

	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	if (fontType == FONT_TYPE3)
	{
		srcPtr = &bmp.font3[chr * FONT3_CHAR_W];
		for (y = 0; y < FONT3_CHAR_H; y++)
		{
			for (x = 0; x < FONT3_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT3_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
	else if (fontType == FONT_TYPE4)
	{
		srcPtr = &font4Ptr[chr * FONT4_CHAR_W];
		for (y = 0; y < FONT4_CHAR_H; y++)
		{
			for (x = 0; x < FONT4_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT4_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
	else if (fontType == FONT_TYPE5)
	{
		srcPtr = &font5Ptr[chr * FONT5_CHAR_W];
		for (y = 0; y < FONT5_CHAR_H; y++)
		{
			for (x = 0; x < FONT5_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT5_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
	else
	{
		srcPtr = &bmp.font7[chr * FONT7_CHAR_W];
		for (y = 0; y < FONT7_CHAR_H; y++)
		{
			for (x = 0; x < FONT7_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT7_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
}

static void drawEmptyNoteSmall(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	if (config.ptnAlternativeLayout)
	{
		dstPtr += (3 * SCREEN_W) + 2;
		*dstPtr = color;
		dstPtr += 5;
		*dstPtr = color;
		dstPtr += 5;
		*dstPtr = color;
	}
	else
	{
		const uint8_t *srcPtr = &bmp.font7[18 * FONT7_CHAR_W];

		for (int32_t y = 0; y < FONT7_CHAR_H; y++)
		{
			for (int32_t x = 0; x < FONT7_CHAR_W*3; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT7_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
}

static void drawKeyOffSmall(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr = &bmp.font7[21 * FONT7_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + (xPos + 2)];

	for (int32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT7_CHAR_W*2; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = color;
		}

		srcPtr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteSmall(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color)
{
	uint32_t char1, char2;

	noteNum--;

	const uint8_t note = noteTab1[noteNum];
	const uint32_t char3 = noteTab2[noteNum] * FONT7_CHAR_W;

	if (config.ptnAcc == 0)
	{
		char1 = sharpNote1Char_small[note];
		char2 = sharpNote2Char_small[note];
	}
	else
	{
		char1 = flatNote1Char_small[note];
		char2 = flatNote2Char_small[note];
	}

	const uint8_t *ch1Ptr = &bmp.font7[char1];
	const uint8_t *ch2Ptr = &bmp.font7[char2];
	const uint8_t *ch3Ptr = &bmp.font7[char3];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT7_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT7_CHAR_W; x++)
		{
			if (ch1Ptr[x] != 0) dstPtr[x] = color;
			if (ch2Ptr[x] != 0) dstPtr[FONT7_CHAR_W+x] = color;
			if (ch3Ptr[x] != 0) dstPtr[((FONT7_CHAR_W*2)-2)+x] = color;
		}

		ch1Ptr += FONT7_WIDTH;
		ch2Ptr += FONT7_WIDTH;
		ch3Ptr += FONT7_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawEmptyNoteMedium(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	if (config.ptnAlternativeLayout)
	{
		dstPtr += (3 * SCREEN_W) + 3;
		*dstPtr = color;
		dstPtr += 8;
		*dstPtr = color;
		dstPtr += 8;
		*dstPtr = color;
	}
	else
	{
		const uint8_t *srcPtr = &font4Ptr[43 * FONT4_CHAR_W];

		for (int32_t y = 0; y < FONT4_CHAR_H; y++)
		{
			for (int32_t x = 0; x < FONT4_CHAR_W*3; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT4_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
}

static void drawKeyOffMedium(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr = &font4Ptr[40 * FONT4_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W*3; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = color;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteMedium(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color)
{
	uint32_t char1, char2;

	noteNum--;

	const uint8_t note = noteTab1[noteNum];
	const uint32_t char3 = noteTab2[noteNum] * FONT4_CHAR_W;

	if (config.ptnAcc == 0)
	{
		char1 = sharpNote1Char_med[note];
		char2 = sharpNote2Char_med[note];
	}
	else
	{
		char1 = flatNote1Char_med[note];
		char2 = flatNote2Char_med[note];
	}

	const uint8_t *ch1Ptr = &font4Ptr[char1];
	const uint8_t *ch2Ptr = &font4Ptr[char2];
	const uint8_t *ch3Ptr = &font4Ptr[char3];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W; x++)
		{
			if (ch1Ptr[x] != 0) dstPtr[x] = color;
			if (ch2Ptr[x] != 0) dstPtr[FONT4_CHAR_W+x] = color;
			if (ch3Ptr[x] != 0) dstPtr[(FONT4_CHAR_W*2)+x] = color;
		}

		ch1Ptr += FONT4_WIDTH;
		ch2Ptr += FONT4_WIDTH;
		ch3Ptr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawEmptyNoteBig(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	if (config.ptnAlternativeLayout)
	{
		dstPtr += (3 * SCREEN_W) + 7;
		*dstPtr = color;
		dstPtr += 16;
		*dstPtr = color;
		dstPtr += 16;
		*dstPtr = color;
	}
	else
	{
		const uint8_t *srcPtr = &font4Ptr[67 * FONT4_CHAR_W];

		for (int32_t y = 0; y < FONT4_CHAR_H; y++)
		{
			for (int32_t x = 0; x < FONT4_CHAR_W*6; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT4_WIDTH;
			dstPtr += SCREEN_W;
		}
	}
}

static void drawKeyOffBig(uint32_t xPos, uint32_t yPos, uint32_t color)
{
	const uint8_t *srcPtr = &bmp.font4[61 * FONT4_CHAR_W];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT4_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT4_CHAR_W*6; x++)
		{
			if (srcPtr[x] != 0)
				dstPtr[x] = color;
		}

		srcPtr += FONT4_WIDTH;
		dstPtr += SCREEN_W;
	}
}

static void drawNoteBig(uint32_t xPos, uint32_t yPos, int32_t noteNum, uint32_t color)
{
	uint32_t char1, char2;

	noteNum--;

	const uint8_t note = noteTab1[noteNum];
	const uint32_t char3 = noteTab2[noteNum] * FONT5_CHAR_W;

	if (config.ptnAcc == 0)
	{
		char1 = sharpNote1Char_big[note];
		char2 = sharpNote2Char_big[note];
	}
	else
	{
		char1 = flatNote1Char_big[note];
		char2 = flatNote2Char_big[note];
	}

	const uint8_t *ch1Ptr = &font5Ptr[char1];
	const uint8_t *ch2Ptr = &font5Ptr[char2];
	const uint8_t *ch3Ptr = &font5Ptr[char3];
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t y = 0; y < FONT5_CHAR_H; y++)
	{
		for (int32_t x = 0; x < FONT5_CHAR_W; x++)
		{
			if (ch1Ptr[x] != 0) dstPtr[x] = color;
			if (ch2Ptr[x] != 0) dstPtr[FONT5_CHAR_W+x] = color;
			if (ch3Ptr[x] != 0) dstPtr[(FONT5_CHAR_W*2)+x] = color;
		}

		ch1Ptr += FONT5_WIDTH;
		ch2Ptr += FONT5_WIDTH;
		ch3Ptr += FONT5_WIDTH;
		dstPtr += SCREEN_W;
	}
}
