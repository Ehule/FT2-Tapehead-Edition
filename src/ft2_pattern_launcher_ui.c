#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
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
#include "ft2_inst_ed.h"
#include "ft2_sample_ed.h"
#include "ft2_sample_loader.h"
#include "ft2_sample_matrix_editor.h"

static bool patternLauncherPanelShown;
static bool sampleLauncherDeckShown;
static bool patternLauncherStandaloneShown;
static bool standaloneRestorePatternEditor;
static bool standaloneRestoreInstEditor;
static bool standaloneRestoreSampleEditor;
static uint8_t patternLauncherPage;
static uint8_t patternLauncherBreatheFrame;
static bool patternLauncherSongPatterns[MAX_PATTERNS];
static bool patternLauncherVisiblePatterns[32];
static bool patternLauncherExposedPatterns[MAX_PATTERNS];
static bool patternLauncherExposureInitialized;
static uint8_t standaloneOrderPos;
static uint16_t standaloneStatusFrames;
static char standaloneStatusText[32];
static uint8_t standaloneButtonFlash;
static uint8_t standaloneButtonFlashFrames;
static bool standaloneSampleEditMode;
static bool standaloneSampleEditExitPending;
static uint8_t standaloneSampleEditSource;
static uint16_t standaloneSelectedSampleTile;
static uint8_t standaloneModuleInstrument = 1;
static uint8_t standaloneModuleSample;

enum
{
	SAMPLE_EDIT_SOURCE_DISK = 0,
	SAMPLE_EDIT_SOURCE_MODULE = 1
};

enum
{
	STANDALONE_FLASH_NONE = 0,
	STANDALONE_FLASH_ORDER_PREV,
	STANDALONE_FLASH_ORDER_NEXT,
	STANDALONE_FLASH_PLAY_SONG,
	STANDALONE_FLASH_PLAY_PATTERN,
	STANDALONE_FLASH_STOP_SONG,
	STANDALONE_FLASH_STOP_DECK,
	STANDALONE_FLASH_STOP_ALL,
	STANDALONE_FLASH_SAMPLE_EDIT_PRIMARY,
	STANDALONE_FLASH_SAMPLE_EDIT_SECONDARY,
	STANDALONE_FLASH_SAMPLE_EDIT_FILL_DIRECTORY,
	STANDALONE_FLASH_SAMPLE_EDIT_UNASSIGN,
	STANDALONE_FLASH_SAMPLE_EDIT_DELETE,
	STANDALONE_FLASH_SAMPLE_EDIT_CLEAR_BANK,
	STANDALONE_FLASH_SAMPLE_EDIT_DONE
};

static char *patternLauncherPageCaptions[8] = { "00-1F", "20-3F", "40-5F", "60-7F", "80-9F", "A0-BF", "C0-DF", "E0-FF" };
/*
** The full range captions are exactly as wide as the standalone bank buttons.
** Their shadows therefore spill across the button borders at large display
** scales. The full-screen launcher only needs each bank's starting pattern;
** these shorter captions remain unambiguous and leave the chrome visible.
*/
static char *standalonePatternBankCaptions[8] = { "00", "20", "40", "60", "80", "A0", "C0", "E0" };
static char *instrumentBankCaptions[8] = { "01-08", "09-10", "11-18", "19-20", "21-28", "29-30", "31-38", "39-40" };
static char *sampleLauncherPageCaptions[8] = { "00-1F", "20-3F", "40-5F", "60-7F", "80-9F", "A0-BF", "C0-DF", "E0-FF" };

enum
{
	STANDALONE_PATTERN_X = 12,
	STANDALONE_SAMPLE_X = 320,
	STANDALONE_GRID_Y = 46,
	STANDALONE_TILE_W = 70,
	STANDALONE_TILE_H = 32,
	STANDALONE_TILE_GAP = 4,
	STANDALONE_PAGE_Y = 337
};

static uint16_t getVisibleSampleTile(uint8_t localTile)
{
	return (uint16_t)((sampleLauncherGetBank() * SAMPLE_LAUNCHER_TILES_PER_BANK) +
		localTile);
}

static void setStandaloneStatus(const char *text)
{
	strncpy(standaloneStatusText, text, sizeof (standaloneStatusText) - 1);
	standaloneStatusText[sizeof (standaloneStatusText) - 1] = '\0';
	standaloneStatusFrames = 180;
}

static void flashStandaloneButton(uint8_t button)
{
	standaloneButtonFlash = button;
	standaloneButtonFlashFrames = 10;
}

static void setSampleEditMode(bool enabled)
{
	if (enabled == standaloneSampleEditMode)
		return;
	standaloneSampleEditMode = enabled;
	standaloneSampleEditExitPending = false;
	if (enabled)
	{
		standaloneSelectedSampleTile =
			(uint16_t)(sampleLauncherGetBank() * SAMPLE_LAUNCHER_TILES_PER_BANK);
		standaloneModuleInstrument = editor.curInstr > 0 ? editor.curInstr : 1;
		standaloneModuleSample = editor.curSmp < MAX_SMP_PER_INST ? editor.curSmp : 0;
		if (!sampleMatrixBrowserOpen())
			setStandaloneStatus("COULD NOT OPEN SAMPLE FOLDER");
	}
	else
	{
		sampleMatrixBrowserClose();
	}
}

bool patternLauncherPanelIsShown(void)
{
	return patternLauncherPanelShown && !ui.patternEditorOnly;
}

bool patternLauncherPanelIsSampleDeck(void)
{
	return patternLauncherPanelShown && sampleLauncherDeckShown && !ui.patternEditorOnly;
}

bool patternLauncherDeckIsSample(void)
{
	return sampleLauncherDeckShown;
}

bool patternLauncherStandaloneIsShown(void)
{
	return patternLauncherStandaloneShown;
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
	if (changed && patternLauncherStandaloneShown)
		patternLauncherDrawStandalone();
	else if (changed && patternLauncherPanelShown)
		patternLauncherDrawPanel();
}

void patternLauncherNotifyPatternChanged(uint16_t patternNum)
{
	/* Keep both halves of the color classification current at the mutation
	** boundary. This avoids a newly populated song pattern being drawn with
	** the Matrix-only color while waiting for separate order/usage polling. */
	rebuildPatternLauncherSongUsage();
	rebuildPatternLauncherVisibleUsage();

	if (patternNum / 32 == patternLauncherPage)
	{
		if (patternLauncherStandaloneShown)
			patternLauncherDrawStandalone();
		else if (patternLauncherPanelShown)
			patternLauncherDrawPanel();
	}
}

static void drawSampleLauncherPanel(void)
{
	const int16_t current = sampleLauncherGetQCurrent();
	static const uint8_t queueLevels[4] = { 255, 210, 170, 135 };
	clearRect(421, 3, 166, 152);

	for (int16_t row = 0; row < 8; row++)
	{
		for (int16_t col = 0; col < 4; col++)
		{
			const uint8_t localTile = (uint8_t)((row * 4) + col);
			const uint16_t tile = getVisibleSampleTile(localTile);
			const int16_t x = 423 + (col * 41);
			const int16_t y = 4 + (row * 19);
			const bool loaded = sampleLauncherTileIsLoaded(tile);
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
	if (ui.patternEditorOnly)
		return;

	if (patternLauncherStandaloneShown)
	{
		patternLauncherDrawStandalone();
		return;
	}

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
		const uint16_t tile = getVisibleSampleTile((uint8_t)((row * 4) + col));
		if (sampleLauncherTileIsLoaded(tile) && !sampleLauncherTogglePoly(tile))
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
	if (ui.patternEditorOnly || !patternLauncherPanelShown)
		return;
	if (patternLauncherStandaloneShown)
	{
		patternLauncherDrawStandalone();
		return;
	}

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
		sampleLauncherSetBank(page & 7);
	else
		patternLauncherPage = page & 7;
	if (patternLauncherPanelShown)
		patternLauncherDrawPanel();
}

uint8_t patternLauncherGetPage(void)
{
	return patternLauncherPage;
}

void patternLauncherSetDeckMode(bool sampleDeck)
{
	if (sampleLauncherDeckShown == sampleDeck)
		return;

	sampleLauncherDeckShown = sampleDeck;
	if (patternLauncherPanelShown)
		patternLauncherDrawPanel();
}

bool patternLauncherPatternIsExposed(uint8_t patternNum)
{
	if (!patternLauncherExposureInitialized)
		patternLauncherResetExposure();
	return patternLauncherExposedPatterns[patternNum];
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
		const uint16_t tile = getVisibleSampleTile((uint8_t)((row * 4) + col));
		if (keyb.leftCtrlPressed && keyb.leftAltPressed &&
			!mouse.rightButtonPressed)
		{
			sampleLauncherHardStop(tile);
			patternLauncherDrawPanel();
			return true;
		}
		if (!sampleLauncherTileIsLoaded(tile))
			return true;

		if (keyb.leftAltPressed)
			sampleLauncherCycleTileBus(tile, audio.monoOutputMode
				? (uint8_t)(audio.outputBusCount * 2) : audio.outputBusCount);
		else if (!mouse.rightButtonPressed)
			sampleLauncherRequestQ(tile);

		patternLauncherDrawPanel();
		return true;
	}

	if (keyb.leftCtrlPressed && keyb.leftAltPressed &&
		!mouse.rightButtonPressed)
	{
		patternLauncherHardStop(patternNum);
		if (polyMatrixIsPatternActive(patternNum))
			polyMatrixTogglePattern(patternNum, true);
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

static void drawStandaloneButton(int16_t x, int16_t y, int16_t w,
	int16_t h, const char *caption, bool selected)
{
	drawFramework(x, y, w, h, selected ? FRAMEWORK_TYPE2 : FRAMEWORK_TYPE1);
	if (selected)
		fillRect(x + 2, y + 2, w - 4, h - 4, PAL_BUTTONS);

	const uint16_t captionWidth = textWidth(caption);
	const int16_t textX = x + ((w - captionWidth) / 2);
	const int16_t textY = y + ((h - FONT1_CHAR_H) / 2);
	textOutShadow(textX, textY, PAL_FORGRND, PAL_DSKTOP2, caption);
}

static void makeStandaloneSampleName(uint16_t tile, char name[9])
{
	const char *source = sampleLauncherGetTileName(tile);
	uint8_t writePos = 0;
	for (uint16_t readPos = 0; source[readPos] != '\0' && writePos < 8;
		readPos++)
	{
		if (isalnum((unsigned char)source[readPos]))
			name[writePos++] = (char)toupper((unsigned char)source[readPos]);
	}
	name[writePos] = '\0';
}

static void drawStandalonePatternTile(uint8_t patternNum, int16_t x, int16_t y)
{
	static const uint8_t queueLevels[4] = { 255, 210, 170, 135 };
	const int8_t queuePos = getPatternLauncherQueuePos(patternNum);
	const int16_t current = patternLauncherGetCurrent();
	const uint8_t exitMode = patternLauncherGetExitMode();
	const bool patternUsed = pattern[patternNum] != NULL;
	const bool exposed = patternLauncherExposedPatterns[patternNum];
	const bool songPattern = patternLauncherSongPatterns[patternNum];
	const bool polyActive = polyMatrixIsPatternActive(patternNum);
	const uint8_t polySlot = polyMatrixGetPatternSlot(patternNum);
	const bool qToPolyPending = patternLauncherPolyHandoffIsPending(patternNum);
	const uint32_t polyColor = polyMatrixPatternStopPending(patternNum)
		? 0xFF287A83
		: (polyMatrixPatternQHandoffPending(patternNum)
			? 0xFF2F9D87 : 0xFF35C9D0);

	drawFramework(x, y, STANDALONE_TILE_W, STANDALONE_TILE_H,
		FRAMEWORK_TYPE1);
	if (!exposed)
	{
		fillPatternLauncherRect(x + 2, y + 2, STANDALONE_TILE_W - 4,
			STANDALONE_TILE_H - 4,
			blendPatternLauncherColor(video.palette[PAL_DSKTOP2],
				video.palette[PAL_DESKTOP], 45));
	}
	else if (patternUsed)
	{
		const uint8_t tint = songPattern ? 100 : 55;
		fillPatternLauncherRect(x + 2, y + 2, STANDALONE_TILE_W - 4,
			STANDALONE_TILE_H - 4,
			blendPatternLauncherColor(video.palette[PAL_BUTTONS],
				video.palette[PAL_DESKTOP], tint));
	}
	if (patternNum == current)
	{
		uint32_t activeColor;
		if (exitMode == PATTERN_LAUNCHER_EXIT_RETURN) activeColor = 0xFFFFD43B;
		else if (exitMode == PATTERN_LAUNCHER_EXIT_STOP) activeColor = 0xFFE34234;
		else if (exitMode == PATTERN_LAUNCHER_EXIT_CONTINUE) activeColor = 0xFFFF8A2B;
		else activeColor = blendPatternLauncherColor(0xFF39C85A,
			video.palette[PAL_DESKTOP], getPatternLauncherBreatheLevel());
		fillPatternLauncherRect(x + 2, y + 2, STANDALONE_TILE_W - 4,
			STANDALONE_TILE_H - 4, activeColor);
		if (polyActive || qToPolyPending)
			fillPatternLauncherRect(x + 2, y + STANDALONE_TILE_H - 6,
				STANDALONE_TILE_W - 4, 4, polyColor);
	}
	else if (queuePos >= 0)
	{
		fillPatternLauncherRect(x + 2, y + 2, STANDALONE_TILE_W - 4,
			STANDALONE_TILE_H - 4,
			blendPatternLauncherColor(video.palette[PAL_BUTTONS],
				video.palette[PAL_DESKTOP], queueLevels[queuePos]));
	}
	else if (polyActive)
	{
		fillPatternLauncherRect(x + 2, y + 2, STANDALONE_TILE_W - 4,
			STANDALONE_TILE_H - 4, polyColor);
	}

	if (!exposed)
	{
		textOutTinyOutline(x + 6, y + 12, "X");
		hexOutPatternLauncherColor(x + 28, y + 11,
			blendPatternLauncherColor(video.palette[PAL_DSKTOP2],
				video.palette[PAL_BUTTONS], 90), patternNum, 2);
	}
	else
	{
		const uint8_t textPal = patternUsed
			? (songPattern ? PAL_PATTEXT : PAL_MOUSEPT) : PAL_DSKTOP2;
		hexOut(x + 28, y + 11, textPal, patternNum, 2);
	}

	if (polySlot != 0)
	{
		char slotText[2] = { (char)('0' + polySlot), '\0' };
		textOutTinyOutline(x + 60, y + 12, slotText);
	}
	else if (queuePos >= 0)
	{
		char queueText[2] = { (char)('1' + queuePos), '\0' };
		textOutTinyOutline(x + 60, y + 12, queueText);
	}
}

static void drawStandaloneSampleTile(uint16_t tile, int16_t x, int16_t y)
{
	static const uint8_t queueLevels[4] = { 255, 210, 170, 135 };
	const bool loaded = sampleLauncherTileIsLoaded(tile);
	const int16_t current = sampleLauncherGetQCurrent();
	const int8_t queuePos = sampleLauncherGetQQueuePos(tile);
	const int8_t polySlot = sampleLauncherGetPolySlot(tile);
	const bool polyStart = sampleLauncherPolyStartPending(tile);
	const bool polyStop = sampleLauncherPolyStopPending(tile);

	drawFramework(x, y, STANDALONE_TILE_W, STANDALONE_TILE_H,
		FRAMEWORK_TYPE1);
	if (loaded && tile == current)
	{
		const uint32_t color = sampleLauncherQStopPending()
			? 0xFFE34234
			: blendPatternLauncherColor(0xFF39C85A,
				video.palette[PAL_DESKTOP], getPatternLauncherBreatheLevel());
		fillPatternLauncherRect(x + 2, y + 2, STANDALONE_TILE_W - 4,
			STANDALONE_TILE_H - 4, color);
		if (polySlot >= 0 || polyStart)
			fillPatternLauncherRect(x + 2, y + STANDALONE_TILE_H - 6,
				STANDALONE_TILE_W - 4, 4,
				polyStop ? 0xFF287A83 : 0xFF35C9D0);
	}
	else if (queuePos >= 0)
	{
		fillPatternLauncherRect(x + 2, y + 2, STANDALONE_TILE_W - 4,
			STANDALONE_TILE_H - 4,
			blendPatternLauncherColor(video.palette[PAL_BUTTONS],
				video.palette[PAL_DESKTOP], queueLevels[queuePos]));
	}
	else if (polySlot >= 0 || polyStart)
	{
		fillPatternLauncherRect(x + 2, y + 2, STANDALONE_TILE_W - 4,
			STANDALONE_TILE_H - 4,
			polyStop ? 0xFF287A83 : (polyStart ? 0xFF2F9D87 : 0xFF35C9D0));
	}

	if (loaded)
	{
		char shortName[9];
		makeStandaloneSampleName(tile, shortName);
		char busText[2] = { (char)('A' + sampleLauncherGetTileBus(tile)), '\0' };
		textOutTinyOutline(x + 6, y + 12, busText);
		if (shortName[0] != '\0')
			textOutTinyOutline(x + 17, y + 12, shortName);
		else
			hexOut(x + 28, y + 11, PAL_PATTEXT, tile, 2);
	}
	else
	{
		hexOut(x + 28, y + 11, PAL_DSKTOP2, tile, 2);
	}

	if (polySlot >= 0)
	{
		char slotText[2] = { (char)('1' + polySlot), '\0' };
		textOutTinyOutline(x + 60, y + 12, slotText);
	}
	else if (queuePos >= 0)
	{
		char queueText[2] = { (char)('1' + queuePos), '\0' };
		textOutTinyOutline(x + 60, y + 12, queueText);
	}
}

static void drawStandaloneSampleEditTile(uint16_t tile, int16_t x, int16_t y)
{
	drawStandaloneSampleTile(tile, x, y);
	if (tile == standaloneSelectedSampleTile)
	{
		const uint32_t color = video.palette[PAL_MOUSEPT];
		fillPatternLauncherRect(x + 1, y + 1, STANDALONE_TILE_W - 2, 2, color);
		fillPatternLauncherRect(x + 1, y + STANDALONE_TILE_H - 3,
			STANDALONE_TILE_W - 2, 2, color);
		fillPatternLauncherRect(x + 1, y + 1, 2, STANDALONE_TILE_H - 2, color);
		fillPatternLauncherRect(x + STANDALONE_TILE_W - 3, y + 1, 2,
			STANDALONE_TILE_H - 2, color);
	}
}

static void drawSampleEditSource(void)
{
	drawStandaloneButton(12, 44, 62, 20, "DISK",
		standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_DISK);
	drawStandaloneButton(78, 44, 72, 20, "MODULE",
		standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_MODULE);

	if (standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_DISK)
	{
		drawStandaloneButton(154, 44, 42, 20, "UP", false);
		drawStandaloneButton(200, 44, 52, 20, "UPDATE", false);
		const char *path = sampleMatrixBrowserGetDisplayPath();
		textOutClipX(12, 69, PAL_FORGRND, path, 302);
		drawFramework(12, 81, 264, 242, FRAMEWORK_TYPE1);
		drawStandaloneButton(280, 81, 24, 20, "^", false);
		drawStandaloneButton(280, 105, 24, 20, "v", false);
		const uint32_t scroll = sampleMatrixBrowserGetScroll();
		for (uint32_t row = 0; row < SAMPLE_MATRIX_BROWSER_VISIBLE_ROWS; row++)
		{
			const uint32_t index = scroll + row;
			if (index >= sampleMatrixBrowserGetCount())
				break;
			const int16_t y = 86 + (int16_t)(row * 13);
			if (sampleMatrixBrowserEntryIsSelected(index))
				fillPatternLauncherRect(15, y - 1, 257, 11,
					video.palette[PAL_PATTEXT]);
			if (sampleMatrixBrowserEntryIsDirectory(index))
				textOutTiny(17, y, ">", video.palette[PAL_FORGRND]);
			textOutClipX(sampleMatrixBrowserEntryIsDirectory(index) ? 27 : 17,
				y, sampleMatrixBrowserEntryIsSelected(index) ? PAL_BLCKTXT :
				PAL_FORGRND, sampleMatrixBrowserGetName(index), 270);
		}
		char selectionText[32];
		snprintf(selectionText, sizeof (selectionText), "%u FILE%s SELECTED",
			sampleMatrixBrowserGetSelectionCount(),
			sampleMatrixBrowserGetSelectionCount() == 1 ? "" : "S");
		textOutTinyOutline(120, 31, selectionText);
	}
	else
	{
		drawStandaloneButton(154, 44, 28, 20, "<", false);
		drawStandaloneButton(276, 44, 28, 20, ">", false);
		char instrumentText[48];
		snprintf(instrumentText, sizeof (instrumentText), "I%02X %.22s",
			standaloneModuleInstrument,
			song.instrName[standaloneModuleInstrument]);
		textOutClipX(12, 70, PAL_FORGRND, instrumentText, 302);
		drawFramework(12, 81, 292, 242, FRAMEWORK_TYPE1);
		for (uint8_t sample = 0; sample < MAX_SMP_PER_INST; sample++)
		{
			const int16_t y = 87 + (sample * 14);
			if (sample == standaloneModuleSample)
				fillPatternLauncherRect(15, y - 2, 285, 12,
					video.palette[PAL_PATTEXT]);
			char sampleText[40];
			const sample_t *nativeSample = instr[standaloneModuleInstrument] != NULL
				? &instr[standaloneModuleInstrument]->smp[sample] : NULL;
			snprintf(sampleText, sizeof (sampleText), "%X  %s", sample,
				nativeSample != NULL && nativeSample->dataPtr != NULL
					? nativeSample->name : "-- empty --");
			textOutClipX(18, y,
				sample == standaloneModuleSample ? PAL_BLCKTXT : PAL_FORGRND,
				sampleText, 300);
		}
	}
}

static void drawSampleMatrixEditor(void)
{
	clearRect(0, 0, SCREEN_W, SCREEN_H);
	drawFramework(0, 0, SCREEN_W, SCREEN_H, FRAMEWORK_TYPE1);
	textOutShadow(12, 9, PAL_FORGRND, PAL_DSKTOP2, "SAMPLE MATRIX EDITOR");
	drawStandaloneButton(550, 6, 66, 20, "DONE", false);
	textOutShadow(12, 29, PAL_PATTEXT, PAL_DSKTOP2, "SOURCE");
	textOutShadow(320, 29, PAL_PATTEXT, PAL_DSKTOP2, "SAMPLES");
	drawFramework(8, 40, 300, 292, FRAMEWORK_TYPE1);
	drawFramework(316, 40, 308, 292, FRAMEWORK_TYPE1);
	drawSampleEditSource();

	for (int16_t row = 0; row < 8; row++)
	{
		for (int16_t col = 0; col < 4; col++)
		{
			const uint8_t localTile = (uint8_t)((row * 4) + col);
			const int16_t y = STANDALONE_GRID_Y +
				(row * (STANDALONE_TILE_H + STANDALONE_TILE_GAP));
			const int16_t x = STANDALONE_SAMPLE_X +
				(col * (STANDALONE_TILE_W + STANDALONE_TILE_GAP));
			drawStandaloneSampleEditTile(getVisibleSampleTile(localTile), x, y);
		}
	}
	for (uint8_t page = 0; page < 8; page++)
	{
		const int16_t x = 320 + (page * 37);
		drawStandaloneButton(x, STANDALONE_PAGE_Y, 34, 20,
			standalonePatternBankCaptions[page], page == sampleLauncherGetBank());
	}

	drawFramework(8, 363, 616, 29, FRAMEWORK_TYPE1);
	drawStandaloneButton(12, 367, 82, 20,
		standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_DISK ? "IMPORT ONE" : "ASSIGN",
		standaloneButtonFlash == STANDALONE_FLASH_SAMPLE_EDIT_PRIMARY);
	drawStandaloneButton(98, 367, 76, 20,
		standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_DISK ? "FILL SEL" : "ASSIGN ALL",
		standaloneButtonFlash == STANDALONE_FLASH_SAMPLE_EDIT_SECONDARY);
	if (standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_DISK)
		drawStandaloneButton(178, 367, 72, 20, "FILL DIR",
			standaloneButtonFlash == STANDALONE_FLASH_SAMPLE_EDIT_FILL_DIRECTORY);
	drawStandaloneButton(254, 367, 76, 20, "UNASSIGN",
		standaloneButtonFlash == STANDALONE_FLASH_SAMPLE_EDIT_UNASSIGN);
	drawStandaloneButton(334, 367, 68, 20, "DELETE",
		standaloneButtonFlash == STANDALONE_FLASH_SAMPLE_EDIT_DELETE);
	drawStandaloneButton(406, 367, 76, 20, "CLEAR BNK",
		standaloneButtonFlash == STANDALONE_FLASH_SAMPLE_EDIT_CLEAR_BANK);
	drawStandaloneButton(538, 367, 78, 20, "DONE",
		standaloneButtonFlash == STANDALONE_FLASH_SAMPLE_EDIT_DONE);
	if (standaloneStatusFrames > 0)
		textOutTiny(180, 12, standaloneStatusText, 0xFFE34234);
}

void patternLauncherDrawStandalone(void)
{
	if (!patternLauncherStandaloneShown)
		return;
	if (standaloneSampleEditMode)
	{
		drawSampleMatrixEditor();
		return;
	}

	rebuildPatternLauncherSongUsage();
	rebuildPatternLauncherVisibleUsage();
	clearRect(0, 0, SCREEN_W, SCREEN_H);
	drawFramework(0, 0, SCREEN_W, SCREEN_H, FRAMEWORK_TYPE1);

	textOutShadow(12, 9, PAL_FORGRND, PAL_DSKTOP2, "DECK MATRIX");
	char timingText[32];
	snprintf(timingText, sizeof (timingText), "BPM %03u TPL %02u",
		song.BPM, song.speed);
	textOutShadow(122, 9, PAL_FORGRND, PAL_DSKTOP2, timingText);

	char patternStatus[24], sampleStatus[24];
	const int16_t patternQ = patternLauncherGetCurrent();
	const int16_t sampleQ = sampleLauncherGetQCurrent();
	char patternQText[3] = "--", sampleQText[3] = "--";
	if (patternQ >= 0)
		snprintf(patternQText, sizeof (patternQText), "%02X",
			(unsigned int)(uint8_t)patternQ);
	if (sampleQ >= 0)
		snprintf(sampleQText, sizeof (sampleQText), "%02X",
			(unsigned int)(uint8_t)sampleQ);
	snprintf(patternStatus, sizeof (patternStatus), "PAT Q:%s P:%u/4",
		patternQText, polyMatrixGetActiveCount());
	snprintf(sampleStatus, sizeof (sampleStatus), "SMP Q:%s P:%u/4",
		sampleQText, sampleLauncherGetPolyCount());
	textOutTinyOutline(330, 6, patternStatus);
	textOutTinyOutline(330, 18, sampleStatus);
	if (standaloneStatusFrames > 0)
		textOutTiny(438, 31, standaloneStatusText, 0xFFE34234);

	textOutShadow(12, 29, PAL_PATTEXT, PAL_DSKTOP2, "PATTERNS");
	textOutShadow(320, 29, PAL_PATTEXT, PAL_DSKTOP2, "SAMPLES");
	drawStandaloneButton(538, 6, 78, 20, "EDIT SMP", false);

	drawFramework(8, 40, 300, 292, FRAMEWORK_TYPE1);
	drawFramework(316, 40, 308, 292, FRAMEWORK_TYPE1);
	for (int16_t row = 0; row < 8; row++)
	{
		for (int16_t col = 0; col < 4; col++)
		{
			const uint8_t tile = (uint8_t)((row * 4) + col);
			const int16_t y = STANDALONE_GRID_Y +
				(row * (STANDALONE_TILE_H + STANDALONE_TILE_GAP));
			const int16_t patternX = STANDALONE_PATTERN_X +
				(col * (STANDALONE_TILE_W + STANDALONE_TILE_GAP));
			const int16_t sampleX = STANDALONE_SAMPLE_X +
				(col * (STANDALONE_TILE_W + STANDALONE_TILE_GAP));
			const uint8_t patternNum = (uint8_t)((patternLauncherPage * 32) + tile);
			drawStandalonePatternTile(patternNum, patternX, y);
			drawStandaloneSampleTile(getVisibleSampleTile(tile), sampleX, y);
		}
	}

	for (uint8_t page = 0; page < 8; page++)
	{
		const int16_t x = 12 + (page * 37);
		drawStandaloneButton(x, STANDALONE_PAGE_Y, 34, 20,
			standalonePatternBankCaptions[page], page == patternLauncherPage);
		const int16_t sampleX = 320 + (page * 37);
		drawStandaloneButton(sampleX, STANDALONE_PAGE_Y, 34, 20,
			standalonePatternBankCaptions[page], page == sampleLauncherGetBank());
	}

	if (songPlaying && !patternLauncherIsEnabled() &&
		(playMode == PLAYMODE_SONG || playMode == PLAYMODE_RECSONG))
	{
		standaloneOrderPos = (uint8_t)CLAMP(editor.songPos, 0,
			song.songLength > 0 ? song.songLength - 1 : 0);
	}

	drawFramework(8, 363, 616, 29, FRAMEWORK_TYPE1);
	drawStandaloneButton(12, 367, 34, 20, "<",
		standaloneButtonFlash == STANDALONE_FLASH_ORDER_PREV);
	char orderText[20];
	const uint8_t selectedPattern = song.orders[standaloneOrderPos];
	snprintf(orderText, sizeof (orderText), "O%02X P%02X",
		standaloneOrderPos, selectedPattern);
	drawStandaloneButton(50, 367, 78, 20, orderText, true);
	drawStandaloneButton(132, 367, 34, 20, ">",
		standaloneButtonFlash == STANDALONE_FLASH_ORDER_NEXT);
	drawStandaloneButton(170, 367, 68, 20, "PLAY SNG",
		standaloneButtonFlash == STANDALONE_FLASH_PLAY_SONG);
	drawStandaloneButton(242, 367, 68, 20, "PLAY PAT",
		standaloneButtonFlash == STANDALONE_FLASH_PLAY_PATTERN);
	drawStandaloneButton(314, 367, 68, 20, "STOP SNG",
		standaloneButtonFlash == STANDALONE_FLASH_STOP_SONG);
	drawStandaloneButton(386, 367, 76, 20, "STOP DECK",
		standaloneButtonFlash == STANDALONE_FLASH_STOP_DECK);
	drawStandaloneButton(466, 367, 68, 20, "STOP ALL",
		standaloneButtonFlash == STANDALONE_FLASH_STOP_ALL);
	drawStandaloneButton(538, 367, 78, 20, "TRACKER", false);
}

static bool standaloneGridTileAt(int16_t x, int16_t y, int16_t gridX,
	uint8_t *tile)
{
	if (x < gridX || y < STANDALONE_GRID_Y)
		return false;

	const int16_t strideX = STANDALONE_TILE_W + STANDALONE_TILE_GAP;
	const int16_t strideY = STANDALONE_TILE_H + STANDALONE_TILE_GAP;
	const int16_t col = (x - gridX) / strideX;
	const int16_t row = (y - STANDALONE_GRID_Y) / strideY;
	if (col < 0 || col > 3 || row < 0 || row > 7)
		return false;
	if (((x - gridX) % strideX) >= STANDALONE_TILE_W ||
		((y - STANDALONE_GRID_Y) % strideY) >= STANDALONE_TILE_H)
		return false;

	*tile = (uint8_t)((row * 4) + col);
	return true;
}

static void handleStandalonePatternTile(uint8_t tile, uint8_t mouseButton,
	bool shiftPressed)
{
	const uint8_t patternNum = (uint8_t)((patternLauncherPage * 32) + tile);
	if (mouseButton == SDL_BUTTON_LEFT && keyb.leftCtrlPressed &&
		keyb.leftAltPressed)
	{
		patternLauncherHardStop(patternNum);
		if (polyMatrixIsPatternActive(patternNum))
			polyMatrixTogglePattern(patternNum, true);
		return;
	}
	if (mouseButton == SDL_BUTTON_LEFT && keyb.leftAltPressed)
	{
		patternLauncherExposedPatterns[patternNum] ^= 1;
		return;
	}
	if (!patternLauncherExposedPatterns[patternNum])
		return;

	if (mouseButton == SDL_BUTTON_MIDDLE)
	{
		if (patternLauncherGetCurrent() == patternNum &&
			!polyMatrixIsPatternActive(patternNum))
		{
			patternLauncherRequestPolyHandoff(patternNum);
		}
		else if (!polyMatrixTogglePattern(patternNum, shiftPressed))
		{
			setStandaloneStatus("NO FREE PATTERN LANES");
		}
		return;
	}

	if (mouseButton == SDL_BUTTON_RIGHT)
	{
		if (shiftPressed)
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
		return;
	}

	if (mouseButton != SDL_BUTTON_LEFT)
		return;
	if (keyb.leftCtrlPressed && keyb.leftShiftPressed &&
		polyMatrixIsPatternActive(patternNum))
	{
		polyMatrixRequestQHandoff(patternNum);
	}
	else
	{
		patternLauncherRequest(patternNum, keyb.leftCtrlPressed,
			keyb.leftShiftPressed);
	}
}

static void openStandaloneSampleInEditor(uint16_t tile)
{
	if (!sampleLauncherSelectTileInEditor(tile))
		return;

	patternLauncherSetStandaloneShown(false);
	const bool needsUpperBank = editor.curInstr > 0x40;
	if (needsUpperBank != editor.instrBankSwapped)
		pbSwapInstrBank();
	editor.instrBankOffset = ((editor.curInstr - 1) / 8) * 8;
	editor.sampleBankOffset = (editor.curSmp / 5) * 5;
	if (editor.sampleBankOffset > MAX_SMP_PER_INST - 5)
		editor.sampleBankOffset = MAX_SMP_PER_INST - 5;
	updateNewInstrument();
	showSampleEditor();
}

static void handleStandaloneSampleTile(uint8_t localTile, uint8_t mouseButton,
	bool shiftPressed)
{
	const uint16_t tile = getVisibleSampleTile(localTile);
	if (mouseButton == SDL_BUTTON_LEFT && keyb.leftCtrlPressed &&
		keyb.leftAltPressed)
	{
		sampleLauncherHardStop(tile);
		return;
	}
	if (mouseButton == SDL_BUTTON_RIGHT && keyb.leftCtrlPressed)
	{
		const sample_t *selectedSample = getCurSample();
		if (selectedSample == NULL || selectedSample->dataPtr == NULL ||
			selectedSample->length <= 0)
		{
			setStandaloneStatus("SELECT A LOADED SAMPLE");
			return;
		}
		if (sampleLauncherTileIsLoaded(tile))
		{
			char message[96];
			snprintf(message, sizeof (message),
				"Replace Sample Deck tile %02X with selected sample?", tile);
			if (okBox(2, "Sample Matrix", message, NULL) != 1)
				return;
		}

		const sampleLauncherPlaceResult_t result =
			sampleLauncherCopySampleToTile(tile, editor.curInstr, editor.curSmp);
		if (result == SAMPLE_LAUNCHER_PLACE_OK)
		{
			char status[32];
			snprintf(status, sizeof (status), "SAMPLE COPIED TO %02X", tile);
			setStandaloneStatus(status);
		}
		else if (result == SAMPLE_LAUNCHER_PLACE_EMPTY_SOURCE)
			setStandaloneStatus("SELECT A LOADED SAMPLE");
		else if (result == SAMPLE_LAUNCHER_PLACE_SAME_SAMPLE)
			setStandaloneStatus("SAMPLE ALREADY ON TILE");
		else if (result == SAMPLE_LAUNCHER_PLACE_NO_INSTRUMENT)
			setStandaloneStatus("NO FREE INSTRUMENT SLOT");
		else
			setStandaloneStatus("NOT ENOUGH MEMORY");
		return;
	}
	if (!sampleLauncherTileIsLoaded(tile))
		return;
	if (mouseButton == SDL_BUTTON_LEFT && shiftPressed)
	{
		openStandaloneSampleInEditor(tile);
		return;
	}
	if (mouseButton == SDL_BUTTON_MIDDLE)
	{
		if (!sampleLauncherTogglePoly(tile))
			setStandaloneStatus("NO FREE SAMPLE LANES");
	}
	else if (mouseButton == SDL_BUTTON_LEFT && keyb.leftAltPressed)
	{
		sampleLauncherCycleTileBus(tile, audio.monoOutputMode
			? (uint8_t)(audio.outputBusCount * 2) : audio.outputBusCount);
	}
	else if (mouseButton == SDL_BUTTON_LEFT)
	{
		if (!sampleLauncherRequestQ(tile))
			setStandaloneStatus("SAMPLE QUEUE FULL");
	}
}

static bool beginDiskMatrixImport(bool selectedOnly, bool exactTile)
{
	uint32_t count = selectedOnly ? sampleMatrixBrowserGetSelectionCount() :
		sampleMatrixBrowserGetFileCount();
	if (count == 0)
	{
		setStandaloneStatus(selectedOnly ? "SELECT ONE OR MORE FILES" :
			"NO SAMPLE FILES IN FOLDER");
		return false;
	}
	if (exactTile)
		count = 1;

	const UNICHAR **names = malloc(count * sizeof (*names));
	if (names == NULL)
	{
		setStandaloneStatus("NOT ENOUGH MEMORY");
		return false;
	}
	for (uint32_t i = 0; i < count; i++)
		names[i] = selectedOnly ? sampleMatrixBrowserGetSelectedName(i) :
			sampleMatrixBrowserGetFileName(i);

	if (exactTile && (sampleLauncherTileIsLoaded(standaloneSelectedSampleTile) ||
		sampleLauncherTileNaturalStorageIsLoaded(standaloneSelectedSampleTile)))
	{
		char message[96];
		snprintf(message, sizeof (message),
			"Replace Sample Matrix tile %02X with the selected disk sample?",
			standaloneSelectedSampleTile);
		if (okBox(2, "Sample Matrix Editor", message, NULL) != 1)
		{
			free(names);
			return false;
		}
	}

	const bool started = loadSamplesToMatrix(sampleMatrixBrowserGetPath(), names,
		count, standaloneSelectedSampleTile, exactTile);
	free(names);
	setStandaloneStatus(started ? "IMPORTING SAMPLES..." :
		"IMPORT COULD NOT START");
	return started;
}

static void assignSelectedModuleSample(void)
{
	if (instr[standaloneModuleInstrument] == NULL ||
		instr[standaloneModuleInstrument]->smp[standaloneModuleSample].dataPtr == NULL)
	{
		setStandaloneStatus("SELECT A LOADED MODULE SAMPLE");
		return;
	}
	if (sampleLauncherTileIsLoaded(standaloneSelectedSampleTile))
	{
		char message[96];
		snprintf(message, sizeof (message),
			"Replace Sample Matrix assignment %02X? The sample is not copied.",
			standaloneSelectedSampleTile);
		if (okBox(2, "Sample Matrix Editor", message, NULL) != 1)
			return;
	}
	if (sampleLauncherAssignTile(standaloneSelectedSampleTile,
		standaloneModuleInstrument, standaloneModuleSample))
	{
		setStandaloneStatus("MODULE SAMPLE ASSIGNED");
	}
	else
	{
		setStandaloneStatus("ASSIGNMENT FAILED");
	}
}

static void assignAllModuleSamples(void)
{
	if (instr[standaloneModuleInstrument] == NULL)
	{
		setStandaloneStatus("SELECT A LOADED INSTRUMENT");
		return;
	}
	uint16_t searchTile = standaloneSelectedSampleTile;
	uint32_t assigned = 0, available = 0;
	for (uint8_t sample = 0; sample < MAX_SMP_PER_INST; sample++)
	{
		if (instr[standaloneModuleInstrument]->smp[sample].dataPtr == NULL ||
			instr[standaloneModuleInstrument]->smp[sample].length <= 0)
		{
			continue;
		}
		available++;
		while (searchTile < SAMPLE_LAUNCHER_MAX_TILES &&
			sampleLauncherTileIsLoaded(searchTile))
		{
			searchTile++;
		}
		if (searchTile >= SAMPLE_LAUNCHER_MAX_TILES)
			break;
		if (sampleLauncherAssignTile(searchTile, standaloneModuleInstrument, sample))
			assigned++;
		searchTile++;
	}
	char status[32];
	snprintf(status, sizeof (status), "%u OF %u SAMPLES ASSIGNED",
		assigned, available);
	setStandaloneStatus(status);
}

static bool handleSampleMatrixEditorClick(int16_t x, int16_t y,
	uint8_t mouseButton)
{
	if (!standaloneSampleEditMode)
		return false;
	if (mouseButton != SDL_BUTTON_LEFT)
		return true;
	if (standaloneSampleEditExitPending)
		return true;

	uint8_t localTile;
	if (standaloneGridTileAt(x, y, STANDALONE_SAMPLE_X, &localTile))
	{
		standaloneSelectedSampleTile = getVisibleSampleTile(localTile);
		return true;
	}
	if (x >= 550 && x < 616 && y >= 6 && y < 26)
	{
		setSampleEditMode(false);
		return true;
	}
	if (x >= 538 && x < 616 && y >= 367 && y < 387)
	{
		flashStandaloneButton(STANDALONE_FLASH_SAMPLE_EDIT_DONE);
		standaloneSampleEditExitPending = true;
		return true;
	}
	if (x >= 12 && x < 74 && y >= 44 && y < 64)
	{
		standaloneSampleEditSource = SAMPLE_EDIT_SOURCE_DISK;
		return true;
	}
	if (x >= 78 && x < 150 && y >= 44 && y < 64)
	{
		standaloneSampleEditSource = SAMPLE_EDIT_SOURCE_MODULE;
		return true;
	}

	if (standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_DISK)
	{
		if (x >= 154 && x < 196 && y >= 44 && y < 64)
			sampleMatrixBrowserGoParent();
		else if (x >= 200 && x < 252 && y >= 44 && y < 64)
			sampleMatrixBrowserRefresh();
		else if (x >= 280 && x < 304 && y >= 81 && y < 101)
			sampleMatrixBrowserScroll(-SAMPLE_MATRIX_BROWSER_VISIBLE_ROWS);
		else if (x >= 280 && x < 304 && y >= 105 && y < 125)
			sampleMatrixBrowserScroll(SAMPLE_MATRIX_BROWSER_VISIBLE_ROWS);
		else if (x >= 12 && x < 276 && y >= 86 && y < 307)
		{
			const int32_t row = (y - 86) / 13;
			if (row >= 0 && row < SAMPLE_MATRIX_BROWSER_VISIBLE_ROWS)
			{
				const uint32_t index = sampleMatrixBrowserGetScroll() + row;
				if (sampleMatrixBrowserEntryIsDirectory(index))
					sampleMatrixBrowserOpenDirectory(index);
				else
					sampleMatrixBrowserSelect(index, keyb.leftCtrlPressed,
						keyb.leftShiftPressed);
			}
		}
	}
	else
	{
		if (x >= 154 && x < 182 && y >= 44 && y < 64)
			standaloneModuleInstrument = standaloneModuleInstrument > 1
				? standaloneModuleInstrument - 1 : MAX_INST;
		else if (x >= 276 && x < 304 && y >= 44 && y < 64)
			standaloneModuleInstrument = standaloneModuleInstrument < MAX_INST
				? standaloneModuleInstrument + 1 : 1;
		else if (x >= 12 && x < 304 && y >= 85 && y < 309)
		{
			const int32_t sample = (y - 85) / 14;
			if (sample >= 0 && sample < MAX_SMP_PER_INST)
				standaloneModuleSample = (uint8_t)sample;
		}
	}

	if (y >= STANDALONE_PAGE_Y && y < STANDALONE_PAGE_Y + 20 &&
		x >= 320 && x < 616)
	{
		const int16_t page = (x - 320) / 37;
		if (page >= 0 && page < 8 && ((x - 320) % 37) < 34)
		{
			const uint8_t local = standaloneSelectedSampleTile %
				SAMPLE_LAUNCHER_TILES_PER_BANK;
			sampleLauncherSetBank((uint8_t)page);
			standaloneSelectedSampleTile =
				(uint16_t)(page * SAMPLE_LAUNCHER_TILES_PER_BANK + local);
		}
	}
	else if (y >= 367 && y < 387 && x >= 12 && x < 94)
	{
		flashStandaloneButton(STANDALONE_FLASH_SAMPLE_EDIT_PRIMARY);
		if (standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_DISK)
			beginDiskMatrixImport(true, true);
		else
			assignSelectedModuleSample();
	}
	else if (y >= 367 && y < 387 && x >= 98 && x < 174)
	{
		flashStandaloneButton(STANDALONE_FLASH_SAMPLE_EDIT_SECONDARY);
		if (standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_DISK)
			beginDiskMatrixImport(true, false);
		else
			assignAllModuleSamples();
	}
	else if (standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_DISK &&
		y >= 367 && y < 387 && x >= 178 && x < 250)
	{
		flashStandaloneButton(STANDALONE_FLASH_SAMPLE_EDIT_FILL_DIRECTORY);
		beginDiskMatrixImport(false, false);
	}
	else if (y >= 367 && y < 387 && x >= 254 && x < 330)
	{
		flashStandaloneButton(STANDALONE_FLASH_SAMPLE_EDIT_UNASSIGN);
		if (sampleLauncherUnassignTile(standaloneSelectedSampleTile))
			setStandaloneStatus("TILE UNASSIGNED - SAMPLE KEPT");
	}
	else if (y >= 367 && y < 387 && x >= 334 && x < 402)
	{
		flashStandaloneButton(STANDALONE_FLASH_SAMPLE_EDIT_DELETE);
		if (sampleLauncherTileIsLoaded(standaloneSelectedSampleTile))
		{
			char message[96];
			snprintf(message, sizeof (message),
				"Delete the native sample used by tile %02X? This affects every reference.",
				standaloneSelectedSampleTile);
			if (okBox(2, "Sample Matrix Editor", message, NULL) == 1 &&
				sampleLauncherDeleteTileSample(standaloneSelectedSampleTile))
			{
				setStandaloneStatus("NATIVE SAMPLE DELETED");
			}
		}
	}
	else if (y >= 367 && y < 387 && x >= 406 && x < 482)
	{
		flashStandaloneButton(STANDALONE_FLASH_SAMPLE_EDIT_CLEAR_BANK);
		if (okBox(2, "Sample Matrix Editor",
			"Detach all 32 tiles? Samples stay in FT2 and these tiles become reusable.",
			NULL) == 1)
		{
			sampleLauncherUnassignBank(sampleLauncherGetBank());
			setStandaloneStatus("BANK DETACHED - SAMPLES KEPT");
		}
	}
	return true;
}

bool patternLauncherHandleStandaloneWheel(int16_t x, int16_t y,
	bool directionUp)
{
	if (!patternLauncherStandaloneShown)
		return false;

	if (standaloneSampleEditMode &&
		standaloneSampleEditSource == SAMPLE_EDIT_SOURCE_DISK &&
		x >= 12 && x < 304 && y >= 81 && y < 323)
	{
		sampleMatrixBrowserScroll(directionUp ? -3 : 3);
		patternLauncherDrawStandalone();
	}

	/* Never let a wheel event over either full-window Matrix surface change
	** the hidden tracker editor underneath it. */
	return true;
}

bool patternLauncherHandleStandaloneClick(int16_t x, int16_t y,
	uint8_t mouseButton, bool shiftPressed)
{
	if (!patternLauncherStandaloneShown)
		return false;
	if (standaloneSampleEditMode)
	{
		handleSampleMatrixEditorClick(x, y, mouseButton);
		patternLauncherDrawStandalone();
		return true;
	}
	if (mouseButton == SDL_BUTTON_LEFT && x >= 538 && x < 616 &&
		y >= 6 && y < 26)
	{
		setSampleEditMode(true);
		patternLauncherDrawStandalone();
		return true;
	}

	uint8_t tile;
	if (standaloneGridTileAt(x, y, STANDALONE_PATTERN_X, &tile))
		handleStandalonePatternTile(tile, mouseButton, shiftPressed);
	else if (standaloneGridTileAt(x, y, STANDALONE_SAMPLE_X, &tile))
		handleStandaloneSampleTile(tile, mouseButton, shiftPressed);
	else if (mouseButton == SDL_BUTTON_LEFT && y >= STANDALONE_PAGE_Y &&
		y < STANDALONE_PAGE_Y + 20 && x >= 12 && x < 308)
	{
		const int16_t page = (x - 12) / 37;
		if (page >= 0 && page < 8 && ((x - 12) % 37) < 34)
			patternLauncherPage = (uint8_t)page;
	}
	else if (mouseButton == SDL_BUTTON_LEFT && y >= STANDALONE_PAGE_Y &&
		y < STANDALONE_PAGE_Y + 20 && x >= 320 && x < 616)
	{
		const int16_t page = (x - 320) / 37;
		if (page >= 0 && page < 8 && ((x - 320) % 37) < 34)
			sampleLauncherSetBank((uint8_t)page);
	}
	else if (mouseButton == SDL_BUTTON_LEFT && y >= 367 && y < 387 &&
		x >= 12 && x < 46)
	{
		flashStandaloneButton(STANDALONE_FLASH_ORDER_PREV);
		if (standaloneOrderPos > 0)
			standaloneOrderPos--;
	}
	else if (mouseButton == SDL_BUTTON_LEFT && y >= 367 && y < 387 &&
		x >= 132 && x < 166)
	{
		flashStandaloneButton(STANDALONE_FLASH_ORDER_NEXT);
		if (standaloneOrderPos + 1 < song.songLength)
			standaloneOrderPos++;
	}
	else if (mouseButton == SDL_BUTTON_LEFT && x >= 170 && x < 238 &&
		y >= 367 && y < 387)
	{
		flashStandaloneButton(STANDALONE_FLASH_PLAY_SONG);
		patternLauncherSetEnabled(false);
		editor.songPos = standaloneOrderPos;
		setNewSongPos(standaloneOrderPos);
		startPlaying(PLAYMODE_SONG, 0);
	}
	else if (mouseButton == SDL_BUTTON_LEFT && x >= 242 && x < 310 &&
		y >= 367 && y < 387)
	{
		flashStandaloneButton(STANDALONE_FLASH_PLAY_PATTERN);
		patternLauncherSetEnabled(false);
		editor.songPos = standaloneOrderPos;
		setNewSongPos(standaloneOrderPos);
		startPlaying(PLAYMODE_PATT, 0);
	}
	else if (mouseButton == SDL_BUTTON_LEFT && x >= 314 && x < 382 &&
		y >= 367 && y < 387)
	{
		flashStandaloneButton(STANDALONE_FLASH_STOP_SONG);
		if (songPlaying && !patternLauncherIsEnabled())
			stopPlayingKeepPoly();
	}
	else if (mouseButton == SDL_BUTTON_LEFT && x >= 386 && x < 462 &&
		y >= 367 && y < 387)
	{
		flashStandaloneButton(STANDALONE_FLASH_STOP_DECK);
		patternLauncherStopDeckQ();
		polyMatrixReset();
		sampleLauncherReset();
	}
	else if (mouseButton == SDL_BUTTON_LEFT && x >= 466 && x < 534 &&
		y >= 367 && y < 387)
	{
		flashStandaloneButton(STANDALONE_FLASH_STOP_ALL);
		stopPlaying();
		sampleLauncherReset();
	}
	else if (mouseButton == SDL_BUTTON_LEFT && x >= 538 && x < 616 &&
		y >= 367 && y < 387)
	{
		patternLauncherSetStandaloneShown(false);
		return true;
	}

	if (patternLauncherStandaloneShown)
		patternLauncherDrawStandalone();
	return true;
}

bool patternLauncherHandleStandaloneSpace(void)
{
	if (!patternLauncherStandaloneShown)
		return false;
	if (standaloneSampleEditMode)
		return true;
	if (songPlaying && !patternLauncherIsEnabled())
		stopPlayingKeepPoly();
	patternLauncherDrawStandalone();
	return true;
}

void patternLauncherSetStandaloneShown(bool shown)
{
	if (shown == patternLauncherStandaloneShown)
	{
		if (shown)
			patternLauncherDrawStandalone();
		return;
	}

	if (shown)
	{
		standaloneRestorePatternEditor = ui.patternEditorShown;
		standaloneRestoreInstEditor = ui.instEditorShown;
		standaloneRestoreSampleEditor = ui.sampleEditorShown;
		patternLauncherPanelShown = true;
		sampleLauncherDeckShown = false;
		patternLauncherStandaloneShown = true;
		standaloneOrderPos = (uint8_t)CLAMP(editor.songPos, 0,
			song.songLength > 0 ? song.songLength - 1 : 0);
		hideTopScreen();
		if (standaloneRestorePatternEditor) hidePatternEditor();
		if (standaloneRestoreInstEditor) hideInstEditor();
		if (standaloneRestoreSampleEditor) hideSampleEditor();
		unstuckLastUsedGUIElement();
		patternLauncherDrawStandalone();
	}
	else
	{
		setSampleEditMode(false);
		patternLauncherStandaloneShown = false;
		showTopScreen(DONT_RESTORE_SCREENS);
		if (standaloneRestoreInstEditor)
			showInstEditor();
		else if (standaloneRestoreSampleEditor)
			showSampleEditor();
		else
			showPatternEditor();
		unstuckLastUsedGUIElement();
	}
}

void handlePatternLauncherPanelRefresh(void)
{
	static int16_t oldCurrent = -2;
	static int16_t oldQueue[4] = { -2, -2, -2, -2 };
	static uint8_t oldQueueCount = 0xFF;
	static uint8_t oldExitMode = 0xFF;
	static uint8_t oldPolyCount = 0xFF;

	if (patternLauncherStandaloneShown)
	{
		uint32_t added, requested, omitted;
		if (sampleMatrixImportTakeResult(&added, &requested, &omitted))
		{
			char resultText[64];
			if (omitted == 0)
				snprintf(resultText, sizeof (resultText), "%u SAMPLES IMPORTED", added);
			else
				snprintf(resultText, sizeof (resultText), "%u OF %u IMPORTED - %u OMITTED",
					added, requested, omitted);
			setStandaloneStatus(resultText);
		}
		patternLauncherBreatheFrame++;
		if (patternLauncherBreatheFrame >= 120)
			patternLauncherBreatheFrame = 0;
		if (standaloneStatusFrames > 0)
			standaloneStatusFrames--;
		if (standaloneButtonFlashFrames > 0 && --standaloneButtonFlashFrames == 0)
		{
			standaloneButtonFlash = STANDALONE_FLASH_NONE;
			if (standaloneSampleEditExitPending)
				setSampleEditMode(false);
		}
		/* The full screen is drawn after FT2's normal redraw pass in ft2_main.c. */
		return;
	}

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
