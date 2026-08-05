#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/ft2_pattern_launcher.h"
#include "../src/ft2_replayer.h"
#include "../src/ft2_structs.h"

bool songPlaying;
int8_t playMode;
song_t song;
editor_t editor;
note_t *pattern[MAX_PATTERNS];
int16_t patternNumRows[MAX_PATTERNS];

static int32_t stopPlayingCalls;
static int32_t stopPlayingKeepPolyCalls;
static int32_t startPlayingCalls;
static bool polyAudioWork;
static bool polyBoundaryStartSucceeds;
static int32_t polyBoundaryStartCalls;
static int32_t polyCompleteCalls;
static int32_t polyCompleteAtBoundaryCalls;
static bool polyReadyHandoff;
static uint8_t polyReadyPattern;
static bool polyDestinationAvailable[MAX_CHANNELS];

void startPlaying(int8_t mode, int16_t row)
{
	(void)row;
	startPlayingCalls++;
	playMode = mode;
	songPlaying = true;
}

void setNewSongPos(int32_t pos)
{
	song.songPos = (int16_t)pos;
	editor.songPos = (int16_t)pos;
	song.pattNum = song.orders[pos];
}

void stopPlaying(void)
{
	stopPlayingCalls++;
	songPlaying = false;
	playMode = PLAYMODE_IDLE;
}

void stopPlayingKeepPoly(void)
{
	stopPlayingKeepPolyCalls++;
	songPlaying = false;
	playMode = PLAYMODE_IDLE;
}

bool polyMatrixHasAudioWork(void)
{
	return polyAudioWork;
}

bool polyMatrixStartPatternAtBoundary(uint8_t patternNum)
{
	(void)patternNum;
	polyBoundaryStartCalls++;
	return polyBoundaryStartSucceeds;
}

bool polyMatrixClaimReadyQHandoff(uint8_t *patternNum)
{
	if (!polyReadyHandoff)
		return false;

	polyReadyHandoff = false;
	*patternNum = polyReadyPattern;
	return true;
}

void polyMatrixCompleteQHandoff(uint8_t patternNum)
{
	(void)patternNum;
	polyCompleteCalls++;
}

void polyMatrixCompleteQHandoffAtBoundary(uint8_t patternNum)
{
	(void)patternNum;
	polyCompleteAtBoundaryCalls++;
}

bool polyMatrixDestinationAvailableToQ(int32_t destinationChannel,
	int16_t handoffPattern)
{
	(void)handoffPattern;
	return destinationChannel >= 0 && destinationChannel < MAX_CHANNELS &&
		polyDestinationAvailable[destinationChannel];
}

static void resetFixture(void)
{
	memset(&song, 0, sizeof (song));
	memset(&editor, 0, sizeof (editor));
	memset(pattern, 0, sizeof (pattern));
	memset(patternNumRows, 0, sizeof (patternNumRows));
	memset(polyDestinationAvailable, true, sizeof (polyDestinationAvailable));
	patternLauncherSetEnabled(false);
	songPlaying = true;
	playMode = PLAYMODE_PATT;
	song.songLength = 1;
	song.numChannels = 4;
	song.currNumRows = 4;
	patternNumRows[5] = 4;
	patternNumRows[6] = 4;
	patternNumRows[7] = 4;
	stopPlayingCalls = 0;
	stopPlayingKeepPolyCalls = 0;
	startPlayingCalls = 0;
	polyAudioWork = true;
	polyBoundaryStartSucceeds = true;
	polyBoundaryStartCalls = 0;
	polyCompleteCalls = 0;
	polyCompleteAtBoundaryCalls = 0;
	polyReadyHandoff = false;
	polyReadyPattern = 0;
}

static void beginCue(uint8_t patternNum);

static void testCueExitPreservesPolySpools(void)
{
	resetFixture();

	/* Queue and begin the ordinary left-click Matrix cue. */
	patternLauncherRequest(5, false, false);
	assert(patternLauncherHandleBoundary() == PATTERN_LAUNCHER_BOUNDARY_HANDLED);
	assert(patternLauncherGetCurrent() == 5);

	/* Re-click requests the normal end-of-revolution exit. */
	patternLauncherRequest(5, false, false);
	assert(patternLauncherGetExitMode() == PATTERN_LAUNCHER_EXIT_RETURN);
	assert(patternLauncherHandleBoundary() == PATTERN_LAUNCHER_BOUNDARY_STOPPED);

	assert(!songPlaying);
	assert(playMode == PLAYMODE_IDLE);
	assert(stopPlayingCalls == 0);

	handlePatternLauncherStop();
	assert(stopPlayingKeepPolyCalls == 1);
	assert(stopPlayingCalls == 0);
	assert(!songPlaying);
}

static void testCueExitUsesOrdinaryStopWithoutPoly(void)
{
	resetFixture();
	polyAudioWork = false;

	patternLauncherRequest(5, false, false);
	assert(patternLauncherHandleBoundary() == PATTERN_LAUNCHER_BOUNDARY_HANDLED);
	patternLauncherRequest(5, false, false);
	assert(patternLauncherHandleBoundary() == PATTERN_LAUNCHER_BOUNDARY_STOPPED);

	handlePatternLauncherStop();
	assert(stopPlayingCalls == 1);
	assert(stopPlayingKeepPolyCalls == 0);
}

static void testAnyWaitingCueCanBeCancelled(void)
{
	resetFixture();
	beginCue(5);
	patternLauncherRequest(6, false, false);
	patternLauncherRequest(7, false, false);
	assert(patternLauncherGetQueueCount() == 2);

	patternLauncherRequest(6, false, false);
	assert(patternLauncherGetQueueCount() == 1);
	assert(patternLauncherGetQueueItem(0) == 7);
	assert(patternLauncherGetCurrent() == 5);
}

static void testStoppedCueUsesRequestedPatternLength(void)
{
	resetFixture();
	songPlaying = false;
	song.currNumRows = 17;
	patternNumRows[5] = 9;

	patternLauncherRequest(5, false, false);
	assert(song.pattNum == 5);
	assert(song.currNumRows == 9);
	assert(editor.editPattern == 5);
	assert(songPlaying);
}

static void beginCue(uint8_t patternNum)
{
	patternLauncherRequest(patternNum, false, false);
	assert(patternLauncherHandleBoundary() ==
		PATTERN_LAUNCHER_BOUNDARY_HANDLED);
	assert(patternLauncherGetCurrent() == patternNum);
}

static void testQToPolyHandoffStopsOnlyQAtBoundary(void)
{
	resetFixture();
	beginCue(5);

	assert(patternLauncherRequestPolyHandoff(5));
	assert(patternLauncherPolyHandoffIsPending(5));
	assert(patternLauncherHandleBoundary() ==
		PATTERN_LAUNCHER_BOUNDARY_STOPPED);
	assert(polyBoundaryStartCalls == 1);
	assert(!songPlaying);
	assert(patternLauncherGetCurrent() == -1);

	handlePatternLauncherStop();
	assert(stopPlayingKeepPolyCalls == 1);
}

static void testQToPolyHandoffAdvancesExistingQueue(void)
{
	resetFixture();
	beginCue(5);
	assert(patternLauncherRequestPolyHandoff(5));
	patternLauncherRequest(6, false, false);
	assert(patternLauncherGetQueueCount() == 1);
	assert(patternLauncherPolyHandoffIsPending(5));
	assert(patternLauncherHandleBoundary() ==
		PATTERN_LAUNCHER_BOUNDARY_HANDLED);
	assert(polyBoundaryStartCalls == 1);
	assert(patternLauncherGetCurrent() == 6);
	assert(song.pattNum == 6);
	assert(songPlaying);
}

static void testPolyToQHandoffGetsNextQBoundary(void)
{
	resetFixture();
	beginCue(5);
	polyReadyHandoff = true;
	polyReadyPattern = 7;

	handlePolyMatrixQHandoff();
	assert(polyCompleteCalls == 0);
	assert(patternLauncherGetCurrent() == 5);

	assert(patternLauncherHandleBoundary() ==
		PATTERN_LAUNCHER_BOUNDARY_HANDLED);
	assert(patternLauncherGetCurrent() == 7);
	assert(song.pattNum == 7);
	assert(polyCompleteAtBoundaryCalls == 1);
}

static void testPolyToQHandoffStartsQWhenStopped(void)
{
	resetFixture();
	songPlaying = false;
	playMode = PLAYMODE_IDLE;
	patternLauncherSetEnabled(false);
	polyReadyHandoff = true;
	polyReadyPattern = 7;

	handlePolyMatrixQHandoff();
	assert(songPlaying);
	assert(patternLauncherGetCurrent() == 7);
	assert(song.pattNum == 7);
	assert(polyCompleteCalls == 1);
}

static void testCueRoutesAroundPolyOwnedTunnel(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];

	resetFixture();
	memset(source, 0, sizeof (source));
	pattern[5] = source;
	source[0].note = 48;
	polyDestinationAvailable[0] = false;

	beginCue(5);
	assert(!patternLauncherOwnsDestination(0));
	assert(patternLauncherOwnsDestination(1));
	assert(patternLauncherGetSourceForDestination(1) == 0);
}

static void testStopDeckResumesInterruptedSong(void)
{
	resetFixture();
	song.songLength = 4;
	song.songPos = 2;
	song.orders[2] = 6;
	playMode = PLAYMODE_SONG;
	patternLauncherRequest(5, false, false);
	assert(patternLauncherHandleBoundary() == PATTERN_LAUNCHER_BOUNDARY_HANDLED);
	assert(patternLauncherGetCurrent() == 5);
	patternLauncherStopDeckQ();
	assert(!patternLauncherIsEnabled());
	assert(song.songPos == 2);
	assert(playMode == PLAYMODE_SONG);
	assert(songPlaying);
	assert(startPlayingCalls == 1);
}

static void testStopDeckStopsCueStartedFromIdle(void)
{
	resetFixture();
	songPlaying = false;
	playMode = PLAYMODE_IDLE;
	patternLauncherRequest(5, false, false);
	startPlayingCalls = 0;
	patternLauncherStopDeckQ();
	assert(!patternLauncherIsEnabled());
	assert(!songPlaying);
	assert(startPlayingCalls == 0);
	assert(stopPlayingKeepPolyCalls == 1);
}

static void testHardStopCancelsWaitingCueOnly(void)
{
	resetFixture();
	beginCue(5);
	patternLauncherRequest(6, false, false);
	patternLauncherRequest(7, false, false);
	assert(patternLauncherHardStop(6));
	assert(patternLauncherGetCurrent() == 5);
	assert(patternLauncherGetQueueCount() == 1);
	assert(patternLauncherGetQueueItem(0) == 7);
	assert(songPlaying);
}

static void testHardStopPullsActiveQImmediately(void)
{
	resetFixture();
	song.songLength = 4;
	song.songPos = 2;
	song.orders[2] = 6;
	playMode = PLAYMODE_SONG;
	patternLauncherRequest(5, false, false);
	assert(patternLauncherHandleBoundary() ==
		PATTERN_LAUNCHER_BOUNDARY_HANDLED);
	patternLauncherRequest(7, false, false);

	assert(patternLauncherHardStop(5));
	assert(!patternLauncherIsEnabled());
	assert(patternLauncherGetCurrent() == -1);
	assert(patternLauncherGetQueueCount() == 0);
	assert(song.songPos == 2);
	assert(playMode == PLAYMODE_SONG);
	assert(songPlaying);
	assert(startPlayingCalls == 1);
}

int main(void)
{
	testCueExitPreservesPolySpools();
	testCueExitUsesOrdinaryStopWithoutPoly();
	testAnyWaitingCueCanBeCancelled();
	testStoppedCueUsesRequestedPatternLength();
	testQToPolyHandoffStopsOnlyQAtBoundary();
	testQToPolyHandoffAdvancesExistingQueue();
	testPolyToQHandoffGetsNextQBoundary();
	testPolyToQHandoffStartsQWhenStopped();
	testCueRoutesAroundPolyOwnedTunnel();
	testStopDeckResumesInterruptedSong();
	testStopDeckStopsCueStartedFromIdle();
	testHardStopCancelsWaitingCueOnly();
	testHardStopPullsActiveQImmediately();
	puts("13 Pattern Matrix/Poly ownership boundary tests passed.");
	return 0;
}
