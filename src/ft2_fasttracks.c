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
	uint16_t lastTPL;
	bool transportStarted;
	uint8_t ratioIndex;
} fastTracksChannelState_t;

#define FAST_TRACKS_MAX_CROSSINGS_PER_TICK 8

#define FAST_TRACKS_DEFAULT_CHANNEL_STATE \
	{ FAST_TRACKS_MODE_STANDARD, false, false, 0, 0, 0, 0, false, FAST_TRACKS_DEFAULT_RATIO_INDEX }

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
	state->transportStarted = false;
}

static int32_t wrapFastTracksPOCRow(int32_t row)
{
	if (song.currNumRows <= 0)
		return 0;

	row %= song.currNumRows;
	if (row < 0)
		row += song.currNumRows;

	return row;
}

static bool advanceFastTracksPOCPatternPosition(volatile fastTracksChannelState_t *state, int32_t rowDirection)
{
	state->sourceRow = wrapFastTracksPOCRow(state->sourceRow + rowDirection);
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

static bool resolveFastTracksPOCSongOrder(int32_t order, int32_t *patternNumber, int32_t *patternLength)
{
	order = wrapFastTracksPOCOrder(order);

	const int32_t pattNum = song.orders[order];
	if (pattNum < 0 || pattNum >= MAX_PATTERNS)
		return false;

	int32_t rows = patternNumRows[pattNum];
	if (rows <= 0)
		rows = 1;

	if (patternNumber != NULL)
		*patternNumber = pattNum;
	if (patternLength != NULL)
		*patternLength = rows;

	return true;
}

static bool advanceFastTracksPOCSongPosition(volatile fastTracksChannelState_t *state, int32_t rowDirection)
{
	int32_t patternLength;
	if (!resolveFastTracksPOCSongOrder(state->sourceOrder, NULL, &patternLength))
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
			if (!resolveFastTracksPOCSongOrder(state->sourceOrder, NULL, &patternLength))
				return false;

			state->sourceRow = patternLength - 1;
		}
	}

	return true;
}

static bool advanceFastTracksPOCSourcePosition(volatile fastTracksChannelState_t *state, int32_t rowDirection)
{
	switch (state->mode)
	{
		case FAST_TRACKS_MODE_PATTERN:
			return advanceFastTracksPOCPatternPosition(state, rowDirection);

		case FAST_TRACKS_MODE_SONG:
			return advanceFastTracksPOCSongPosition(state, rowDirection);

		case FAST_TRACKS_MODE_STANDARD:
		default:
			return false;
	}
}

static int32_t getFastTracksPOCMasterPhaseRow(void)
{
	/* FT2 pre-advances song.row at the end of master tick 1, one audio
	** callback before tick zero actually reads the new row. During that
	** interval the transport phase still belongs to the preceding row.
	** Treating song.row as current there gives a synchronized 1:1 Fast
	** Track the next row plus the previous row's fractional phase, causing
	** it to advance one row too far on the following tick zero. */
	if (song.tick == 1)
		return wrapFastTracksPOCRow(song.row - 1);

	return song.row;
}

static void syncFastTracksPOCTransportToMaster(volatile fastTracksChannelState_t *state,
	const fastTracksRatio_t *ratio, uint16_t masterTPL, int32_t masterElapsedTicks)
{
	state->sourceOrder = song.songPos;
	state->sourceRow = getFastTracksPOCMasterPhaseRow();
	state->tickAccumulator = ratio->denominator * masterElapsedTicks;
	state->lastTPL = masterTPL;
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

int32_t fastTracksPOCAdvanceAudio(int32_t channelIndex, uint16_t tpl,
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
		if (advanceFastTracksPOCSourcePosition(state, rowDirection))
		{
			if (crossings != NULL && crossingCount < maxCrossings)
			{
				crossings[crossingCount].sourceOrder = state->sourceOrder;
				crossings[crossingCount].sourceRow = state->sourceRow;
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
	if (!resolveFastTracksPOCSongOrder(state->sourceOrder, &pattNum, NULL))
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

	return state->sourceRow == getFastTracksPOCMasterPhaseRow() &&
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
		track->sourceRow = track->clutched ? song.row : state->sourceRow;
		track->sourceOrder = state->mode == FAST_TRACKS_MODE_SONG ?
			(int16_t)wrapFastTracksPOCOrder(state->sourceOrder) : song.songPos;
		track->sourcePattern = song.pattNum;
		if (!track->clutched && state->mode == FAST_TRACKS_MODE_SONG)
		{
			int32_t sourcePattern;
			if (resolveFastTracksPOCSongOrder(state->sourceOrder,
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
		syncFastTracksPOCTransportToMaster(state, ratio, masterTPL, masterElapsedTicks);
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
			syncFastTracksPOCTransportToMaster(state, ratio, masterTPL, masterElapsedTicks);
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
		if (resolveFastTracksPOCSongOrder(state->sourceOrder, NULL, &patternLength))
			state->sourceRow = CLAMP(state->sourceRow, 0, patternLength - 1);
	}
	else if (mode == FAST_TRACKS_MODE_PATTERN)
	{
		state->sourceRow = wrapFastTracksPOCRow(state->sourceRow);
	}

	state->mode = mode;

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
	syncFastTracksPOCTransportToMaster(state, ratio, masterTPL, masterElapsedTicks);

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
		syncFastTracksPOCTransportToMaster(state, ratio, masterTPL, masterElapsedTicks);
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
		syncFastTracksPOCTransportToMaster(state,
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
	const fastTracksCrossing_t *crossing, int32_t *patternNumber, int32_t *row)
{
	const volatile fastTracksChannelState_t *state = getFastTracksPOCChannelState(channelIndex);
	if (state == NULL || crossing == NULL || patternNumber == NULL || row == NULL)
		return false;

	int32_t pattNum;
	int32_t patternLength;
	if (state->mode == FAST_TRACKS_MODE_SONG)
	{
		if (!resolveFastTracksPOCSongOrder(crossing->sourceOrder, &pattNum, &patternLength))
			return false;
	}
	else
	{
		pattNum = song.pattNum;
		patternLength = song.currNumRows;
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
