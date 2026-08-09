#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ft2_header.h"
#include "ft2_pattern_launcher.h"
#include "ft2_poly_matrix.h"
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
static volatile int16_t patternLauncherPolyHandoffPending = -1;
static volatile int16_t patternLauncherForcedNext = -1;
static volatile bool patternLauncherRouteActive;
static volatile int8_t patternLauncherSourceForDestination[MAX_CHANNELS];
static volatile bool patternLauncherDestinationReleasePending[MAX_CHANNELS];
static bool patternLauncherExposedPatterns[MAX_PATTERNS];
static bool patternLauncherExposureInitialized;
static bool patternTrackIsPopulated(uint8_t patternNum, uint8_t sourceChannel);

bool patternLauncherPatternHasMaterial(uint8_t patternNum)
{
	if (pattern[patternNum] == NULL || patternNumRows[patternNum] <= 0)
		return false;

	for (uint8_t channel = 0; channel < song.numChannels; channel++)
	{
		if (patternTrackIsPopulated(patternNum, channel))
			return true;
	}
	return false;
}

bool patternLauncherPatternIsExposed(uint8_t patternNum)
{
	if (!patternLauncherExposureInitialized)
		patternLauncherResetExposure();
	return patternLauncherExposedPatterns[patternNum];
}

bool patternLauncherTileIsLaunchable(uint8_t patternNum)
{
	return patternLauncherPatternIsExposed(patternNum) &&
		patternLauncherPatternHasMaterial(patternNum);
}

void patternLauncherResetExposure(void)
{
	for (uint16_t i = 0; i < MAX_PATTERNS; i++)
		patternLauncherExposedPatterns[i] = true;
	patternLauncherExposureInitialized = true;
}

void patternLauncherTogglePatternExposure(uint8_t patternNum)
{
	if (!patternLauncherExposureInitialized)
		patternLauncherResetExposure();
	patternLauncherExposedPatterns[patternNum] ^= 1;
	patternLauncherValidatePending();
}

static bool patternTrackIsPopulated(uint8_t patternNum, uint8_t sourceChannel)
{
	if (pattern[patternNum] == NULL || patternNumRows[patternNum] <= 0)
		return false;

	const note_t *track = &pattern[patternNum][sourceChannel];
	for (int32_t row = 0; row < patternNumRows[patternNum];
		row++, track += MAX_CHANNELS)
	{
		if (track->note != 0 || track->instr != 0 || track->vol != 0 ||
			track->efx != 0 || track->efxData != 0)
		{
			return true;
		}
	}

	return false;
}

static bool buildPatternLauncherRoute(uint8_t patternNum,
	int16_t handoffPattern, int8_t sourceForDestination[MAX_CHANNELS])
{
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		sourceForDestination[i] = -1;

	if (!patternLauncherTileIsLaunchable(patternNum))
		return false;

	bool reserved[MAX_CHANNELS] = { false };
	for (uint8_t sourceChannel = 0; sourceChannel < song.numChannels;
		sourceChannel++)
	{
		if (!patternTrackIsPopulated(patternNum, sourceChannel))
			continue;

		int32_t destination = -1;
		for (int32_t offset = 0; offset < song.numChannels; offset++)
		{
			const int32_t candidate =
				(sourceChannel + offset) % song.numChannels;
			if (!reserved[candidate] &&
				polyMatrixDestinationAvailableToQ(candidate, handoffPattern))
			{
				destination = candidate;
				break;
			}
		}

		if (destination < 0)
			return false;

		reserved[destination] = true;
		sourceForDestination[destination] = (int8_t)sourceChannel;
	}

	return true;
}

static bool installPatternLauncherRoute(uint8_t patternNum,
	int16_t handoffPattern, bool releaseUnusedDestinations)
{
	int8_t sourceForDestination[MAX_CHANNELS];
	if (!buildPatternLauncherRoute(patternNum, handoffPattern,
		sourceForDestination))
	{
		return false;
	}

	if (releaseUnusedDestinations && patternLauncherRouteActive)
	{
		for (int32_t destination = 0; destination < song.numChannels;
			destination++)
		{
			if (patternLauncherSourceForDestination[destination] >= 0 &&
				sourceForDestination[destination] < 0)
			{
				patternLauncherDestinationReleasePending[destination] = true;
			}
		}
	}

	/* Publish the complete route only after every source has a tunnel. */
	patternLauncherRouteActive = false;
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		patternLauncherSourceForDestination[i] = sourceForDestination[i];
	patternLauncherRouteActive = true;
	return true;
}

static void clearPatternLauncherRoute(bool clearPendingReleases)
{
	patternLauncherRouteActive = false;
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		patternLauncherSourceForDestination[i] = -1;
		if (clearPendingReleases)
			patternLauncherDestinationReleasePending[i] = false;
	}
}

static void clearPatternLauncherQueue(void)
{
	patternLauncherQueueCount = 0;
	for (uint8_t i = 0; i < PATTERN_LAUNCHER_QUEUE_MAX; i++)
		patternLauncherQueue[i] = -1;
}

void patternLauncherClearQueue(void)
{
	clearPatternLauncherQueue();
	patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_NONE;
}

static void removePatternFromQueue(int16_t patternNum)
{
	uint8_t writeIndex = 0;
	for (uint8_t readIndex = 0; readIndex < patternLauncherQueueCount;
		readIndex++)
	{
		if (patternLauncherQueue[readIndex] != patternNum)
			patternLauncherQueue[writeIndex++] =
				patternLauncherQueue[readIndex];
	}

	for (uint8_t i = writeIndex; i < patternLauncherQueueCount; i++)
		patternLauncherQueue[i] = -1;
	patternLauncherQueueCount = writeIndex;
}

void patternLauncherValidatePending(void)
{
	uint8_t writeIndex = 0;
	for (uint8_t readIndex = 0; readIndex < patternLauncherQueueCount; readIndex++)
	{
		const int16_t item = patternLauncherQueue[readIndex];
		if (item >= 0 && patternLauncherTileIsLaunchable((uint8_t)item))
			patternLauncherQueue[writeIndex++] = item;
	}
	for (uint8_t i = writeIndex; i < patternLauncherQueueCount; i++)
		patternLauncherQueue[i] = -1;
	patternLauncherQueueCount = writeIndex;

	if (patternLauncherForcedNext >= 0 &&
		!patternLauncherTileIsLaunchable((uint8_t)patternLauncherForcedNext))
		patternLauncherForcedNext = -1;
	if (patternLauncherPolyHandoffPending >= 0 &&
		!patternLauncherTileIsLaunchable((uint8_t)patternLauncherPolyHandoffPending))
		patternLauncherPolyHandoffPending = -1;
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
	patternLauncherValidatePending();
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

bool patternLauncherPolyHandoffIsPending(uint8_t patternNum)
{
	return patternLauncherPolyHandoffPending == patternNum;
}

uint8_t patternLauncherGetExitMode(void)
{
	return (uint8_t)patternLauncherExitMode;
}

bool patternLauncherHasRouting(void)
{
	return patternLauncherEnabled && patternLauncherCurrent >= 0 &&
		patternLauncherRouteActive;
}

bool patternLauncherOwnsDestination(int32_t destinationChannel)
{
	return patternLauncherHasRouting() && destinationChannel >= 0 &&
		destinationChannel < song.numChannels &&
		patternLauncherSourceForDestination[destinationChannel] >= 0;
}

int32_t patternLauncherGetSourceForDestination(int32_t destinationChannel)
{
	if (!patternLauncherOwnsDestination(destinationChannel))
		return -1;

	return patternLauncherSourceForDestination[destinationChannel];
}

bool patternLauncherConsumeDestinationRelease(int32_t destinationChannel)
{
	if (destinationChannel < 0 || destinationChannel >= song.numChannels)
		return false;

	const bool pending =
		patternLauncherDestinationReleasePending[destinationChannel];
	patternLauncherDestinationReleasePending[destinationChannel] = false;
	return pending;
}

void patternLauncherSetEnabled(bool enabled)
{
	patternLauncherEnabled = enabled;
	if (!enabled)
	{
		clearPatternLauncherRoute(true);
		patternLauncherCurrent = -1;
		clearPatternLauncherQueue();
		patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_NONE;
		patternLauncherSavedSongPos = -1;
		patternLauncherStopCleanupPending = false;
		patternLauncherPolyHandoffPending = -1;
		patternLauncherForcedNext = -1;
	}
}

void patternLauncherRequest(uint8_t patternNum, bool ctrlPressed, bool shiftPressed)
{
	patternLauncherValidatePending();
	if (!patternLauncherTileIsLaunchable(patternNum))
		return;
	if (!patternLauncherEnabled)
	{
		patternLauncherSavedSongPos =
			(songPlaying && playMode != PLAYMODE_PATT && playMode != PLAYMODE_RECPATT) ?
			song.songPos : -1;

		if (!songPlaying)
		{
			if (!installPatternLauncherRoute(patternNum, -1, false))
				return;

			patternLauncherEnabled = true;
			patternLauncherPolyHandoffPending = -1;
			patternLauncherCurrent = patternNum;
			clearPatternLauncherQueue();

			song.pattNum = patternNum;
			song.currNumRows = patternNumRows[patternNum];
			editor.editPattern = patternNum;
			startPlaying(PLAYMODE_PATT, 0);
			return;
		}

		patternLauncherEnabled = true;
	}

	/* Re-clicking any waiting tile removes that specific queue entry. This is
	** intentionally not limited to the newest item: every visible queue number
	** can be canceled directly from the Matrix. */
	for (uint8_t i = 0; i < patternLauncherQueueCount; i++)
	{
		if (patternLauncherQueue[i] == patternNum)
		{
			removePatternFromQueue(patternNum);
			return;
		}
	}

	if (patternLauncherCurrent == patternNum && patternLauncherQueueCount == 0)
	{
		patternLauncherPolyHandoffPending = -1;
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

	if (patternLauncherQueueCount < PATTERN_LAUNCHER_QUEUE_MAX)
	{
		patternLauncherQueue[patternLauncherQueueCount++] = patternNum;
	}
}

bool patternLauncherHardStop(uint8_t patternNum)
{
	bool found = false;
	for (uint8_t i = 0; i < patternLauncherQueueCount; i++)
	{
		if (patternLauncherQueue[i] == patternNum)
		{
			found = true;
			break;
		}
	}

	if (found)
		removePatternFromQueue(patternNum);

	if (patternLauncherCurrent == patternNum)
	{
		/* A hard stop is deliberately stronger than the yellow/orange/red
		** boundary exits. Pull the complete Q deck immediately, including its
		** waiting queue, then restore the song that Q interrupted when one
		** exists. Poly remains independently owned. */
		patternLauncherStopDeckQ();
		return true;
	}

	return found;
}

bool patternLauncherScheduleStop(uint8_t patternNum)
{
	bool changed = false;
	for (uint8_t i = 0; i < patternLauncherQueueCount; i++)
	{
		if (patternLauncherQueue[i] == patternNum)
		{
			removePatternFromQueue(patternNum);
			changed = true;
			break;
		}
	}

	if (patternLauncherPolyHandoffPending == patternNum)
	{
		patternLauncherPolyHandoffPending = -1;
		changed = true;
	}
	if (patternLauncherForcedNext == patternNum)
	{
		patternLauncherForcedNext = -1;
		changed = true;
	}

	if (patternLauncherEnabled && patternLauncherCurrent == patternNum)
	{
		/* This is an assertion, not a toggle. Repeated Shift+pad presses
		** therefore cannot accidentally re-arm the loop. */
		changed |= patternLauncherExitMode != PATTERN_LAUNCHER_EXIT_STOP;
		patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_STOP;
	}

	return changed;
}

bool patternLauncherRequestPolyHandoff(uint8_t patternNum)
{
	if (!patternLauncherEnabled || patternLauncherCurrent != patternNum)
		return false;

	patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_NONE;
	patternLauncherPolyHandoffPending =
		patternLauncherPolyHandoffPending == patternNum ? -1 : patternNum;
	return true;
}

patternLauncherBoundaryResult_t patternLauncherHandleBoundary(void)
{
	if (!patternLauncherEnabled)
		return PATTERN_LAUNCHER_BOUNDARY_INACTIVE;
	patternLauncherValidatePending();

	/*
	** Middle-clicking the active Q tile transfers it into Poly at Q's own
	** loop boundary. If another Q item is waiting, it becomes foreground;
	** otherwise Q stops while the new Poly bundle keeps the clock alive.
	*/
	if (patternLauncherPolyHandoffPending == patternLauncherCurrent &&
		patternLauncherCurrent >= 0)
	{
		const uint8_t handoffPattern =
			(uint8_t)patternLauncherPolyHandoffPending;
		if (polyMatrixStartPatternAtBoundary(handoffPattern))
		{
			patternLauncherPolyHandoffPending = -1;
			patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_NONE;
			if (patternLauncherQueueCount == 0)
			{
				clearPatternLauncherRoute(false);
				patternLauncherEnabled = false;
				patternLauncherCurrent = -1;
				patternLauncherSavedSongPos = -1;
				patternLauncherStopCleanupPending = true;
				playMode = PLAYMODE_IDLE;
				songPlaying = false;
				return PATTERN_LAUNCHER_BOUNDARY_STOPPED;
			}

			const uint8_t nextPattern =
				(uint8_t)patternLauncherQueue[0];
			if (!installPatternLauncherRoute(nextPattern, -1, false))
			{
				/* The transferred Poly spool keeps playing. Q has no safe
				** tunnel bundle for its next item, so stop Q rather than
				** evicting an established performance layer. */
				clearPatternLauncherRoute(false);
				patternLauncherEnabled = false;
				patternLauncherCurrent = -1;
				clearPatternLauncherQueue();
				patternLauncherSavedSongPos = -1;
				patternLauncherStopCleanupPending = true;
				playMode = PLAYMODE_IDLE;
				songPlaying = false;
				return PATTERN_LAUNCHER_BOUNDARY_STOPPED;
			}

			patternLauncherCurrent = nextPattern;
			for (uint8_t i = 1; i < patternLauncherQueueCount; i++)
				patternLauncherQueue[i-1] = patternLauncherQueue[i];
			patternLauncherQueueCount--;
			patternLauncherQueue[patternLauncherQueueCount] = -1;
		}
	}

	/*
	** Ctrl+Shift+left-click arms Poly -> Q. The Poly bundle waits at its
	** group boundary until Q can accept it at this ordinary boundary.
	*/
	if (patternLauncherForcedNext >= 0)
	{
		const uint8_t handoffPattern = (uint8_t)patternLauncherForcedNext;
		patternLauncherForcedNext = -1;
		patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_NONE;
		if (installPatternLauncherRoute(handoffPattern, handoffPattern, true))
		{
			patternLauncherCurrent = handoffPattern;
			removePatternFromQueue(handoffPattern);
			polyMatrixCompleteQHandoffAtBoundary(handoffPattern);
		}
	}
	else if (patternLauncherExitMode != PATTERN_LAUNCHER_EXIT_NONE)
	{
		const patternLauncherExitMode_t exitMode = patternLauncherExitMode;
		patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_NONE;
		clearPatternLauncherRoute(false);
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
		const uint8_t nextPattern = (uint8_t)patternLauncherQueue[0];
		if (installPatternLauncherRoute(nextPattern, -1, true))
		{
			patternLauncherCurrent = nextPattern;
			for (uint8_t i = 1; i < patternLauncherQueueCount; i++)
				patternLauncherQueue[i-1] = patternLauncherQueue[i];

			patternLauncherQueueCount--;
			patternLauncherQueue[patternLauncherQueueCount] = -1;
		}
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
	if (polyMatrixHasAudioWork())
		stopPlayingKeepPoly();
	else
		stopPlaying();
}

void patternLauncherStopDeckQ(void)
{
	if (!patternLauncherEnabled)
		return;

	const int16_t resumeSongPos = patternLauncherSavedSongPos;
	patternLauncherSetEnabled(false);
	if (resumeSongPos >= 0 && resumeSongPos < song.songLength)
	{
		editor.songPos = resumeSongPos;
		setNewSongPos(resumeSongPos);
		startPlaying(PLAYMODE_SONG, 0);
	}
	else
	{
		stopPlayingKeepPoly();
	}
}

void handlePolyMatrixQHandoff(void)
{
	if (patternLauncherForcedNext >= 0)
		return;

	uint8_t patternNum;
	if (!polyMatrixClaimReadyQHandoff(&patternNum))
		return;
	if (!patternLauncherTileIsLaunchable(patternNum))
	{
		polyMatrixCancelQHandoff(patternNum);
		return;
	}

	if (!songPlaying)
	{
		patternLauncherRequest(patternNum, false, false);
		polyMatrixCompleteQHandoff(patternNum);
		return;
	}

	if (!patternLauncherEnabled)
	{
		patternLauncherSavedSongPos =
			(playMode != PLAYMODE_PATT && playMode != PLAYMODE_RECPATT) ?
			song.songPos : -1;
		patternLauncherEnabled = true;
	}

	patternLauncherExitMode = PATTERN_LAUNCHER_EXIT_NONE;
	patternLauncherPolyHandoffPending = -1;
	patternLauncherForcedNext = patternNum;
}
