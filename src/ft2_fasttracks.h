#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FAST_TRACKS_CHANNEL_COUNT 32
#define FAST_TRACKS_MAX_CROSSINGS_PER_TICK 8
#define FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX 7

/*
** FasTracks transport source selected independently for each tracker channel.
** Each tracker channel can independently use standard, pattern-relative or
** song-order-relative playback.
*/
typedef enum fastTracksMode_t
{
	FAST_TRACKS_MODE_STANDARD = 0,
	FAST_TRACKS_MODE_PATTERN,
	FAST_TRACKS_MODE_SONG
} fastTracksMode_t;

typedef struct fastTracksCrossing_t
{
	int16_t sourceOrder;
	int32_t sourceRow;
} fastTracksCrossing_t;

/*
** One coherent UI view of a track. The replayer owns the mutable transport;
** drawing code receives a complete snapshot instead of reading individual
** fields while the audio callback may be advancing them.
*/
typedef struct fastTracksTrackSnapshot_t
{
	fastTracksMode_t mode;
	bool enabled, selected, clutched, reversed, masterAligned;
	int16_t sourceOrder;
	int16_t sourcePattern;
	int32_t sourceRow;
	uint8_t ratioNumerator, ratioDenominator;
} fastTracksTrackSnapshot_t;

typedef struct fastTracksSnapshot_t
{
	bool masterEnabled, transmissionClutchLatched;
	fastTracksTrackSnapshot_t tracks[FAST_TRACKS_CHANNEL_COUNT];
} fastTracksSnapshot_t;

/* Exact runtime snapshot used by the offline composition baker. Unlike the
** UI snapshot, this preserves fractional transport phase as well. */
typedef struct fastTracksRuntimeTrack_t
{
	fastTracksMode_t mode;
	bool clutchHeld, reversed, transportStarted;
	int16_t sourceOrder;
	int32_t sourceRow, tickAccumulator;
	uint16_t lastTPL;
	uint8_t ratioIndex;
} fastTracksRuntimeTrack_t;

typedef struct fastTracksRuntimeState_t
{
	bool masterEnabled, transmissionClutchLatched;
	fastTracksRuntimeTrack_t tracks[FAST_TRACKS_CHANNEL_COUNT];
} fastTracksRuntimeState_t;

/* UI/control-thread interface. */
bool fastTracksPOCMasterIsEnabled(void);
bool fastTracksPOCIsSelected(int32_t channelIndex);
fastTracksMode_t fastTracksPOCGetMode(int32_t channelIndex);
bool fastTracksPOCIsEnabled(int32_t channelIndex);
bool fastTracksPOCIsClutched(int32_t channelIndex);
bool fastTracksPOCIsReversed(int32_t channelIndex);
bool fastTracksPOCTransmissionClutchIsLatched(void);
bool fastTracksPOCAnyEnabled(void);
int32_t fastTracksPOCGetSourceRow(int32_t channelIndex);
int32_t fastTracksPOCGetSourceOrder(int32_t channelIndex);
int32_t fastTracksPOCGetSourcePattern(int32_t channelIndex);
bool fastTracksPOCIsMasterAligned(int32_t channelIndex);
uint8_t fastTracksPOCGetRatioNumerator(int32_t channelIndex);
uint8_t fastTracksPOCGetRatioDenominator(int32_t channelIndex);
uint8_t fastTracksPOCGetRatioIndex(int32_t channelIndex);
uint8_t fastTracksPOCGetRatioCount(void);
void fastTracksPOCGetSnapshot(fastTracksSnapshot_t *snapshot);
void fastTracksPOCGetRuntimeState(fastTracksRuntimeState_t *state);
void fastTracksPOCSetRuntimeState(const fastTracksRuntimeState_t *state);
void fastTracksPOCClutchPress(int32_t channelIndex);
void fastTracksPOCClutchRelease(int32_t channelIndex);
void fastTracksPOCSetTransmissionClutch(bool engaged);
void fastTracksPOCTransmissionClutchToggle(void);
void fastTracksPOCCycleRatio(int32_t channelIndex);
void fastTracksPOCSetRatioIndex(int32_t channelIndex, uint8_t ratioIndex);
void fastTracksPOCSetClutch(int32_t channelIndex, bool engaged);
void fastTracksPOCRandomizeSelectedRatios(bool syncToMaster);
void fastTracksPOCSyncSelectedToMaster(void);
void fastTracksPOCSetAllRatiosOneToOne(void);
void fastTracksPOCResetAllRatios(void);
void fastTracksPOCSetMode(int32_t channelIndex, fastTracksMode_t mode);
void fastTracksPOCSetSelectedMode(fastTracksMode_t mode);
void fastTracksPOCToggleSongModeForTest(int32_t channelIndex);
void fastTracksPOCSetTrackEnabled(int32_t channelIndex, bool enabled);
void fastTracksPOCSyncTrackToMaster(int32_t channelIndex);
void fastTracksPOCToggle(int32_t channelIndex);
void fastTracksPOCToggleDirection(int32_t channelIndex);
void fastTracksPOCResetForLoadedModule(void);
void fastTracksPOCSetMasterEnabled(bool enabled);
void fastTracksPOCMasterToggle(void);

/* Audio-thread transport interface. These never acquire the SDL audio lock. */
int32_t fastTracksPOCAdvanceAudio(int32_t channelIndex, uint16_t tpl,
	fastTracksCrossing_t *crossings, int32_t maxCrossings);
bool fastTracksPOCResolveCrossing(int32_t channelIndex,
	const fastTracksCrossing_t *crossing, int32_t *patternNumber, int32_t *row);
