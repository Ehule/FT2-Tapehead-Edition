#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ft2_header.h"
#include "ft2_pattern_launcher.h"
#include "ft2_replayer.h"
#include "ft2_structs.h"

/*
** Tapehead Pattern Matrix transport
** ---------------------------------
** Runtime-only, non-destructive pattern selection. This module owns the
** active pattern, queue, exit gesture and saved song position. The replayer
** calls patternLauncherHandleBoundary() only when a pattern boundary occurs.
*/
static volatile bool patternLauncherEnabled;
static volatile int16_t patternLauncherCurrent = -1;
static volatile int16_t patternLauncherQueue[PATTERN_LAUNCHER_QUEUE_MAX] = { -1, -1, -1, -1 };
static volatile uint8_t patternLauncherQueueCount;
static volatile patternLauncherExitMode_t patternLauncherExitMode;
static volatile int16_t patternLauncherSavedSongPos = -1;
static volatile bool patternLauncherStopCleanupPending;

static void clearPatternLauncherQueue(void)
{
	patternLauncherQueueCount = 0;
	for (uint8_t i = 0; i < PATTERN_LAUNCHER_QUEUE_MAX; i++)
		patternLauncherQueue[i] = -1;
}

bool patternLauncherIsEnabled(void)
{
	return patternLauncherEnabled;
}

int16_t patternLauncherGetCurrent(void)
{
	return patternLauncherCurrent;
}

uint8_t patternLauncherGetQueueCount(void)
{
	return patternLauncherQueueCount;
}

int16_t patternLauncherGetQueueItem(uint8_t index)
{
	if (index >= patternLauncherQueueCount)
		return -1;

	return patternLauncherQueue[index];
}

bool patternLauncherStopIsPending(void)
{
	return patternLauncherExitMode == PATTERN_LAUNCHER_EXIT_STOP;
}

uint8_t patternLauncherGetExitMode(void)
{
	return (uint8_t)patternLauncherExitMode;
}

void patternLauncherSetEnabled(bool enabled)
{
	patternLauncherEnabled = enabled;
	if (!enabled)
	{
		patternLauncherCurrent = -1;
		clearPatternLauncherQueue();
		patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_NONE;
		patternLauncherSavedSongPos = -1;
	}
}

void patternLauncherRequest(uint8_t patternNum, bool ctrlPressed, bool shiftPressed)
{
	if (!patternLauncherEnabled)
	{
		patternLauncherSavedSongPos =
			(songPlaying && playMode != PLAYMODE_PATT && playMode != PLAYMODE_RECPATT) ?
			song.songPos : -1;
		patternLauncherEnabled = true;
	}

	/* Re-clicking the newest queued pattern acts as a one-step undo. */
	if (patternLauncherQueueCount > 0 &&
		patternLauncherQueue[patternLauncherQueueCount-1] == patternNum)
	{
		patternLauncherQueueCount--;
		patternLauncherQueue[patternLauncherQueueCount] = -1;
		return;
	}

	if (patternLauncherCurrent == patternNum && patternLauncherQueueCount == 0)
	{
		patternLauncherExitMode_t requestedMode;
		if (ctrlPressed)
			requestedMode = PATTERN_LAUNCHER_EXIT_STOP;
		else if (shiftPressed)
			requestedMode = PATTERN_LAUNCHER_EXIT_CONTINUE;
		else
			requestedMode = PATTERN_LAUNCHER_EXIT_RETURN;

		patternLauncherExitMode = patternLauncherExitMode == requestedMode ?
			PATTERN_LAUNCHER_EXIT_NONE : requestedMode;
		return;
	}

	patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_NONE;

	if (!songPlaying)
	{
		patternLauncherCurrent = patternNum;
		clearPatternLauncherQueue();

		song.pattNum = patternNum;
		editor.editPattern = patternNum;
		startPlaying(PLAYMODE_PATT, 0);
	}
	else if (patternLauncherQueueCount < PATTERN_LAUNCHER_QUEUE_MAX)
	{
		patternLauncherQueue[patternLauncherQueueCount++] = patternNum;
	}
}

patternLauncherBoundaryResult_t patternLauncherHandleBoundary(void)
{
	if (!patternLauncherEnabled)
		return PATTERN_LAUNCHER_BOUNDARY_INACTIVE;

	if (patternLauncherExitMode != PATTERN_LAUNCHER_EXIT_NONE)
	{
		const patternLauncherExitMode_t exitMode = patternLauncherExitMode;
		patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_NONE;
		patternLauncherEnabled = false;
		patternLauncherCurrent = -1;
		clearPatternLauncherQueue();

		if (exitMode == PATTERN_LAUNCHER_EXIT_STOP ||
			patternLauncherSavedSongPos < 0 || song.songLength == 0)
		{
			patternLauncherSavedSongPos = -1;
			patternLauncherStopCleanupPending = true;
			playMode = PLAYMODE_IDLE;
			songPlaying = false;
			return PATTERN_LAUNCHER_BOUNDARY_STOPPED;
		}

		int16_t resumePos = patternLauncherSavedSongPos;
		if (exitMode == PATTERN_LAUNCHER_EXIT_CONTINUE)
		{
			resumePos++;
			if (resumePos >= song.songLength)
				resumePos = song.songLoopStart;
		}

		patternLauncherSavedSongPos = -1;
		song.songPos = (uint8_t)resumePos;
		song.pattNum = song.orders[song.songPos];
		song.currNumRows = patternNumRows[song.pattNum];
	}
	else if (patternLauncherQueueCount > 0)
	{
		patternLauncherCurrent = patternLauncherQueue[0];
		for (uint8_t i = 1; i < patternLauncherQueueCount; i++)
			patternLauncherQueue[i-1] = patternLauncherQueue[i];

		patternLauncherQueueCount--;
		patternLauncherQueue[patternLauncherQueueCount] = -1;
	}

	if (patternLauncherCurrent >= 0)
	{
		song.pattNum = (uint8_t)patternLauncherCurrent;
		song.currNumRows = patternNumRows[song.pattNum];
	}

	return PATTERN_LAUNCHER_BOUNDARY_HANDLED;
}

void handlePatternLauncherStop(void)
{
	if (!patternLauncherStopCleanupPending)
		return;

	patternLauncherStopCleanupPending = false;
	stopPlaying();
}
