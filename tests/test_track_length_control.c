#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ft2_audio.h"
#include "ft2_fasttracks.h"
#include "ft2_replayer.h"
#include "ft2_structs.h"

ui_t ui;
audio_t audio;
song_t song;
int16_t patternNumRows[MAX_PATTERNS];

static void initializePatternRows(void)
{
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
		patternNumRows[i] = 64;
}

static void testDefaultsBoundsAndCopy(void)
{
	fastTracksPOCResetAllPatternMetadata();
	assert(fastTracksPOCPatternMetadataIsDefault(3));
	assert(fastTracksPOCGetTrackLength(3, 2) == 0);
	assert(fastTracksPOCGetEffectiveTrackLength(3, 2) == 64);
	assert(fastTracksPOCGetControlTrack(3) == -1);

	fastTracksPOCSetTrackLength(3, 2, 13);
	fastTracksPOCSetTrackLength(3, 3, UINT16_MAX);
	fastTracksPOCSetControlTrack(3, 2);
	assert(fastTracksPOCGetTrackLength(3, 2) == 13);
	assert(fastTracksPOCGetTrackLength(3, 3) == MAX_PATT_LEN);
	assert(fastTracksPOCGetEffectiveTrackLength(3, 3) == 64);
	assert(fastTracksPOCGetControlTrack(3) == 2);
	assert(!fastTracksPOCPatternMetadataIsDefault(3));

	fastTracksPOCCopyPatternMetadata(3, 4);
	assert(fastTracksPOCGetTrackLength(4, 2) == 13);
	assert(fastTracksPOCGetControlTrack(4) == 2);
	fastTracksPOCResetPatternMetadata(3);
	assert(fastTracksPOCPatternMetadataIsDefault(3));
	assert(fastTracksPOCGetTrackLength(4, 2) == 13);
}

static void testMasterPhaseAndResizeSafety(void)
{
	fastTracksPOCResetPatternMetadata(8);
	fastTracksPOCSetTrackLength(8, 1, 13);
	fastTracksPOCSetMasterCycleRow(27);
	assert(fastTracksPOCResolveMasterSourceRow(8, 1, 27) == 1);

	patternNumRows[8] = 7;
	assert(fastTracksPOCGetTrackLength(8, 1) == 13);
	assert(fastTracksPOCGetEffectiveTrackLength(8, 1) == 7);
	assert(fastTracksPOCResolveMasterSourceRow(8, 1, 27) == 6);

	patternNumRows[8] = 64;
	assert(fastTracksPOCGetTrackLength(8, 1) == 13);
	assert(fastTracksPOCGetEffectiveTrackLength(8, 1) == 13);
}

static void testXMRoundTrip(void)
{
	fastTracksPOCResetAllPatternMetadata();
	fastTracksPOCSetTrackLength(12, 0, 31);
	fastTracksPOCSetTrackLength(12, 7, 17);
	fastTracksPOCSetControlTrack(12, 7);

	FILE *file = tmpfile();
	assert(file != NULL);
	assert(fastTracksPOCWriteXMExtension(file));
	const long fileSize = ftell(file);
	assert(fileSize > 12);

	fastTracksPOCResetAllPatternMetadata();
	fastTracksPOCBeginModuleLoad();
	rewind(file);
	assert(fastTracksPOCReadXMExtension(file, (uint32_t)fileSize));
	fastTracksPOCCommitXMExtension();
	assert(fastTracksPOCGetTrackLength(12, 0) == 31);
	assert(fastTracksPOCGetTrackLength(12, 7) == 17);
	assert(fastTracksPOCGetControlTrack(12) == 7);
	fclose(file);
}

static void testLogicalCycleCompletionForwardAndReverse(void)
{
	fastTracksPOCResetAllPatternMetadata();
	patternNumRows[0] = 64;
	fastTracksPOCSetTrackLength(0, 0, 13);
	song.pattNum = 0;
	song.songLength = 1;
	song.orders[0] = 0;

	fastTracksRuntimeState_t state;
	memset(&state, 0, sizeof (state));
	state.masterEnabled = true;
	state.tracks[0].mode = FAST_TRACKS_MODE_PATTERN;
	state.tracks[0].ratioIndex = FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX;
	state.tracks[0].transportStarted = true;
	state.tracks[0].lastTPL = 1;
	fastTracksPOCSetRuntimeState(&state);

	fastTracksCrossing_t crossing;
	for (int32_t step = 1; step <= 13; step++)
	{
		assert(fastTracksPOCAdvanceAudio(0, 0, 1, &crossing, 1) == 1);
		assert(crossing.sourceRow >= 0 && crossing.sourceRow < 13);
		assert(crossing.cycleCompleted == (step == 13));
	}
	assert(crossing.sourceRow == 0);

	fastTracksPOCGetRuntimeState(&state);
	state.tracks[0].reversed = true;
	state.tracks[0].sourceRow = 0;
	state.tracks[0].tickAccumulator = 0;
	state.tracks[0].cycleStepCounter = 0;
	fastTracksPOCSetRuntimeState(&state);
	for (int32_t step = 1; step <= 13; step++)
	{
		assert(fastTracksPOCAdvanceAudio(0, 0, 1, &crossing, 1) == 1);
		assert(crossing.sourceRow >= 0 && crossing.sourceRow < 13);
		assert(crossing.cycleCompleted == (step == 13));
	}
	assert(crossing.sourceRow == 0);
}

static int32_t ticksUntilPrivateCycle(uint16_t length, uint8_t ratioIndex,
	bool reversed)
{
	fastTracksPOCResetAllPatternMetadata();
	patternNumRows[0] = 64;
	fastTracksPOCSetTrackLength(0, 0, length);
	song.pattNum = 0;
	song.songLength = 1;
	song.orders[0] = 0;

	fastTracksRuntimeState_t state;
	memset(&state, 0, sizeof (state));
	state.masterEnabled = true;
	state.tracks[0].mode = FAST_TRACKS_MODE_PATTERN;
	state.tracks[0].ratioIndex = ratioIndex;
	state.tracks[0].reversed = reversed;
	state.tracks[0].transportStarted = true;
	state.tracks[0].lastTPL = 1;
	fastTracksPOCSetRuntimeState(&state);

	for (int32_t tick = 1; tick <= 1024; tick++)
	{
		fastTracksCrossing_t crossings[FAST_TRACKS_MAX_CROSSINGS_PER_TICK];
		const int32_t count = fastTracksPOCAdvanceAudio(0, 0, 1, crossings,
			FAST_TRACKS_MAX_CROSSINGS_PER_TICK);
		assert(count <= FAST_TRACKS_MAX_CROSSINGS_PER_TICK);
		for (int32_t i = 0; i < count; i++)
		{
			assert(crossings[i].sourceRow >= 0 &&
				crossings[i].sourceRow < length);
			if (crossings[i].cycleCompleted)
				return tick;
		}
	}

	return -1;
}

static void testPrimeLengthsAndRatioDurations(void)
{
	const uint16_t primeLengths[] = { 11, 13, 17, 19 };
	fastTracksPOCResetAllPatternMetadata();
	patternNumRows[20] = 64;
	fastTracksPOCSetMasterCycleRow(221);
	for (int32_t channel = 0; channel < 4; channel++)
	{
		fastTracksPOCSetTrackLength(20, channel, primeLengths[channel]);
		assert(fastTracksPOCResolveMasterSourceRow(20, channel, 221) ==
			221 % primeLengths[channel]);
	}

	/* Ratio-bank indices are compatibility-frozen: 1:2, 3:2, and 2:1. */
	assert(ticksUntilPrivateCycle(32, 14, false) == 16);
	assert(ticksUntilPrivateCycle(32, 0, false) == 64);
	assert(ticksUntilPrivateCycle(17, 13, false) == 12);
	assert(ticksUntilPrivateCycle(17, 13, true) == 12);
}

int main(void)
{
	initializePatternRows();
	testDefaultsBoundsAndCopy();
	testMasterPhaseAndResizeSafety();
	testXMRoundTrip();
	testLogicalCycleCompletionForwardAndReverse();
	testPrimeLengthsAndRatioDurations();
	puts("Track LEN/CONTROL metadata tests passed.");
	return 0;
}
