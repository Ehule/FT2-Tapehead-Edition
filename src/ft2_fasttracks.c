#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_config.h"
#include "ft2_pattern_ed.h"
#include "ft2_random.h"
#include "ft2_structs.h"
#include "ft2_replayer.h"
#include "ft2_fasttracks_core.h"

#if FAST_TRACKS_CHANNEL_COUNT != MAX_CHANNELS
#error FasTracks snapshot/channel count must match XM channel count
#endif

/*
** FasTracks multi-channel transport
** ---------------------------------
** All 32 XM channels can own a rational private Pattern or Song transport.
** This runtime-only interpretation does not alter XM pattern data or the
** file format.
*/
#define FAST_TRACKS_MAX_CHANNELS MAX_CHANNELS

#define FAST_TRACKS_XM_META_MAGIC "THPLEN01"
#define FAST_TRACKS_XM_META_PATTERN_SIZE (1 + (FAST_TRACKS_MAX_CHANNELS * 2))
#define FAST_TRACKS_XM_META_SIZE (MAX_PATTERNS * FAST_TRACKS_XM_META_PATTERN_SIZE)

/* LEN is one song/module-wide value per tracker track. An explicit value can
** extend a shorter source pattern with blank rows, while a shorter LEN wraps
** independently inside a longer pattern. CONTROL is also song/module-wide.
** Zero follows the effective pattern domain established by the longest lane. */
static uint16_t fastTracksTrackLength[FAST_TRACKS_MAX_CHANNELS];
static uint8_t fastTracksControlTrackPlusOne;
/* THPLEN01 stored LEN and CONTROL beside every pattern. Keep accepting that
** layout and migrate each first explicit song-order value into song-wide state. */
static uint16_t pendingFastTracksPatternLength[MAX_PATTERNS][FAST_TRACKS_MAX_CHANNELS];
static uint8_t pendingFastTracksControlTrackPlusOne[MAX_PATTERNS];
static bool pendingFastTracksMetadataValid;
static volatile uint32_t fastTracksMasterCycleRow;

/*
** A track can remain selected while the master switch is suspended.
** This distinction lets the large logo stop/resume the whole Fast Tracks
** machine without destroying the user's per-track setup.
*/
static volatile bool fastTracksPOCMasterEnabled = false;

/* Symmetric rational ratio bank. All entries use the unified tick transport. */
typedef struct fastTracksRatio_t
{
	uint8_t numerator;   /* source rows */
	uint8_t denominator; /* master rows */
} fastTracksRatio_t;

static const fastTracksRatio_t fastTracksPOCRatioBank[] =
{
	/* Slower than the master transport. */
	{ 1, 2 },
	{ 2, 3 },
	{ 3, 4 },
	{ 4, 5 },
	{ 5, 6 },
	{ 7, 8 },
	{ 15, 16 },

	/* Center: master speed with an independent private phase. */
	{ 1, 1 },

	/* Faster than the master transport, mirrored around 1:1. */
	{ 17, 16 },
	{ 8, 7 },
	{ 6, 5 },
	{ 5, 4 },
	{ 4, 3 },
	{ 3, 2 },
	{ 2, 1 },

	/* Extreme high-speed transports for deliberate oddball behavior. */
	{ 3, 1 },
	{ 5, 1 }
};

#define FAST_TRACKS_RATIO_COUNT ((int32_t)(sizeof (fastTracksPOCRatioBank) / sizeof (fastTracksPOCRatioBank[0])))
#define FAST_TRACKS_DEFAULT_RATIO_INDEX FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX /* neutral 1:1 */

/*
** All mutable state belonging to one Fast Track lives together. Keeping the
** transport, phase, ratio and selection state in one object prevents the
** parallel arrays from drifting apart and gives the future clutch, pattern
** effects and 32-track expansion one shared control path.
*/
typedef struct fastTracksChannelState_t
{
	fastTracksMode_t mode;
	bool clutchHeld;
	bool reversed;
	int16_t sourceOrder;
	int32_t sourceRow;
	int32_t tickAccumulator;
	uint16_t lastTPL, cycleStepCounter;
	bool transportStarted;
	uint8_t ratioIndex;
} fastTracksChannelState_t;

#define FAST_TRACKS_MAX_CROSSINGS_PER_TICK 8

#define FAST_TRACKS_DEFAULT_CHANNEL_STATE \
	{ FAST_TRACKS_MODE_STANDARD, false, false, 0, 0, 0, 0, 0, false, FAST_TRACKS_DEFAULT_RATIO_INDEX }

static volatile fastTracksChannelState_t fastTracksPOCChannels[FAST_TRACKS_MAX_CHANNELS] =
{
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE,
	FAST_TRACKS_DEFAULT_CHANNEL_STATE, FAST_TRACKS_DEFAULT_CHANNEL_STATE
};

/* Latched global transmission clutch. While active, audible playback follows
** the master transport, but selected private transports keep advancing unseen. */
static volatile bool fastTracksPOCTransmissionClutchLatched;

void fastTracksPOCGetRuntimeState(fastTracksRuntimeState_t *state)
{
	if (state == NULL)
		return;

	state->masterEnabled = fastTracksPOCMasterEnabled;
	state->transmissionClutchLatched = fastTracksPOCTransmissionClutchLatched;
	state->masterCycleRow = fastTracksMasterCycleRow;
	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		const volatile fastTracksChannelState_t *src = &fastTracksPOCChannels[i];
		fastTracksRuntimeTrack_t *dst = &state->tracks[i];
		dst->mode = src->mode;
		dst->clutchHeld = src->clutchHeld;
		dst->reversed = src->reversed;
		dst->sourceOrder = src->sourceOrder;
		dst->sourceRow = src->sourceRow;
		dst->tickAccumulator = src->tickAccumulator;
		dst->lastTPL = src->lastTPL;
		dst->cycleStepCounter = src->cycleStepCounter;
		dst->transportStarted = src->transportStarted;
		dst->ratioIndex = src->ratioIndex;
	}
}

void fastTracksPOCSetRuntimeState(const fastTracksRuntimeState_t *state)
{
	if (state == NULL)
		return;

	fastTracksPOCMasterEnabled = state->masterEnabled;
	fastTracksPOCTransmissionClutchLatched = state->transmissionClutchLatched;
	fastTracksMasterCycleRow = state->masterCycleRow;
	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		const fastTracksRuntimeTrack_t *src = &state->tracks[i];
		volatile fastTracksChannelState_t *dst = &fastTracksPOCChannels[i];
		dst->mode = src->mode;
		dst->clutchHeld = src->clutchHeld;
		dst->reversed = src->reversed;
		dst->sourceOrder = src->sourceOrder;
		dst->sourceRow = src->sourceRow;
		dst->tickAccumulator = src->tickAccumulator;
		dst->lastTPL = src->lastTPL;
		dst->cycleStepCounter = src->cycleStepCounter;
		dst->transportStarted = src->transportStarted;
		dst->ratioIndex = src->ratioIndex;
	}
}

static bool fastTracksPOCChannelIsValid(int32_t channelIndex)
{
	return channelIndex >= 0 && channelIndex < FAST_TRACKS_MAX_CHANNELS;
}

static volatile fastTracksChannelState_t *getFastTracksPOCChannelState(int32_t channelIndex)
{
	if (!fastTracksPOCChannelIsValid(channelIndex))
		return NULL;

	return &fastTracksPOCChannels[channelIndex];
}

static void resetFastTracksPOCTransport(volatile fastTracksChannelState_t *state, int32_t sourceRow)
{
	state->sourceOrder = song.songPos;
	state->sourceRow = sourceRow;
	state->tickAccumulator = 0;
	state->lastTPL = song.speed > 0 ? song.speed : 1;
	state->cycleStepCounter = 0;
	state->transportStarted = false;
}

uint16_t fastTracksPOCGetTrackLength(uint16_t patternNumber, int32_t channelIndex)
{
	if (patternNumber >= MAX_PATTERNS || !fastTracksPOCChannelIsValid(channelIndex))
		return 0;

	return fastTracksTrackLength[channelIndex];
}

uint16_t fastTracksPOCGetExtendedPatternLength(uint16_t patternNumber)
{
	if (patternNumber >= MAX_PATTERNS)
		return 1;

	uint16_t length = (uint16_t)CLAMP(patternNumRows[patternNumber], 1,
		MAX_PATT_LEN);
	for (int32_t channelIndex = 0; channelIndex < FAST_TRACKS_MAX_CHANNELS;
		channelIndex++)
	{
		length = MAX(length, fastTracksTrackLength[channelIndex]);
	}
	return length;
}

uint16_t fastTracksPOCGetEffectiveTrackLength(uint16_t patternNumber, int32_t channelIndex)
{
	if (patternNumber >= MAX_PATTERNS || !fastTracksPOCChannelIsValid(channelIndex))
		return 1;

	const uint16_t storedLength = fastTracksTrackLength[channelIndex];
	if (storedLength == 0)
		return fastTracksPOCGetExtendedPatternLength(patternNumber);

	/* LEN deliberately outranks the source pattern's ordinary row count. Rows
	** in the extension tail are resolved as blank by the replayer, allowing a
	** short pattern to occupy a longer polymetric lane without data overread. */
	return storedLength;
}

uint16_t fastTracksPOCGetFastTrackLength(uint16_t patternNumber, int32_t channelIndex)
{
	if (tapeheadConfig.fastTracksUseTrackLengths)
		return fastTracksPOCGetEffectiveTrackLength(patternNumber, channelIndex);

	if (patternNumber >= MAX_PATTERNS || !fastTracksPOCChannelIsValid(channelIndex))
		return 1;

	return (uint16_t)CLAMP(patternNumRows[patternNumber], 1, MAX_PATT_LEN);
}

bool fastTracksPOCUsesTrackLengths(void)
{
	return tapeheadConfig.fastTracksUseTrackLengths;
}

void fastTracksPOCSetUsesTrackLengths(bool enabled)
{
	if (tapeheadConfig.fastTracksUseTrackLengths == enabled)
		return;

	tapeheadConfig.fastTracksUseTrackLengths = enabled;
	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		volatile fastTracksChannelState_t *state = &fastTracksPOCChannels[i];
		if (state->mode == FAST_TRACKS_MODE_STANDARD)
			continue;

		int32_t patternNumber = song.pattNum;
		if (state->mode == FAST_TRACKS_MODE_SONG)
		{
			const int32_t songLength = CLAMP(song.songLength, 1, MAX_ORDERS);
			int32_t order = state->sourceOrder % songLength;
			if (order < 0)
				order += songLength;
			patternNumber = song.orders[order];
		}

		const int32_t rowCount = fastTracksPOCGetFastTrackLength(
			(uint16_t)CLAMP(patternNumber, 0, MAX_PATTERNS - 1), i);
		state->sourceRow %= rowCount;
		if (state->sourceRow < 0)
			state->sourceRow += rowCount;
		state->cycleStepCounter = 0;
	}

	ui.updatePatternEditor = true;
}

int8_t fastTracksPOCGetControlTrack(uint16_t patternNumber)
{
	if (patternNumber >= MAX_PATTERNS || fastTracksControlTrackPlusOne == 0)
		return -1;

	return (int8_t)(fastTracksControlTrackPlusOne - 1);
}

bool fastTracksPOCPatternMetadataIsDefault(uint16_t patternNumber)
{
	if (patternNumber >= MAX_PATTERNS)
		return true;
	/* Song-wide LEN and CONTROL are module metadata, not ownership of any one
	** pattern slot. */
	return true;
}

void fastTracksPOCGetPatternMetadata(uint16_t patternNumber,
	fastTracksPatternMetadata_t *metadata)
{
	if (metadata == NULL)
		return;
	memset(metadata, 0, sizeof (*metadata));
	metadata->controlTrack = -1;
	if (patternNumber >= MAX_PATTERNS)
		return;

	memcpy(metadata->trackLength, fastTracksTrackLength,
		sizeof (metadata->trackLength));
	metadata->controlTrack = fastTracksPOCGetControlTrack(patternNumber);
}

void fastTracksPOCSetPatternMetadata(uint16_t patternNumber,
	const fastTracksPatternMetadata_t *metadata)
{
	if (patternNumber >= MAX_PATTERNS || metadata == NULL)
		return;

	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		fastTracksTrackLength[i] =
			MIN(metadata->trackLength[i], MAX_PATT_LEN);
	}
	fastTracksControlTrackPlusOne =
		metadata->controlTrack >= 0 && metadata->controlTrack < FAST_TRACKS_MAX_CHANNELS
		? (uint8_t)(metadata->controlTrack + 1) : 0;
	fastTracksPOCResetCycleCounters();
}

void fastTracksPOCSetTrackLength(uint16_t patternNumber, int32_t channelIndex,
	uint16_t length)
{
	if (patternNumber >= MAX_PATTERNS || !fastTracksPOCChannelIsValid(channelIndex))
		return;

	fastTracksTrackLength[channelIndex] = MIN(length, MAX_PATT_LEN);
	fastTracksPOCResetCycleCounters();
	ui.updatePatternEditor = true;
}

void fastTracksPOCSetControlTrack(uint16_t patternNumber, int32_t channelIndex)
{
	if (patternNumber >= MAX_PATTERNS)
		return;

	fastTracksControlTrackPlusOne =
		channelIndex >= 0 && channelIndex < FAST_TRACKS_MAX_CHANNELS
		? (uint8_t)(channelIndex + 1) : 0;
	fastTracksPOCResetCycleCounters();
	ui.updatePatternEditor = true;
}

void fastTracksPOCResetPatternMetadata(uint16_t patternNumber)
{
	if (patternNumber >= MAX_PATTERNS)
		return;
	/* Pattern clearing must not erase song/module-wide LEN or CONTROL state. */
}

void fastTracksPOCResetAllPatternMetadata(void)
{
	memset(fastTracksTrackLength, 0, sizeof (fastTracksTrackLength));
	fastTracksControlTrackPlusOne = 0;
	fastTracksMasterCycleRow = 0;
	fastTracksPOCResetCycleCounters();
	ui.updatePatternEditor = true;
}

void fastTracksPOCCopyPatternMetadata(uint16_t sourcePattern,
	uint16_t destinationPattern)
{
	if (sourcePattern >= MAX_PATTERNS || destinationPattern >= MAX_PATTERNS)
		return;
	/* Pattern copies do not replace song/module-wide LEN or CONTROL state. */
}

void fastTracksPOCResetMasterCycle(void)
{
	fastTracksMasterCycleRow = 0;
}

void fastTracksPOCSetMasterCycleRow(uint32_t row)
{
	fastTracksMasterCycleRow = row;
}

void fastTracksPOCAdvanceMasterCycleRow(void)
{
	if (fastTracksMasterCycleRow < UINT32_MAX)
		fastTracksMasterCycleRow++;
}

uint32_t fastTracksPOCGetMasterCycleRow(void)
{
	return fastTracksMasterCycleRow;
}

void fastTracksPOCResetCycleCounters(void)
{
	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
		fastTracksPOCChannels[i].cycleStepCounter = 0;
}

int32_t fastTracksPOCResolveMasterSourceRow(uint16_t patternNumber,
	int32_t channelIndex, int32_t masterRow)
{
	const uint16_t storedLength = fastTracksPOCGetTrackLength(patternNumber, channelIndex);
	if (storedLength == 0)
		return masterRow;

	const uint16_t effectiveLength =
		fastTracksPOCGetEffectiveTrackLength(patternNumber, channelIndex);
	return effectiveLength > 0
		? (int32_t)(fastTracksMasterCycleRow % effectiveLength) : 0;
}

static int32_t wrapFastTracksPOCRow(int32_t row, uint16_t patternNumber,
	int32_t sourceChannel)
{
	const int32_t rowCount =
		fastTracksPOCGetFastTrackLength(patternNumber, sourceChannel);
	if (rowCount <= 0)
		return 0;

	row %= rowCount;
	if (row < 0)
		row += rowCount;

	return row;
}

static bool advanceFastTracksPOCPatternPosition(volatile fastTracksChannelState_t *state,
	int32_t sourceChannel, int32_t rowDirection)
{
	const int32_t rowCount =
		fastTracksPOCGetFastTrackLength(song.pattNum, sourceChannel);
	const int32_t previousRow = wrapFastTracksPOCRow(state->sourceRow,
		song.pattNum, sourceChannel);
	int32_t nextRow = previousRow + rowDirection;
	if (nextRow >= rowCount)
		nextRow = 0;
	else if (nextRow < 0)
		nextRow = rowCount - 1;
	state->sourceRow = nextRow;
	return true;
}

static int32_t getFastTracksPOCSongLength(void)
{
	return CLAMP(song.songLength, 1, MAX_ORDERS);
}

static int32_t wrapFastTracksPOCOrder(int32_t order)
{
	const int32_t songLength = getFastTracksPOCSongLength();
	order %= songLength;
	if (order < 0)
		order += songLength;

	return order;
}

static bool resolveFastTracksPOCSongOrder(int32_t order, int32_t sourceChannel,
	int32_t *patternNumber, int32_t *patternLength)
{
	order = wrapFastTracksPOCOrder(order);

	const int32_t pattNum = song.orders[order];
	if (pattNum < 0 || pattNum >= MAX_PATTERNS)
		return false;

	const int32_t rows =
		fastTracksPOCGetFastTrackLength((uint16_t)pattNum, sourceChannel);

	if (patternNumber != NULL)
		*patternNumber = pattNum;
	if (patternLength != NULL)
		*patternLength = rows;

	return true;
}

static bool advanceFastTracksPOCSongPosition(volatile fastTracksChannelState_t *state,
	int32_t sourceChannel, int32_t rowDirection)
{
	int32_t patternLength;
	if (!resolveFastTracksPOCSongOrder(state->sourceOrder, sourceChannel, NULL,
		&patternLength))
		return false;
	if (rowDirection >= 0)
	{
		state->sourceRow++;
		if (state->sourceRow >= patternLength)
		{
			state->sourceOrder = (int16_t)wrapFastTracksPOCOrder(state->sourceOrder + 1);
			state->sourceRow = 0;
		}
	}
	else
	{
		state->sourceRow--;
		if (state->sourceRow < 0)
		{
			state->sourceOrder = (int16_t)wrapFastTracksPOCOrder(state->sourceOrder - 1);
			if (!resolveFastTracksPOCSongOrder(state->sourceOrder, sourceChannel,
				NULL, &patternLength))
				return false;

			state->sourceRow = patternLength - 1;
		}
	}

	return true;
}

static bool advanceFastTracksPOCSourcePosition(volatile fastTracksChannelState_t *state,
	int32_t sourceChannel, int32_t rowDirection)
{
	switch (state->mode)
	{
		case FAST_TRACKS_MODE_PATTERN:
			return advanceFastTracksPOCPatternPosition(state, sourceChannel,
				rowDirection);

		case FAST_TRACKS_MODE_SONG:
			return advanceFastTracksPOCSongPosition(state, sourceChannel,
				rowDirection);

		case FAST_TRACKS_MODE_STANDARD:
		default:
			return false;
	}
}

static int32_t getFastTracksPOCCycleLength(
	const volatile fastTracksChannelState_t *state, int32_t sourceChannel)
{
	if (state->mode == FAST_TRACKS_MODE_SONG)
	{
		int32_t patternLength;
		if (resolveFastTracksPOCSongOrder(state->sourceOrder, sourceChannel,
			NULL, &patternLength))
		{
			return patternLength;
		}
	}

	return fastTracksPOCGetFastTrackLength(song.pattNum, sourceChannel);
}

static int32_t getFastTracksPOCMasterPhaseRow(int32_t sourceChannel)
{
	/* FT2 pre-advances song.row at the end of master tick 1, one audio
	** callback before tick zero actually reads the new row. During that
	** interval the transport phase still belongs to the preceding row.
	** Treating song.row as current there gives a synchronized 1:1 Fast
	** Track the next row plus the previous row's fractional phase, causing
	** it to advance one row too far on the following tick zero. */
	if (song.tick == 1)
		return wrapFastTracksPOCRow(song.row - 1, song.pattNum, sourceChannel);

	return wrapFastTracksPOCRow(song.row, song.pattNum, sourceChannel);
}

static void syncFastTracksPOCTransportToMaster(volatile fastTracksChannelState_t *state,
	int32_t sourceChannel, const fastTracksRatio_t *ratio, uint16_t masterTPL,
	int32_t masterElapsedTicks)
{
	state->sourceOrder = song.songPos;
	state->sourceRow = getFastTracksPOCMasterPhaseRow(sourceChannel);
	state->tickAccumulator = ratio->denominator * masterElapsedTicks;
	state->lastTPL = masterTPL;
	state->cycleStepCounter = 0;
	state->transportStarted = true;
}

static const fastTracksRatio_t *getFastTracksPOCRatio(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL)
		return &fastTracksPOCRatioBank[FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX];

	return &fastTracksPOCRatioBank[state->ratioIndex % FAST_TRACKS_RATIO_COUNT];
}

static int32_t getFastTracksPOCThreshold(int32_t channelIndex, uint16_t tpl)
{
	const fastTracksRatio_t *ratio = getFastTracksPOCRatio(channelIndex);
	if (tpl == 0)
		tpl = 1;

	return ratio->denominator * tpl;
}

int32_t fastTracksPOCAdvanceAudio(int32_t channelIndex, int32_t sourceChannel,
	uint16_t tpl,
	fastTracksCrossing_t *crossings, int32_t maxCrossings)
{
	volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL)
		return 0;

	const fastTracksRatio_t *ratio = getFastTracksPOCRatio(channelIndex);
	bool transportStarted = state->transportStarted;
	int32_t accumulator = state->tickAccumulator;
	uint16_t lastTPL = state->lastTPL;
	const int32_t requestedCrossings = fastTracksClockTick(&transportStarted,
		&accumulator, &lastTPL, ratio->numerator, ratio->denominator, tpl);

	state->transportStarted = transportStarted;
	state->tickAccumulator = accumulator;
	state->lastTPL = lastTPL;

	int32_t crossingCount = 0;
	for (int32_t processedCrossings = 0;
		processedCrossings < requestedCrossings; processedCrossings++)
	{
		/* Direction belongs to the private track transport. Record every row
		** crossed during this audio tick instead of collapsing the movement to
		** one Boolean/final-row event. The present ratio bank needs at most five
		** entries at TPL=1; the larger fixed bound leaves safe expansion room. */
		const int32_t rowDirection = state->reversed ? -1 : 1;
		const int32_t cycleLength =
			getFastTracksPOCCycleLength(state, sourceChannel);
		if (advanceFastTracksPOCSourcePosition(state, sourceChannel, rowDirection))
		{
			state->cycleStepCounter++;
			const bool cycleCompleted = cycleLength > 0 &&
				state->cycleStepCounter >= cycleLength;
			if (cycleCompleted)
				state->cycleStepCounter = 0;
			if (crossings != NULL && crossingCount < maxCrossings)
			{
				crossings[crossingCount].sourceOrder = state->sourceOrder;
				crossings[crossingCount].sourceRow = state->sourceRow;
				crossings[crossingCount].cycleCompleted = cycleCompleted;
			}

			crossingCount++;
		}
	}

	return crossingCount;
}

bool fastTracksPOCMasterIsEnabled(void)
{
	return fastTracksPOCMasterEnabled;
}

bool fastTracksPOCIsSelected(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	return state != NULL && state->mode != FAST_TRACKS_MODE_STANDARD;
}

fastTracksMode_t fastTracksPOCGetMode(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL)
		return FAST_TRACKS_MODE_STANDARD;

	return state->mode;
}

bool fastTracksPOCIsEnabled(int32_t channelIndex)
{
	return fastTracksPOCMasterEnabled && fastTracksPOCIsSelected(channelIndex);
}

bool fastTracksPOCIsClutched(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	return state != NULL && (state->clutchHeld || fastTracksPOCTransmissionClutchLatched);
}

bool fastTracksPOCIsReversed(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	return state != NULL && state->reversed;
}

bool fastTracksPOCTransmissionClutchIsLatched(void)
{
	return fastTracksPOCTransmissionClutchLatched;
}

bool fastTracksPOCAnyEnabled(void)
{
	if (!fastTracksPOCMasterEnabled)
		return false;

	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		if (fastTracksPOCChannels[i].mode != FAST_TRACKS_MODE_STANDARD)
			return true;
	}

	return false;
}

int32_t fastTracksPOCGetSourceRow(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL || state->clutchHeld || fastTracksPOCTransmissionClutchLatched)
		return song.row;

	return state->sourceRow;
}

int32_t fastTracksPOCGetSourceOrder(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL || state->mode != FAST_TRACKS_MODE_SONG)
		return song.songPos;

	return wrapFastTracksPOCOrder(state->sourceOrder);
}

int32_t fastTracksPOCGetSourcePattern(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL || state->mode != FAST_TRACKS_MODE_SONG)
		return song.pattNum;

	int32_t pattNum;
	if (!resolveFastTracksPOCSongOrder(state->sourceOrder, channelIndex,
			&pattNum, NULL))
		return song.pattNum;

	return pattNum;
}

bool fastTracksPOCIsMasterAligned(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL)
		return false;
	if (state->clutchHeld || fastTracksPOCTransmissionClutchLatched)
		return true;

	const uint16_t masterTPL = song.speed > 0 ? song.speed : 1;
	int32_t masterElapsedTicks = masterTPL - song.tick;
	if (masterElapsedTicks < 0)
		masterElapsedTicks = 0;
	else if (masterElapsedTicks >= masterTPL)
		masterElapsedTicks = masterTPL - 1;

	const fastTracksRatio_t *ratio = getFastTracksPOCRatio(channelIndex);
	const int32_t expectedAccumulator = ratio->denominator * masterElapsedTicks;

	return state->sourceRow == getFastTracksPOCMasterPhaseRow(channelIndex) &&
		state->tickAccumulator == expectedAccumulator;
}

uint8_t fastTracksPOCGetRatioNumerator(int32_t channelIndex)
{
	return getFastTracksPOCRatio(channelIndex)->numerator;
}

uint8_t fastTracksPOCGetRatioDenominator(int32_t channelIndex)
{
	return getFastTracksPOCRatio(channelIndex)->denominator;
}

uint8_t fastTracksPOCGetRatioIndex(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state =
		getFastTracksPOCChannelState(channelIndex);
	if (state == NULL)
		return FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX;

	return state->ratioIndex % FAST_TRACKS_RATIO_COUNT;
}

uint8_t fastTracksPOCGetRatioCount(void)
{
	return FAST_TRACKS_RATIO_COUNT;
}

void fastTracksPOCGetSnapshot(fastTracksSnapshot_t *snapshot)
{
	if (snapshot == NULL)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	snapshot->masterEnabled = fastTracksPOCMasterEnabled;
	snapshot->transmissionClutchLatched = fastTracksPOCTransmissionClutchLatched;

	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		fastTracksTrackSnapshot_t *track = &snapshot->tracks[i];
		const volatile fastTracksChannelState_t *state = &fastTracksPOCChannels[i];
		const fastTracksRatio_t *ratio = getFastTracksPOCRatio(i);

		track->mode = state->mode;
		track->selected = state->mode != FAST_TRACKS_MODE_STANDARD;
		track->enabled = fastTracksPOCMasterEnabled && track->selected;
		track->clutched = state->clutchHeld || fastTracksPOCTransmissionClutchLatched;
		track->reversed = state->reversed;
		track->sourceRow = track->clutched
			? fastTracksPOCResolveMasterSourceRow(song.pattNum, i, song.row)
			: state->sourceRow;
		track->sourceOrder = state->mode == FAST_TRACKS_MODE_SONG ?
			(int16_t)wrapFastTracksPOCOrder(state->sourceOrder) : song.songPos;
		track->sourcePattern = song.pattNum;
		if (!track->clutched && state->mode == FAST_TRACKS_MODE_SONG)
		{
			int32_t sourcePattern;
			if (resolveFastTracksPOCSongOrder(state->sourceOrder, i,
				&sourcePattern, NULL))
			{
				track->sourcePattern = (int16_t)sourcePattern;
			}
		}
		track->ratioNumerator = ratio->numerator;
		track->ratioDenominator = ratio->denominator;
		track->masterAligned = fastTracksPOCIsMasterAligned(i);
	}

	if (audioWasntLocked)
		unlockAudio();
}

void fastTracksPOCClutchPress(int32_t channelIndex)
{
	if (!fastTracksPOCIsSelected(channelIndex))
		return;

	fastTracksPOCSetClutch(channelIndex, true);
}

void fastTracksPOCClutchRelease(int32_t channelIndex)
{
	fastTracksPOCSetClutch(channelIndex, false);
}

void fastTracksPOCSetTransmissionClutch(bool engaged)
{
	if (fastTracksPOCTransmissionClutchLatched == engaged)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	/* Do not touch any private row, accumulator or ratio here. The entire point
	** of the transmission clutch is that those hidden transports keep drifting
	** while audible playback temporarily rides the master transport. Pattern
	** commands may prepare this latent state while Fast Tracks is globally off. */
	fastTracksPOCTransmissionClutchLatched = engaged;

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCTransmissionClutchToggle(void)
{
	if (!fastTracksPOCAnyEnabled() && !fastTracksPOCTransmissionClutchLatched)
		return;

	fastTracksPOCSetTransmissionClutch(!fastTracksPOCTransmissionClutchLatched);
}

void fastTracksPOCSetRatioIndex(int32_t channelIndex, uint8_t ratioIndex)
{
	volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL || ratioIndex >= FAST_TRACKS_RATIO_COUNT)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	const int32_t oldThreshold = getFastTracksPOCThreshold(channelIndex, song.speed);
	const int32_t oldAccumulator = state->tickAccumulator;

	state->ratioIndex = ratioIndex;

	/* Keep the private row continuous and carry the same normalized phase
	** into the new ratio. Pattern commands and live Alt+Shift changes therefore
	** share the same transport behavior and do not impose a hidden phase reset. */
	const int32_t newThreshold = getFastTracksPOCThreshold(channelIndex, song.speed);
	if (oldThreshold > 0)
		state->tickAccumulator = (int32_t)(((int64_t)oldAccumulator * newThreshold) / oldThreshold);
	else
		state->tickAccumulator = 0;

	state->lastTPL = song.speed > 0 ? song.speed : 1;

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCCycleRatio(int32_t channelIndex)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL)
		return;

	fastTracksPOCSetRatioIndex(channelIndex, (uint8_t)((state->ratioIndex + 1) % FAST_TRACKS_RATIO_COUNT));
}

void fastTracksPOCSetClutch(int32_t channelIndex, bool engaged)
{
	volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL || state->clutchHeld == engaged)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (!engaged)
	{
		const uint16_t masterTPL = song.speed > 0 ? song.speed : 1;
		int32_t masterElapsedTicks = masterTPL - song.tick;
		if (masterElapsedTicks < 0)
			masterElapsedTicks = 0;
		else if (masterElapsedTicks >= masterTPL)
			masterElapsedTicks = masterTPL - 1;

		const fastTracksRatio_t *ratio = getFastTracksPOCRatio(channelIndex);
		syncFastTracksPOCTransportToMaster(state, channelIndex, ratio, masterTPL,
			masterElapsedTicks);
	}

	/* Pattern commands may prepare clutch state while the master Fast Tracks
	** switch or this individual track is inactive. The state remains latent
	** until that transport is enabled. */
	state->clutchHeld = engaged;

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCRandomizeSelectedRatios(bool syncToMaster)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	const uint16_t masterTPL = song.speed > 0 ? song.speed : 1;
	int32_t masterElapsedTicks = masterTPL - song.tick;
	if (masterElapsedTicks < 0)
		masterElapsedTicks = 0;
	else if (masterElapsedTicks >= masterTPL)
		masterElapsedTicks = masterTPL - 1;

	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		volatile fastTracksChannelState_t *state = &fastTracksPOCChannels[i];
		if (state->mode == FAST_TRACKS_MODE_STANDARD)
			continue;

		const int32_t oldRatioIndex = state->ratioIndex % FAST_TRACKS_RATIO_COUNT;
		const int32_t oldThreshold = getFastTracksPOCThreshold(i, masterTPL);
		const int32_t oldAccumulator = state->tickAccumulator;

		/* Pick uniformly from every ratio except the current one. Duplicates
		** between channels are intentionally allowed. */
		int32_t newRatioIndex = randoml(FAST_TRACKS_RATIO_COUNT - 1);
		if (newRatioIndex >= oldRatioIndex)
			newRatioIndex++;

		state->ratioIndex = (uint8_t)newRatioIndex;

		if (syncToMaster)
		{
			const fastTracksRatio_t *ratio = getFastTracksPOCRatio(i);
			syncFastTracksPOCTransportToMaster(state, i, ratio, masterTPL,
				masterElapsedTicks);
		}
		else
		{
			/* Dirty randomization preserves the private row and normalized
			** sub-row phase while changing only its future transport rate. */
			const int32_t newThreshold = getFastTracksPOCThreshold(i, masterTPL);
			if (oldThreshold > 0)
				state->tickAccumulator = (int32_t)(((int64_t)oldAccumulator * newThreshold) / oldThreshold);
			else
				state->tickAccumulator = 0;

			state->lastTPL = masterTPL;
		}
	}

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCSetMode(int32_t channelIndex, fastTracksMode_t mode)
{
	volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL || mode < FAST_TRACKS_MODE_STANDARD || mode > FAST_TRACKS_MODE_SONG || state->mode == mode)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (mode == FAST_TRACKS_MODE_STANDARD)
	{
		state->clutchHeld = false;
	}
	else if (state->mode == FAST_TRACKS_MODE_STANDARD)
	{
		/* Enter either private transport from the current audible master
		** position. Pattern and Song mode then share the established ratio,
		** phase, clutch and direction machinery. */
		resetFastTracksPOCTransport(state, song.row);
	}
	else if (mode == FAST_TRACKS_MODE_SONG)
	{
		/* Promote the current private row into the master's current order.
		** This preserves the displacement already created in Pattern mode. */
		state->sourceOrder = song.songPos;

		int32_t patternLength;
		if (resolveFastTracksPOCSongOrder(state->sourceOrder, channelIndex,
			NULL, &patternLength))
			state->sourceRow = CLAMP(state->sourceRow, 0, patternLength - 1);
	}
	else if (mode == FAST_TRACKS_MODE_PATTERN)
	{
		state->sourceRow = wrapFastTracksPOCRow(state->sourceRow,
			song.pattNum, channelIndex);
	}

	state->mode = mode;
	state->cycleStepCounter = 0;

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCToggleSongModeForTest(int32_t channelIndex)
{
	const fastTracksMode_t mode = fastTracksPOCGetMode(channelIndex);
	if (mode == FAST_TRACKS_MODE_STANDARD)
		fastTracksPOCSetMode(channelIndex, FAST_TRACKS_MODE_SONG);
	else if (mode == FAST_TRACKS_MODE_SONG)
		fastTracksPOCSetMode(channelIndex, FAST_TRACKS_MODE_PATTERN);
	else
		fastTracksPOCSetMode(channelIndex, FAST_TRACKS_MODE_SONG);
}

void fastTracksPOCSetSelectedMode(fastTracksMode_t mode)
{
	if (mode != FAST_TRACKS_MODE_PATTERN && mode != FAST_TRACKS_MODE_SONG)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		if (fastTracksPOCChannels[i].mode != FAST_TRACKS_MODE_STANDARD)
			fastTracksPOCSetMode(i, mode);
	}

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCSetTrackEnabled(int32_t channelIndex, bool enabled)
{
	volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL || fastTracksPOCIsSelected(channelIndex) == enabled)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	if (enabled)
	{
		/* Match the existing Ctrl+Shift track toggle: enabling publishes a
		** fresh private transport aligned to the current master row. */
		resetFastTracksPOCTransport(state, song.row);
		state->mode = FAST_TRACKS_MODE_PATTERN;
	}
	else
	{
		/* The track remains audible from the ordinary master transport while
		** its private FastTracks transport is disengaged. */
		state->clutchHeld = false;
		state->mode = FAST_TRACKS_MODE_STANDARD;
	}

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCSyncTrackToMaster(int32_t channelIndex)
{
	volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	const uint16_t masterTPL = song.speed > 0 ? song.speed : 1;
	int32_t masterElapsedTicks = masterTPL - song.tick;
	if (masterElapsedTicks < 0)
		masterElapsedTicks = 0;
	else if (masterElapsedTicks >= masterTPL)
		masterElapsedTicks = masterTPL - 1;

	/* This is a one-shot phase correction. The ratio, clutch and selected
	** state remain unchanged, so a non-1:1 track immediately begins drifting
	** again after the command. It also works on latent disabled transports. */
	const fastTracksRatio_t *ratio = getFastTracksPOCRatio(channelIndex);
	syncFastTracksPOCTransportToMaster(state, channelIndex, ratio, masterTPL,
		masterElapsedTicks);

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCToggle(int32_t channelIndex)
{
	fastTracksPOCSetTrackEnabled(channelIndex, !fastTracksPOCIsSelected(channelIndex));
}

void fastTracksPOCToggleDirection(int32_t channelIndex)
{
	volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL)
		return;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	state->reversed = !state->reversed;
	state->cycleStepCounter = 0;

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCResetForLoadedModule(void)
{
	fastTracksPOCTransmissionClutchLatched = false;
	/*
	** A newly loaded module has a different pattern map and row count.
	** Keep the user's selected tracks and master state, but discard source
	** rows and phase offsets that belonged to the previous module.
	*/
	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		fastTracksPOCChannels[i].clutchHeld = false;
		resetFastTracksPOCTransport(&fastTracksPOCChannels[i], song.row);
	}
}

void fastTracksPOCSetMasterEnabled(bool enabled)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	fastTracksPOCMasterEnabled = enabled;

	/* Keep every private transport and ratio intact while bypassed. */
	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
	changeLogoType(config.id_FastLogo);
}

void fastTracksPOCSyncSelectedToMaster(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	const uint16_t masterTPL = song.speed > 0 ? song.speed : 1;
	int32_t masterElapsedTicks = masterTPL - song.tick;
	if (masterElapsedTicks < 0)
		masterElapsedTicks = 0;
	else if (masterElapsedTicks >= masterTPL)
		masterElapsedTicks = masterTPL - 1;

	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		volatile fastTracksChannelState_t *state = &fastTracksPOCChannels[i];
		if (state->mode == FAST_TRACKS_MODE_STANDARD)
			continue;

		const fastTracksRatio_t *ratio = getFastTracksPOCRatio(i);
		syncFastTracksPOCTransportToMaster(state, i, ratio, masterTPL,
			masterElapsedTicks);
	}

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCSetAllRatiosOneToOne(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		volatile fastTracksChannelState_t *state = &fastTracksPOCChannels[i];
		if (state->mode == FAST_TRACKS_MODE_STANDARD)
			continue;

		const int32_t oldThreshold = getFastTracksPOCThreshold(i, song.speed);
		const int32_t oldAccumulator = state->tickAccumulator;

		state->ratioIndex = FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX;

		/* Preserve each private transport row and its normalized tick phase.
		** Unlike fastTracksPOCResetAllRatios(), this deliberately does not
		** synchronize anything to the master transport. */
		const int32_t newThreshold = getFastTracksPOCThreshold(i, song.speed);
		if (oldThreshold > 0)
			state->tickAccumulator = (int32_t)(((int64_t)oldAccumulator * newThreshold) / oldThreshold);
		else
			state->tickAccumulator = 0;

		state->lastTPL = song.speed > 0 ? song.speed : 1;
	}

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCResetAllRatios(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	const uint16_t masterTPL = song.speed > 0 ? song.speed : 1;
	int32_t masterElapsedTicks = masterTPL - song.tick;
	if (masterElapsedTicks < 0)
		masterElapsedTicks = 0;
	else if (masterElapsedTicks >= masterTPL)
		masterElapsedTicks = masterTPL - 1;

	for (int32_t i = 0; i < FAST_TRACKS_MAX_CHANNELS; i++)
	{
		volatile fastTracksChannelState_t *state = &fastTracksPOCChannels[i];
		if (state->mode == FAST_TRACKS_MODE_STANDARD)
			continue;

		state->ratioIndex = FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX;
		syncFastTracksPOCTransportToMaster(state, i,
			&fastTracksPOCRatioBank[FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX], masterTPL, masterElapsedTicks);
	}

	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
}

void fastTracksPOCMasterToggle(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	const SDL_Keymod modifiers = SDL_GetModState();
	if (modifiers & KMOD_SHIFT)
	{
		if (audioWasntLocked)
			unlockAudio();

		fastTracksPOCSyncSelectedToMaster();
		return;
	}

	const bool enabled = !fastTracksPOCMasterEnabled;

	if (audioWasntLocked)
		unlockAudio();

	fastTracksPOCSetMasterEnabled(enabled);
}

bool fastTracksPOCResolveCrossing(int32_t channelIndex,
	int32_t sourceChannel, const fastTracksCrossing_t *crossing,
	int32_t *patternNumber, int32_t *row)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL || crossing == NULL || patternNumber == NULL || row == NULL)
		return false;

	int32_t pattNum;
	int32_t patternLength;
	if (state->mode == FAST_TRACKS_MODE_SONG)
	{
		if (!resolveFastTracksPOCSongOrder(crossing->sourceOrder, sourceChannel,
			&pattNum, &patternLength))
			return false;
	}
	else
	{
		pattNum = song.pattNum;
		patternLength = fastTracksPOCGetFastTrackLength(song.pattNum,
			sourceChannel);
	}

	if (pattNum < 0 || pattNum >= MAX_PATTERNS || patternLength <= 0)
		return false;

	int32_t sourceRow = crossing->sourceRow % patternLength;
	if (sourceRow < 0)
		sourceRow += patternLength;

	*patternNumber = pattNum;
	*row = sourceRow;
	return true;
}

bool fastTracksPOCWriteXMExtension(FILE *f)
{
	if (f == NULL)
		return false;

	const uint32_t payloadSize = FAST_TRACKS_XM_META_SIZE;
	if (fwrite(FAST_TRACKS_XM_META_MAGIC, 1, 8, f) != 8 ||
		fwrite(&payloadSize, sizeof (payloadSize), 1, f) != 1)
	{
		return false;
	}

	for (uint16_t patternNumber = 0; patternNumber < MAX_PATTERNS; patternNumber++)
	{
		if (fwrite(&fastTracksControlTrackPlusOne, 1, 1, f) != 1 ||
			fwrite(fastTracksTrackLength, sizeof (uint16_t),
				FAST_TRACKS_MAX_CHANNELS, f) != FAST_TRACKS_MAX_CHANNELS)
		{
			return false;
		}
	}

	return true;
}

void fastTracksPOCBeginModuleLoad(void)
{
	pendingFastTracksMetadataValid = false;
	memset(pendingFastTracksPatternLength, 0,
		sizeof (pendingFastTracksPatternLength));
	memset(pendingFastTracksControlTrackPlusOne, 0,
		sizeof (pendingFastTracksControlTrackPlusOne));
}

bool fastTracksPOCReadXMExtension(FILE *f, uint32_t fileSize)
{
	if (f == NULL)
		return false;

	const long position = ftell(f);
	if (position < 0 ||
		(uint64_t)position + 12 + FAST_TRACKS_XM_META_SIZE > fileSize)
	{
		return true; /* no extension is valid */
	}

	char magic[8];
	uint32_t payloadSize;
	if (fread(magic, 1, sizeof (magic), f) != sizeof (magic) ||
		fread(&payloadSize, sizeof (payloadSize), 1, f) != 1)
	{
		return false;
	}
	if (memcmp(magic, FAST_TRACKS_XM_META_MAGIC, sizeof (magic)) != 0)
	{
		fseek(f, position, SEEK_SET);
		return true;
	}
	if (payloadSize != FAST_TRACKS_XM_META_SIZE)
		return false;

	for (uint16_t patternNumber = 0; patternNumber < MAX_PATTERNS; patternNumber++)
	{
		uint8_t controlPlusOne;
		uint16_t lengths[FAST_TRACKS_MAX_CHANNELS];
		if (fread(&controlPlusOne, 1, 1, f) != 1 ||
			fread(lengths, sizeof (uint16_t), FAST_TRACKS_MAX_CHANNELS, f) !=
				FAST_TRACKS_MAX_CHANNELS)
		{
			return false;
		}
		if (controlPlusOne > FAST_TRACKS_MAX_CHANNELS)
			return false;
		for (int32_t channelIndex = 0; channelIndex < FAST_TRACKS_MAX_CHANNELS;
			channelIndex++)
		{
			if (lengths[channelIndex] > MAX_PATT_LEN)
				return false;
		}
		pendingFastTracksControlTrackPlusOne[patternNumber] = controlPlusOne;
		memcpy(pendingFastTracksPatternLength[patternNumber], lengths,
			sizeof (lengths));
	}

	pendingFastTracksMetadataValid = true;
	return true;
}

void fastTracksPOCCommitXMExtension(void)
{
	fastTracksPOCResetAllPatternMetadata();
	if (pendingFastTracksMetadataValid)
	{
		/* Prefer the first explicit value encountered by the actual song. This
		** preserves the performer's opening setup when loading an older file
		** whose THPLEN01 metadata varied from pattern to pattern. */
		bool controlResolved = false;
		bool resolved[FAST_TRACKS_MAX_CHANNELS] = { false };
		for (int32_t order = 0; order < song.songLength; order++)
		{
			const uint16_t patternNumber = song.orders[order];
			if (patternNumber >= MAX_PATTERNS)
				continue;
			if (!controlResolved &&
				pendingFastTracksControlTrackPlusOne[patternNumber] != 0)
			{
				fastTracksControlTrackPlusOne =
					pendingFastTracksControlTrackPlusOne[patternNumber];
				controlResolved = true;
			}
			for (int32_t channelIndex = 0; channelIndex < FAST_TRACKS_MAX_CHANNELS;
				channelIndex++)
			{
				const uint16_t length =
					pendingFastTracksPatternLength[patternNumber][channelIndex];
				if (!resolved[channelIndex] && length != 0)
				{
					fastTracksTrackLength[channelIndex] = length;
					resolved[channelIndex] = true;
				}
			}
		}
		/* Also recover metadata belonging only to unused patterns. */
		for (uint16_t patternNumber = 0; patternNumber < MAX_PATTERNS;
			patternNumber++)
		{
			if (!controlResolved &&
				pendingFastTracksControlTrackPlusOne[patternNumber] != 0)
			{
				fastTracksControlTrackPlusOne =
					pendingFastTracksControlTrackPlusOne[patternNumber];
				controlResolved = true;
			}
			for (int32_t channelIndex = 0; channelIndex < FAST_TRACKS_MAX_CHANNELS;
				channelIndex++)
			{
				const uint16_t length =
					pendingFastTracksPatternLength[patternNumber][channelIndex];
				if (!resolved[channelIndex] && length != 0)
				{
					fastTracksTrackLength[channelIndex] = length;
					resolved[channelIndex] = true;
				}
			}
		}
	}
	pendingFastTracksMetadataValid = false;
}
