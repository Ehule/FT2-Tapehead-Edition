#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_pattern_ed.h"
#include "ft2_pattern_launcher.h"
#include "ft2_pattern_launcher_ui.h"
#include "ft2_mouse.h"
#include "ft2_video.h"
#include "ft2_structs.h"
#include "ft2_replayer.h"
#include "ft2_keyboard.h"
#include "ft2_bmp.h"
#include "ft2_sysreqs.h"
#include "ft2_poly_matrix.h"
#include "ft2_audio.h"
#include "ft2_sample_launcher.h"

static bool patternLauncherPanelShown;
static bool sampleLauncherDeckShown;
static uint8_t patternLauncherPage;
static uint8_t patternLauncherBreatheFrame;
static bool patternLauncherSongPatterns[MAX_PATTERNS];
static bool patternLauncherVisiblePatterns[32];
static bool patternLauncherExposedPatterns[MAX_PATTERNS];
static bool patternLauncherExposureInitialized;

static char *patternLauncherPageCaptions[8] = { "00-1F", "20-3F", "40-5F", "60-7F", "80-9F", "A0-BF", "C0-DF", "E0-FF" };
static char *instrumentBankCaptions[8] = { "01-08", "09-10", "11-18", "19-20", "21-28", "29-30", "31-38", "39-40" };
static char *sampleLauncherPageCaptions[8] = { "00-1F", "--", "--", "--", "--", "--", "--", "--" };

bool patternLauncherPanelIsShown(void)
{
	return patternLauncherPanelShown;
}

bool patternLauncherPanelIsSampleDeck(void)
{
	return patternLauncherPanelShown && sampleLauncherDeckShown;
}

void patternLauncherResetExposure(void)
{
	for (uint16_t i = 0; i < MAX_PATTERNS; i++)
		patternLauncherExposedPatterns[i] = true;
	patternLauncherExposureInitialized = true;
}

static int8_t getPatternLauncherQueuePos(int16_t patternNum)
{
	const uint8_t queueCount = patternLauncherGetQueueCount();
	for (uint8_t i = 0; i < queueCount; i++)
	{
		if (patternLauncherGetQueueItem(i) == patternNum)
			return (int8_t)i;
	}

	return -1;
}

static uint32_t blendPatternLauncherColor(uint32_t foreground, uint32_t background, uint8_t level)
{
	const uint16_t inverse = 255 - level;
	const uint8_t r = (uint8_t)((((foreground >> 16) & 0xFF) * level + ((background >> 16) & 0xFF) * inverse) / 255);
	const uint8_t g = (uint8_t)((((foreground >> 8) & 0xFF) * level + ((background >> 8) & 0xFF) * inverse) / 255);
	const uint8_t b = (uint8_t)(((foreground & 0xFF) * level + (background & 0xFF) * inverse) / 255);
	return 0xFF000000 | (r << 16) | (g << 8) | b;
}

static void fillPatternLauncherRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color)
{
	uint32_t *dst = &video.frameBuffer[(y * SCREEN_W) + x];
	for (uint16_t yy = 0; yy < h; yy++, dst += SCREEN_W)
	{
		for (uint16_t xx = 0; xx < w; xx++)
			dst[xx] = color;
	}
}

static uint8_t getPatternLauncherBreatheLevel(void)
{
	uint16_t distance = patternLauncherBreatheFrame;
	if (distance > 60)
		distance = 120 - distance;

	return (uint8_t)(255 - ((distance * 105) / 60));
}

static void hexOutPatternLauncherColor(uint16_t xPos, uint16_t yPos,
	uint32_t color, uint32_t val, uint8_t numDigits)
{
	uint32_t *dstPtr = &video.frameBuffer[(yPos * SCREEN_W) + xPos];

	for (int32_t i = numDigits - 1; i >= 0; i--)
	{
		const uint8_t *srcPtr =
			&bmp.font6[((val >> (i * 4)) & 15) * FONT6_CHAR_W];

		for (int32_t y = 0; y < FONT6_CHAR_H; y++)
		{
			for (int32_t x = 0; x < FONT6_CHAR_W; x++)
			{
				if (srcPtr[x] != 0)
					dstPtr[x] = color;
			}

			srcPtr += FONT6_WIDTH;
			dstPtr += SCREEN_W;
		}

		dstPtr -= (SCREEN_W * FONT6_CHAR_H) - FONT6_CHAR_W;
	}
}

static bool rebuildPatternLauncherSongUsage(void)
{
	if (!patternLauncherExposureInitialized)
		patternLauncherResetExposure();
	bool changed = false;
	bool songPatterns[MAX_PATTERNS] = { false };

	for (uint16_t i = 0; i < song.songLength; i++)
		songPatterns[song.orders[i]] = true;

	for (uint16_t i = 0; i < MAX_PATTERNS; i++)
	{
		if (patternLauncherSongPatterns[i] != songPatterns[i])
			changed = true;

		patternLauncherSongPatterns[i] = songPatterns[i];
	}

	return changed;
}

static bool rebuildPatternLauncherVisibleUsage(void)
{
	bool changed = false;
	const uint16_t firstPattern = patternLauncherPage * 32;

	for (uint16_t i = 0; i < 32; i++)
	{
		const bool patternUsed = pattern[firstPattern + i] != NULL;
		if (patternLauncherVisiblePatterns[i] != patternUsed)
			changed = true;

		patternLauncherVisiblePatterns[i] = patternUsed;
	}

	return changed;
}

void patternLauncherNotifySongOrderChanged(void)
{
	const bool changed = rebuildPatternLauncherSongUsage();
	if (changed && patternLauncherPanelShown)
		patternLauncherDrawPanel();
}

void patternLauncherNotifyPatternChanged(uint16_t patternNum)
{
	/* Keep both halves of the color classification current at the mutation
	** boundary. This avoids a newly populated song pattern being drawn with
	** the Matrix-only color while waiting for separate order/usage polling. */
	rebuildPatternLauncherSongUsage();
	rebuildPatternLauncherVisibleUsage();

	if (patternLauncherPanelShown && patternNum / 32 == patternLauncherPage)
		patternLauncherDrawPanel();
}

static void drawSampleLauncherPanel(void)
{
	const int16_t current = sampleLauncherGetQCurrent();
	const uint8_t count = sampleLauncherGetTileCount();
	static const uint8_t queueLevels[4] = { 255, 210, 170, 135 };
	clearRect(421, 3, 166, 152);

	for (int16_t row = 0; row < 8; row++)
	{
		for (int16_t col = 0; col < 4; col++)
		{
			const uint8_t tile = (uint8_t)((row * 4) + col);
			const int16_t x = 423 + (col * 41);
			const int16_t y = 4 + (row * 19);
			const bool loaded = tile < count;
			const int8_t queuePos = sampleLauncherGetQQueuePos(tile);
			const int8_t polySlot = sampleLauncherGetPolySlot(tile);
			const bool polyStart = sampleLauncherPolyStartPending(tile);
			const bool polyStop = sampleLauncherPolyStopPending(tile);

			drawFramework(x, y, 40, 18, FRAMEWORK_TYPE1);
			if (loaded && tile == current)
			{
				const uint32_t color = sampleLauncherQStopPending()
					? 0xFFE34234
					: blendPatternLauncherColor(0xFF39C85A,
						video.palette[PAL_DESKTOP], getPatternLauncherBreatheLevel());
				fillPatternLauncherRect(x + 2, y + 2, 36, 14, color);
				if (polySlot >= 0 || polyStart)
					fillPatternLauncherRect(x + 2, y + 13, 36, 3,
						polyStop ? 0xFF287A83 : 0xFF35C9D0);
			}
			else if (queuePos >= 0)
			{
				fillPatternLauncherRect(x + 2, y + 2, 36, 14,
					blendPatternLauncherColor(video.palette[PAL_BUTTONS],
						video.palette[PAL_DESKTOP], queueLevels[queuePos]));
			}
			else if (polySlot >= 0 || polyStart)
			{
				fillPatternLauncherRect(x + 2, y + 2, 36, 14,
					polyStop ? 0xFF287A83 :
					(polyStart ? 0xFF2F9D87 : 0xFF35C9D0));
			}

			if (loaded)
			{
				char shortName[5] = { 0 };
				const char *name = sampleLauncherGetTileName(tile);
				uint8_t writePos = 0;
				for (uint8_t readPos = 0; name[readPos] != '\0' &&
					writePos < 4; readPos++)
				{
					if (isalnum((unsigned char)name[readPos]))
						shortName[writePos++] =
							(char)toupper((unsigned char)name[readPos]);
				}
				if (writePos > 0)
					textOutTinyOutline(x + 10, y + 5, shortName);
				else
					hexOut(x + 14, y + 5, PAL_PATTEXT, tile, 2);
				char busText[2] = {
					(char)('A' + sampleLauncherGetTileBus(tile)), '\0' };
				textOutTinyOutline(x + 4, y + 5, busText);
			}
			else
			{
				hexOut(x + 14, y + 5, PAL_DSKTOP2, tile, 2);
			}

			if (polySlot >= 0)
			{
				char slotText[2] = { (char)('1' + polySlot), '\0' };
				textOutTinyOutline(x + 32, y + 5, slotText);
			}
			else if (queuePos >= 0)
			{
				char queueText[2] = { (char)('1' + queuePos), '\0' };
				textOutTinyOutline(x + 32, y + 5, queueText);
			}
		}
	}
}

void patternLauncherDrawPanel(void)
{
	if (sampleLauncherDeckShown)
	{
		drawSampleLauncherPanel();
		return;
	}

	const int16_t current = patternLauncherGetCurrent();
	const uint8_t exitMode = patternLauncherGetExitMode();
	static const uint8_t queueLevels[4] = { 255, 210, 170, 135 };

	rebuildPatternLauncherVisibleUsage();
	clearRect(421, 3, 166, 152);

	for (int16_t row = 0; row < 8; row++)
	{
		for (int16_t col = 0; col < 4; col++)
		{
			const int16_t patternNum = (patternLauncherPage * 32) + (row * 4) + col;
			const int16_t x = 423 + (col * 41);
			const int16_t y = 4 + (row * 19);
			const int8_t queuePos = getPatternLauncherQueuePos(patternNum);
			const bool patternUsed = pattern[patternNum] != NULL;
			const bool exposed = patternLauncherExposedPatterns[patternNum];
			const bool songPattern = patternLauncherSongPatterns[patternNum];
			const bool polyActive =
				polyMatrixIsPatternActive((uint8_t)patternNum);
			const uint8_t polySlot =
				polyMatrixGetPatternSlot((uint8_t)patternNum);
			const bool qToPolyPending =
				patternLauncherPolyHandoffIsPending((uint8_t)patternNum);
			const uint32_t polyColor =
				polyMatrixPatternStopPending((uint8_t)patternNum) ?
				0xFF287A83 :
				(polyMatrixPatternQHandoffPending((uint8_t)patternNum) ?
					0xFF2F9D87 : 0xFF35C9D0);

			drawFramework(x, y, 40, 18, FRAMEWORK_TYPE1);
			if (patternNum == current)
			{
				/* Semantic transport colors stay readable regardless of the current FT2 theme.
				** Green = Matrix continues, yellow = return, orange = next order, red = stop.
				*/
				uint32_t activeColor;
				if (exitMode == 1) activeColor = 0xFFFFD43B;      /* return to saved order */
				else if (exitMode == 2) activeColor = 0xFFE34234; /* graceful stop */
				else if (exitMode == 3) activeColor = 0xFFFF8A2B; /* continue at next order */
				else activeColor = blendPatternLauncherColor(0xFF39C85A,
					video.palette[PAL_DESKTOP], getPatternLauncherBreatheLevel());
				fillPatternLauncherRect(x + 2, y + 2, 36, 14, activeColor);

				/* A cyan foot shows that the same tile still owns an
				** independent Poly spool beneath its ordinary Q state. */
				if (polyActive || qToPolyPending)
					fillPatternLauncherRect(x + 2, y + 13, 36, 3, polyColor);
			}
			else if (queuePos >= 0)
			{
				const uint32_t queueColor = blendPatternLauncherColor(video.palette[PAL_BUTTONS],
					video.palette[PAL_DESKTOP], queueLevels[queuePos]);
				fillPatternLauncherRect(x + 2, y + 2, 36, 14, queueColor);
				if (polyActive)
					fillPatternLauncherRect(x + 2, y + 13, 36, 3, polyColor);
			}
			else if (polyActive)
			{
				/* Cyan = independently threaded Poly Matrix spool. A pending
				** graceful pull darkens it until the current revolution ends. */
				fillPatternLauncherRect(x + 2, y + 2, 36, 14, polyColor);
			}

			/* Text color describes membership while the cell background is
			** reserved for transport state. Song patterns follow Pattern Text;
			** populated Matrix-only patterns follow Mouse/channel-header color.
			*/
			if (!exposed)
			{
				hexOutPatternLauncherColor(x + 14, y + 5,
					blendPatternLauncherColor(video.palette[PAL_DSKTOP2],
						video.palette[PAL_BUTTONS], 90), patternNum, 2);
				textOutTinyOutline(x + 5, y + 5, "X");
			}
			else if (!patternUsed && songPattern)
			{
				const uint32_t dimSongColor = blendPatternLauncherColor(
					video.palette[PAL_PATTEXT], video.palette[PAL_BUTTONS], 115);
				hexOutPatternLauncherColor(x + 14, y + 5,
					dimSongColor, patternNum, 2);
			}
			else
			{
				const uint8_t textPal = patternUsed ?
					(songPattern ? PAL_PATTEXT : PAL_MOUSEPT) : PAL_DSKTOP2;
				hexOut(x + 14, y + 5, textPal, patternNum, 2);
			}

			/* Keep a tiny, stable 1..4 label attached to each active Poly
			** spool. It remains visible through Q overlap and handoff states. */
			if (polySlot != 0)
			{
				char slotText[2] = { (char)('0' + polySlot), '\0' };
				textOutTinyOutline(x + 5, y + 5, slotText);
			}

			/* The queue gradient is deliberately theme-derived, so its four
			** levels can be subtle in some palettes. Mirror the waiting order
			** with an explicit 1..4 marker on the right edge. The active Q
			** tile is already identified by its breathing/semantic color and
			** is not part of this next-up numbering. */
			if (queuePos >= 0)
			{
				char queueText[2] = { (char)('1' + queuePos), '\0' };
				textOutTinyOutline(x + 32, y + 5, queueText);
			}
		}
	}
}

bool patternLauncherHandlePanelMiddleClick(int16_t x, int16_t y, bool shiftPressed)
{
	if (!patternLauncherPanelShown || x < 423 || x >= 587 || y < 4 || y >= 156)
		return false;

	const int16_t col = (x - 423) / 41;
	const int16_t row = (y - 4) / 19;
	if (col < 0 || col > 3 || row < 0 || row > 7 ||
		((x - 423) % 41) >= 40 || ((y - 4) % 19) >= 18)
		return true;

	const uint8_t patternNum =
		(uint8_t)((patternLauncherPage * 32) + (row * 4) + col);
	if (sampleLauncherDeckShown)
	{
		const uint8_t tile = (uint8_t)((row * 4) + col);
		if (tile < sampleLauncherGetTileCount() &&
			!sampleLauncherTogglePoly(tile))
		{
			okBox(0, "Sample Matrix", "No more track Lanes available", NULL);
		}
		patternLauncherDrawPanel();
		return true;
	}
	if (!patternLauncherExposedPatterns[patternNum])
		return true;

	/* An active Q tile transfers at Q's next loop boundary. A tile already
	** active in Poly keeps the established pull gesture instead. */
	if (patternLauncherGetCurrent() == patternNum &&
		!polyMatrixIsPatternActive(patternNum))
	{
		patternLauncherRequestPolyHandoff(patternNum);
	}
	else
	{
		if (!polyMatrixTogglePattern(patternNum, shiftPressed))
			okBox(0, "Poly Matrix", "No more track Lanes available", NULL);
	}

	patternLauncherDrawPanel();
	return true;
}

static void drawPatternLauncherShell(void)
{
	/* Keep FT2's native bank-button chrome intact. Matrix only borrows the
	** combined instrument/sample list surface and changes button captions.
	*/
	clearRect(421, 0, 166, 155);
	drawFramework(421, 0, 166, 155, FRAMEWORK_TYPE1);
	patternLauncherDrawPanel();
}

static void drawPatternLauncherBankColumn(void)
{
	/* The Matrix reuses the instrument-bank buttons, but Configuration and
	** other full-screen views can leave pixels behind in the gaps around
	** them. Restore the complete native parent column before drawing the
	** eight Matrix page buttons.
	*/
	clearRect(587, 0, 45, 173);
	drawFramework(587,   0, 45, 71, FRAMEWORK_TYPE1);
	drawFramework(587,  71, 45, 71, FRAMEWORK_TYPE1);
	drawFramework(587, 142, 45, 31, FRAMEWORK_TYPE1);
}


void patternLauncherForceRedraw(void)
{
	if (!patternLauncherPanelShown)
		return;

	rebuildPatternLauncherSongUsage();

	/* Config/Layout and other full-screen views can overwrite the borrowed
	** instrument/sample surface. Rebuild the shell and restore Matrix labels.
	*/
	drawPatternLauncherBankColumn();
	for (uint16_t i = 0; i < 8; i++)
	{
		pushButtons[PB_RANGE1 + i].caption = sampleLauncherDeckShown
			? sampleLauncherPageCaptions[i] : patternLauncherPageCaptions[i];
		showPushButton(PB_RANGE1 + i);
	}

	pushButtons[PB_SWAP_BANK].caption = "Exit";
	pushButtons[PB_SWAP_BANK].caption2 = sampleLauncherDeckShown ? "Samp." : "Patt.";
	showPushButton(PB_SWAP_BANK);
	drawPatternLauncherShell();
}

void patternLauncherSetPage(uint8_t page)
{
	if (sampleLauncherDeckShown)
		return;
	patternLauncherPage = page & 7;
	if (patternLauncherPanelShown)
		patternLauncherDrawPanel();
}

bool patternLauncherHandlePanelClick(int16_t x, int16_t y)
{
	if (!patternLauncherPanelShown || x < 423 || x >= 587 || y < 4 || y >= 156)
		return false;

	/* testInstrSwitcherMouseDown() is called every frame while the mouse is
	** held. Only the initial mouse-down may launch/toggle a pattern.
	*/
	if (mouse.lastUsedObjectType == OBJECT_INSTRSWITCH)
		return true;

	const int16_t col = (x - 423) / 41;
	const int16_t row = (y - 4) / 19;
	if (col < 0 || col > 3 || row < 0 || row > 7)
		return true;

	const int16_t cellX = (x - 423) % 41;
	const int16_t cellY = (y - 4) % 19;
	if (cellX >= 40 || cellY >= 18)
		return true;

	const uint8_t patternNum =
		(uint8_t)((patternLauncherPage * 32) + (row * 4) + col);
	if (sampleLauncherDeckShown)
	{
		const uint8_t tile = (uint8_t)((row * 4) + col);
		if (tile >= sampleLauncherGetTileCount())
			return true;

		if (keyb.leftAltPressed)
			sampleLauncherCycleTileBus(tile, audio.monoOutputMode
				? (uint8_t)(audio.outputBusCount * 2) : audio.outputBusCount);
		else if (!mouse.rightButtonPressed)
			sampleLauncherRequestQ(tile);

		patternLauncherDrawPanel();
		return true;
	}

	if (keyb.leftAltPressed && !mouse.rightButtonPressed)
	{
		patternLauncherExposedPatterns[patternNum] ^= 1;
		patternLauncherDrawPanel();
		return true;
	}
	if (!patternLauncherExposedPatterns[patternNum])
		return true;

	if (mouse.rightButtonPressed)
	{
		if (keyb.leftShiftPressed)
		{
			char message[96];
			snprintf(message, sizeof (message),
				"Delete pattern %02X and remove all song references?", patternNum);
			if (okBox(2, "Pattern Matrix", message, NULL) == 1)
				patternMatrixClearPattern(patternNum, true);
		}
		else if (keyb.leftCtrlPressed)
		{
			char message[96];
			snprintf(message, sizeof (message),
				"Clear pattern %02X data but keep all song references?", patternNum);
			if (okBox(2, "Pattern Matrix", message, NULL) == 1)
				patternMatrixClearPattern(patternNum, false);
		}

		patternLauncherDrawPanel();
		return true;
	}

	/*
	** Ordinary left-click remains independent. Ctrl+Shift is the explicit
	** exception: arm an atomic Poly -> Q transfer at the spool's next group
	** boundary.
	*/
	if (keyb.leftCtrlPressed && keyb.leftShiftPressed &&
		polyMatrixIsPatternActive(patternNum))
	{
		polyMatrixRequestQHandoff(patternNum);
		patternLauncherDrawPanel();
		return true;
	}

	patternLauncherRequest(patternNum, keyb.leftCtrlPressed, keyb.leftShiftPressed);
	patternLauncherDrawPanel();
	return true;
}

void handlePatternLauncherPanelRefresh(void)
{
	static int16_t oldCurrent = -2;
	static int16_t oldQueue[4] = { -2, -2, -2, -2 };
	static uint8_t oldQueueCount = 0xFF;
	static uint8_t oldExitMode = 0xFF;
	static uint8_t oldPolyCount = 0xFF;

	if (!patternLauncherPanelShown || !ui.instrSwitcherShown)
		return;

	patternLauncherBreatheFrame++;
	if (patternLauncherBreatheFrame >= 120)
		patternLauncherBreatheFrame = 0;
	if (sampleLauncherDeckShown)
	{
		patternLauncherDrawPanel();
		return;
	}

	const int16_t current = patternLauncherGetCurrent();
	const uint8_t queueCount = patternLauncherGetQueueCount();
	const uint8_t exitMode = patternLauncherGetExitMode();
	const uint8_t polyCount = polyMatrixGetActiveCount();
	const bool usageChanged = rebuildPatternLauncherVisibleUsage();
	bool changed = current != oldCurrent || queueCount != oldQueueCount ||
		exitMode != oldExitMode || polyCount != oldPolyCount || usageChanged;
	for (uint8_t i = 0; i < 4; i++)
	{
		const int16_t queueItem = patternLauncherGetQueueItem(i);
		if (queueItem != oldQueue[i])
			changed = true;
		oldQueue[i] = queueItem;
	}

	if (changed || current >= 0)
	{
		oldCurrent = current;
		oldQueueCount = queueCount;
		oldExitMode = exitMode;
		oldPolyCount = polyCount;
		patternLauncherDrawPanel();
	}
}

void patternLauncherSetPanelShown(bool shown)
{
	patternLauncherPanelShown = shown;
	if (shown)
		sampleLauncherDeckShown = false;
	for (uint16_t i = 0; i < 16; i++)
		hidePushButton(PB_RANGE1 + i);

	for (uint16_t i = 0; i < 8; i++)
		pushButtons[PB_RANGE1 + i].caption = shown ? patternLauncherPageCaptions[i] : instrumentBankCaptions[i];
	pushButtons[PB_SWAP_BANK].caption = shown ? "Exit" : "Swap";
	pushButtons[PB_SWAP_BANK].caption2 = shown ? "Patt." : "Bank";

	for (uint16_t i = 0; i < 8; i++)
		hideTextBox(TB_INST1 + i);
	for (uint16_t i = 0; i < 5; i++)
		hideTextBox(TB_SAMP1 + i);

	hidePushButton(PB_SAMPLE_LIST_UP);
	hidePushButton(PB_SAMPLE_LIST_DOWN);
	hideScrollBar(SB_SAMPLE_LIST);

	if (shown)
	{
		rebuildPatternLauncherSongUsage();
		drawPatternLauncherBankColumn();
		for (uint16_t i = 0; i < 8; i++)
			showPushButton(PB_RANGE1 + i);
		showPushButton(PB_SWAP_BANK);
		drawPatternLauncherShell();
	}
	else
	{
		showInstrumentSwitcher();
	}
}

void patternLauncherToggleDeck(void)
{
	if (!patternLauncherPanelShown)
		return;

	sampleLauncherDeckShown ^= 1;
	for (uint16_t i = 0; i < 8; i++)
	{
		pushButtons[PB_RANGE1 + i].caption = sampleLauncherDeckShown
			? sampleLauncherPageCaptions[i] : patternLauncherPageCaptions[i];
	}
	pushButtons[PB_SWAP_BANK].caption2 = sampleLauncherDeckShown ? "Samp." : "Patt.";
	patternLauncherForceRedraw();
}
