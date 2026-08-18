#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ft2_audio.h"
#include "ft2_config.h"
#include "ft2_fasttracks.h"
#include "ft2_replayer.h"
#include "ft2_structs.h"

ui_t ui;
audio_t audio;
song_t song;
tapeheadConfig_t tapeheadConfig;
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
	assert(fastTracksPOCGetEffectiveTrackLength(3, 3) == MAX_PATT_LEN);
	assert(fastTracksPOCGetExtendedPatternLength(3) == MAX_PATT_LEN);
	assert(fastTracksPOCGetEffectiveTrackLength(3, 0) == MAX_PATT_LEN);
	assert(fastTracksPOCGetTrackLength(4, 2) == 13); /* song-wide lane */
	assert(fastTracksPOCGetControlTrack(3) == 2);
	assert(fastTracksPOCGetControlTrack(4) == 2); /* song-wide CONTROL */
	assert(fastTracksPOCPatternMetadataIsDefault(3));

	fastTracksPOCCopyPatternMetadata(3, 4);
	assert(fastTracksPOCGetTrackLength(4, 2) == 13);
	assert(fastTracksPOCGetControlTrack(4) == 2);
	fastTracksPOCResetPatternMetadata(3);
	assert(fastTracksPOCPatternMetadataIsDefault(3));
	assert(fastTracksPOCGetControlTrack(3) == 2);
	assert(fastTracksPOCGetControlTrack(4) == 2);
	assert(fastTracksPOCGetTrackLength(4, 2) == 13);
}

static void testMasterPhaseAndResizeSafety(void)
{
	fastTracksPOCResetAllPatternMetadata();
	fastTracksPOCSetTrackLength(8, 1, 13);
	fastTracksPOCSetMasterCycleRow(27);
	assert(fastTracksPOCResolveMasterSourceRow(8, 1, 27) == 1);

	patternNumRows[8] = 7;
	assert(fastTracksPOCGetTrackLength(8, 1) == 13);
	assert(fastTracksPOCGetEffectiveTrackLength(8, 1) == 13);
	assert(fastTracksPOCResolveMasterSourceRow(8, 1, 27) == 1);

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
	assert(fastTracksPOCGetControlTrack(13) == 7);
	fclose(file);
}

static void testPatternLocalExtensionMigratesInSongOrder(void)
{
	FILE *file = tmpfile();
	assert(file != NULL);

	const uint32_t payloadSize =
		MAX_PATTERNS * (1 + (MAX_CHANNELS * sizeof (uint16_t)));
	assert(fwrite("THPLEN01", 1, 8, file) == 8);
	assert(fwrite(&payloadSize, sizeof (payloadSize), 1, file) == 1);
	for (uint16_t patternNumber = 0; patternNumber < MAX_PATTERNS;
		patternNumber++)
	{
		uint8_t controlPlusOne = 0;
		uint16_t lengths[MAX_CHANNELS] = { 0 };
		if (patternNumber == 5)
		{
			controlPlusOne = 4;
			lengths[0] = 25;
		}
		else if (patternNumber == 2)
		{
			controlPlusOne = 7;
			lengths[0] = 17;
		}

		assert(fwrite(&controlPlusOne, 1, 1, file) == 1);
		assert(fwrite(lengths, sizeof (uint16_t), MAX_CHANNELS, file) ==
			MAX_CHANNELS);
	}

	const long fileSize = ftell(file);
	assert(fileSize > 12);
	song.songLength = 3;
	song.orders[0] = 0;
	song.orders[1] = 5;
	song.orders[2] = 2;
	fastTracksPOCBeginModuleLoad();
	rewind(file);
	assert(fastTracksPOCReadXMExtension(file, (uint32_t)fileSize));
	fastTracksPOCCommitXMExtension();

	/* The first explicit values encountered in the order list become the
	** persistent song-wide setup when reading older pattern-local metadata. */
	assert(fastTracksPOCGetControlTrack(0) == 3);
	assert(fastTracksPOCGetControlTrack(2) == 3);
	assert(fastTracksPOCGetControlTrack(5) == 3);
	assert(fastTracksPOCGetTrackLength(0, 0) == 25);
	assert(fastTracksPOCGetTrackLength(2, 0) == 25);
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

static void setPrivateTransport(fastTracksMode_t mode, bool reversed)
{
	fastTracksRuntimeState_t state;
	memset(&state, 0, sizeof (state));
	state.masterEnabled = true;
	state.tracks[0].mode = mode;
	state.tracks[0].ratioIndex = FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX;
	state.tracks[0].reversed = reversed;
	state.tracks[0].transportStarted = true;
	state.tracks[0].lastTPL = 1;
	fastTracksPOCSetRuntimeState(&state);
}

static fastTracksCrossing_t advanceOneRow(void)
{
	fastTracksCrossing_t crossing;
	assert(fastTracksPOCAdvanceAudio(0, 0, 1, &crossing, 1) == 1);
	return crossing;
}

static void testFastTracksLengthDomainToggle(void)
{
	fastTracksPOCResetAllPatternMetadata();
	patternNumRows[0] = 8;
	fastTracksPOCSetTrackLength(0, 0, 3);
	song.pattNum = 0;
	song.songLength = 1;
	song.orders[0] = 0;

	fastTracksPOCSetUsesTrackLengths(true);
	assert(fastTracksPOCUsesTrackLengths());
	assert(fastTracksPOCGetFastTrackLength(0, 0) == 3);
	setPrivateTransport(FAST_TRACKS_MODE_PATTERN, false);
	fastTracksCrossing_t crossing = advanceOneRow();
	assert(crossing.sourceRow == 1 && !crossing.cycleCompleted);
	crossing = advanceOneRow();
	assert(crossing.sourceRow == 2 && !crossing.cycleCompleted);
	crossing = advanceOneRow();
	assert(crossing.sourceRow == 0 && crossing.cycleCompleted);

	fastTracksPOCSetUsesTrackLengths(false);
	assert(!fastTracksPOCUsesTrackLengths());
	assert(fastTracksPOCGetEffectiveTrackLength(0, 0) == 3);
	assert(fastTracksPOCGetFastTrackLength(0, 0) == 8);
	setPrivateTransport(FAST_TRACKS_MODE_PATTERN, false);
	for (int32_t step = 1; step <= 8; step++)
	{
		crossing = advanceOneRow();
		assert(crossing.sourceRow == step % 8);
		assert(crossing.cycleCompleted == (step == 8));
	}

	fastTracksPOCSetUsesTrackLengths(true);
}

static void testSongModeKeepsSongWideLengthAndExtendsShortPatterns(void)
{
	fastTracksPOCResetAllPatternMetadata();
	patternNumRows[1] = 8;
	patternNumRows[2] = 3;
	fastTracksPOCSetTrackLength(1, 0, 4);
	song.pattNum = 1;
	song.songLength = 2;
	song.orders[0] = 1;
	song.orders[1] = 2;

	fastTracksPOCSetUsesTrackLengths(true);
	setPrivateTransport(FAST_TRACKS_MODE_SONG, false);
	fastTracksCrossing_t crossing = advanceOneRow();
	assert(crossing.sourceOrder == 0 && crossing.sourceRow == 1);
	crossing = advanceOneRow();
	assert(crossing.sourceOrder == 0 && crossing.sourceRow == 2);
	crossing = advanceOneRow();
	assert(crossing.sourceOrder == 0 && crossing.sourceRow == 3);
	crossing = advanceOneRow();
	assert(crossing.sourceOrder == 1 && crossing.sourceRow == 0);
	for (int32_t row = 1; row <= 3; row++)
	{
		crossing = advanceOneRow();
		assert(crossing.sourceOrder == 1 && crossing.sourceRow == row);
	}
	crossing = advanceOneRow();
	assert(crossing.sourceOrder == 0 && crossing.sourceRow == 0);

	setPrivateTransport(FAST_TRACKS_MODE_SONG, true);
	crossing = advanceOneRow();
	assert(crossing.sourceOrder == 1 && crossing.sourceRow == 3); /* blank tail */
	int32_t sourcePattern = -1, sourceRow = -1;
	assert(fastTracksPOCResolveCrossing(0, 0, &crossing,
		&sourcePattern, &sourceRow));
	assert(sourcePattern == 2 && sourceRow == 3);

	fastTracksPOCSetUsesTrackLengths(false);
	setPrivateTransport(FAST_TRACKS_MODE_SONG, false);
	advanceOneRow();
	crossing = advanceOneRow();
	assert(crossing.sourceOrder == 0 && crossing.sourceRow == 2);
	fastTracksPOCSetUsesTrackLengths(true);
}

int main(void)
{
	initializePatternRows();
	fastTracksPOCSetUsesTrackLengths(true);
	testDefaultsBoundsAndCopy();
	testMasterPhaseAndResizeSafety();
	testXMRoundTrip();
	testPatternLocalExtensionMigratesInSongOrder();
	testLogicalCycleCompletionForwardAndReverse();
	testPrimeLengthsAndRatioDurations();
	testFastTracksLengthDomainToggle();
	testSongModeKeepsSongWideLengthAndExtendsShortPatterns();
	puts("Track LEN/CONTROL metadata tests passed.");
	return 0;
}
