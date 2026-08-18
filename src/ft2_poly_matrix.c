#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_fasttracks.h"
#include "ft2_pattern_launcher.h"
#include "ft2_poly_matrix.h"

typedef struct polyMatrixThread_t
{
	bool initialPending, waitingAtBoundary;
	uint8_t sourceChannel, destinationChannel;
	int32_t row;
	uint32_t phase;
} polyMatrixThread_t;

typedef struct polyMatrixSpool_t
{
	bool active, stopAtWrap;
	bool qHandoffRequested, qHandoffReady, qHandoffClaimed;
	uint8_t patternNum, threadCount;
	polyMatrixThread_t threads[POLY_MATRIX_MAX_THREADS];
} polyMatrixSpool_t;

static volatile polyMatrixSpool_t spools[POLY_MATRIX_MAX_SPOOLS];
static volatile bool destinationReleasePending[MAX_CHANNELS];

static volatile polyMatrixSpool_t *findPattern(uint8_t patternNum)
{
	for (int32_t i = 0; i < POLY_MATRIX_MAX_SPOOLS; i++)
	{
		if (spools[i].active && spools[i].patternNum == patternNum)
			return &spools[i];
	}

	return NULL;
}

static volatile polyMatrixThread_t *findDestination(int32_t destinationChannel,
	volatile polyMatrixSpool_t **owner)
{
	for (int32_t i = 0; i < POLY_MATRIX_MAX_SPOOLS; i++)
	{
		volatile polyMatrixSpool_t *spool = &spools[i];
		if (!spool->active)
			continue;

		for (uint8_t threadIndex = 0; threadIndex < spool->threadCount;
			threadIndex++)
		{
			volatile polyMatrixThread_t *thread =
				&spool->threads[threadIndex];
			if (thread->destinationChannel == destinationChannel)
			{
				if (owner != NULL)
					*owner = spool;
				return thread;
			}
		}
	}

	return NULL;
}

static bool patternTrackIsPopulated(uint8_t patternNum, uint8_t sourceChannel)
{
	const int32_t numRows = patternNumRows[patternNum];
	const note_t *track = &pattern[patternNum][sourceChannel];
	for (int32_t row = 0; row < numRows; row++, track += MAX_CHANNELS)
	{
		if (track->note != 0 || track->instr != 0 || track->vol != 0 ||
			track->efx != 0 || track->efxData != 0 || track->tuneType != 0 ||
			track->tuneData != 0)
		{
			return true;
		}
	}

	return false;
}

static int32_t collectSourceChannels(uint8_t patternNum,
	uint8_t sourceChannels[POLY_MATRIX_MAX_THREADS])
{
	if (pattern[patternNum] == NULL || patternNumRows[patternNum] <= 0)
		return 0;

	int32_t count = 0;
	for (uint8_t sourceChannel = 0; sourceChannel < song.numChannels;
		sourceChannel++)
	{
		if (!patternTrackIsPopulated(patternNum, sourceChannel))
			continue;

		if (count >= POLY_MATRIX_MAX_THREADS)
			return -1;

		sourceChannels[count++] = sourceChannel;
	}

	return count;
}

static bool destinationIsFree(int32_t destinationChannel,
	const bool reserved[MAX_CHANNELS], bool allowQOverlap)
{
	return destinationChannel >= 0 && destinationChannel < song.numChannels &&
		findDestination(destinationChannel, NULL) == NULL &&
		!destinationReleasePending[destinationChannel] &&
		(allowQOverlap ||
		 !patternLauncherOwnsDestination(destinationChannel)) &&
		!reserved[destinationChannel];
}

static int32_t chooseDestination(uint8_t preferred,
	const bool reserved[MAX_CHANNELS], bool allowQOverlap)
{
	if (destinationIsFree(preferred, reserved, allowQOverlap))
		return preferred;

	/* Wrap forward from the preferred tunnel. Occupied tunnels deliberately
	** make the thread emerge somewhere else. */
	for (int32_t offset = 1; offset < song.numChannels; offset++)
	{
		const int32_t candidate = (preferred + offset) % song.numChannels;
		if (destinationIsFree(candidate, reserved, allowQOverlap))
			return candidate;
	}

	return -1;
}

static volatile polyMatrixSpool_t *findFreeSpool(void)
{
	for (int32_t i = 0; i < POLY_MATRIX_MAX_SPOOLS; i++)
	{
		if (!spools[i].active)
			return &spools[i];
	}

	return NULL;
}

static bool startPatternUnlocked(uint8_t patternNum, bool allowQOverlap)
{
	if (findPattern(patternNum) != NULL)
		return true;

	volatile polyMatrixSpool_t *spool = findFreeSpool();
	if (spool == NULL)
		return false;

	uint8_t sourceChannels[POLY_MATRIX_MAX_THREADS];
	const int32_t sourceCount = collectSourceChannels(patternNum, sourceChannels);
	if (sourceCount <= 0)
		return false;

	bool reserved[MAX_CHANNELS] = { false };
	uint8_t destinations[POLY_MATRIX_MAX_THREADS];
	for (int32_t i = 0; i < sourceCount; i++)
	{
		const int32_t destination =
			chooseDestination(sourceChannels[i], reserved, allowQOverlap);
		if (destination < 0)
			return false;

		destinations[i] = (uint8_t)destination;
		reserved[destination] = true;
	}

	memset((void *)spool, 0, sizeof (*spool));
	spool->patternNum = patternNum;
	spool->threadCount = (uint8_t)sourceCount;
	for (int32_t i = 0; i < sourceCount; i++)
	{
		volatile polyMatrixThread_t *thread = &spool->threads[i];
		thread->sourceChannel = sourceChannels[i];
		thread->destinationChannel = destinations[i];
		thread->row = 0;
		thread->phase = 0;
		thread->initialPending = true;
	}

	/* Publish the complete bundle only after every route is known. */
	spool->active = true;
	return true;
}

static void queueSpoolReleases(volatile polyMatrixSpool_t *spool)
{
	for (uint8_t i = 0; i < spool->threadCount; i++)
		destinationReleasePending[spool->threads[i].destinationChannel] = true;
}

static void removeSpoolUnlocked(volatile polyMatrixSpool_t *spool)
{
	queueSpoolReleases(spool);
	memset((void *)spool, 0, sizeof (*spool));
}

static bool everyThreadIsWaiting(
	const volatile polyMatrixSpool_t *spool)
{
	for (uint8_t i = 0; i < spool->threadCount; i++)
	{
		if (!spool->threads[i].waitingAtBoundary)
			return false;
	}

	return true;
}

static bool spoolContainsSource(const volatile polyMatrixSpool_t *spool,
	int32_t sourceChannel)
{
	for (uint8_t i = 0; i < spool->threadCount; i++)
	{
		if (spool->threads[i].sourceChannel == sourceChannel)
			return true;
	}
	return false;
}

static void holdAllThreadsAtBoundary(volatile polyMatrixSpool_t *spool)
{
	for (uint8_t i = 0; i < spool->threadCount; i++)
		spool->threads[i].waitingAtBoundary = true;
}

bool polyMatrixTogglePattern(uint8_t patternNum, bool immediate)
{
	/* An active voice may always be pulled, but unavailable material can never
	** create a new spool. */
	if (findPattern(patternNum) == NULL &&
		!patternLauncherTileIsLaunchable(patternNum))
		return false;
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	volatile polyMatrixSpool_t *spool = findPattern(patternNum);
	bool result = true;
	if (spool != NULL)
	{
		if (immediate || spool->stopAtWrap)
		{
			removeSpoolUnlocked(spool);
		}
		else
		{
			spool->qHandoffRequested = false;
			spool->qHandoffReady = false;
			spool->qHandoffClaimed = false;
			spool->stopAtWrap = true;
			for (uint8_t i = 0; i < spool->threadCount; i++)
				spool->threads[i].waitingAtBoundary = false;
		}
	}
	else
	{
		result = startPatternUnlocked(patternNum, false);
	}

	if (audioWasntLocked)
		unlockAudio();
	return result;
}

bool polyMatrixSchedulePatternStop(uint8_t patternNum)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	volatile polyMatrixSpool_t *spool = findPattern(patternNum);
	bool changed = false;
	if (spool != NULL)
	{
		changed = !spool->stopAtWrap || spool->qHandoffRequested ||
			spool->qHandoffReady || spool->qHandoffClaimed;
		spool->qHandoffRequested = false;
		spool->qHandoffReady = false;
		spool->qHandoffClaimed = false;
		spool->stopAtWrap = true;
		for (uint8_t i = 0; i < spool->threadCount; i++)
			spool->threads[i].waitingAtBoundary = false;
	}

	if (audioWasntLocked)
		unlockAudio();
	return changed;
}

bool polyMatrixStartPatternAtBoundary(uint8_t patternNum)
{
	/* Called by the replayer at an ordinary Q boundary. The audio callback is
	** already the exclusive writer, so attempting SDL_LockAudioDevice here
	** would deadlock. */
	return startPatternUnlocked(patternNum, true);
}

bool polyMatrixIsPatternActive(uint8_t patternNum)
{
	return findPattern(patternNum) != NULL;
}

bool polyMatrixPatternStopPending(uint8_t patternNum)
{
	const volatile polyMatrixSpool_t *spool = findPattern(patternNum);
	return spool != NULL && spool->stopAtWrap;
}

bool polyMatrixPatternQHandoffPending(uint8_t patternNum)
{
	const volatile polyMatrixSpool_t *spool = findPattern(patternNum);
	return spool != NULL && spool->qHandoffRequested;
}

uint8_t polyMatrixGetPatternSlot(uint8_t patternNum)
{
	for (uint8_t i = 0; i < POLY_MATRIX_MAX_SPOOLS; i++)
	{
		if (spools[i].active && spools[i].patternNum == patternNum)
			return i + 1;
	}

	return 0;
}

int32_t polyMatrixGetDestination(uint8_t patternNum)
{
	const volatile polyMatrixSpool_t *spool = findPattern(patternNum);
	return spool == NULL || spool->threadCount == 0 ?
		-1 : spool->threads[0].destinationChannel;
}

int32_t polyMatrixGetDestinationForSource(uint8_t patternNum,
	uint8_t sourceChannel)
{
	const volatile polyMatrixSpool_t *spool = findPattern(patternNum);
	if (spool == NULL)
		return -1;

	for (uint8_t i = 0; i < spool->threadCount; i++)
	{
		if (spool->threads[i].sourceChannel == sourceChannel)
			return spool->threads[i].destinationChannel;
	}

	return -1;
}

uint8_t polyMatrixGetActiveCount(void)
{
	uint8_t count = 0;
	for (int32_t i = 0; i < POLY_MATRIX_MAX_SPOOLS; i++)
		count += spools[i].active;
	return count;
}

bool polyMatrixRequestQHandoff(uint8_t patternNum)
{
	if (!patternLauncherTileIsLaunchable(patternNum))
		return false;
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	volatile polyMatrixSpool_t *spool = findPattern(patternNum);
	bool accepted = spool != NULL;
	if (accepted)
	{
		/* Q has one foreground lane, so accept only one forced Poly -> Q
		** transfer at a time. */
		for (int32_t i = 0; i < POLY_MATRIX_MAX_SPOOLS; i++)
		{
			if (&spools[i] != spool && spools[i].active &&
				spools[i].qHandoffRequested)
			{
				accepted = false;
				break;
			}
		}
	}

	if (accepted)
	{
		spool->stopAtWrap = false;
		spool->qHandoffRequested = true;
		spool->qHandoffReady = false;
		spool->qHandoffClaimed = false;
		for (uint8_t i = 0; i < spool->threadCount; i++)
			spool->threads[i].waitingAtBoundary = false;
	}

	if (audioWasntLocked)
		unlockAudio();
	return accepted;
}

void polyMatrixCancelQHandoff(uint8_t patternNum)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();
	volatile polyMatrixSpool_t *spool = findPattern(patternNum);
	if (spool != NULL)
	{
		spool->qHandoffRequested = false;
		spool->qHandoffReady = false;
		spool->qHandoffClaimed = false;
		for (uint8_t i = 0; i < spool->threadCount; i++)
			spool->threads[i].waitingAtBoundary = false;
	}
	if (audioWasntLocked)
		unlockAudio();
}

bool polyMatrixClaimReadyQHandoff(uint8_t *patternNum)
{
	if (patternNum == NULL)
		return false;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	bool found = false;
	for (int32_t i = 0; i < POLY_MATRIX_MAX_SPOOLS; i++)
	{
		volatile polyMatrixSpool_t *spool = &spools[i];
		if (spool->active && spool->qHandoffReady &&
			!spool->qHandoffClaimed)
		{
			spool->qHandoffClaimed = true;
			*patternNum = spool->patternNum;
			found = true;
			break;
		}
	}

	if (audioWasntLocked)
		unlockAudio();
	return found;
}

static void completeQHandoffUnlocked(uint8_t patternNum)
{
	volatile polyMatrixSpool_t *spool = findPattern(patternNum);
	if (spool != NULL && spool->qHandoffRequested)
		removeSpoolUnlocked(spool);
}

void polyMatrixCompleteQHandoff(uint8_t patternNum)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	completeQHandoffUnlocked(patternNum);

	if (audioWasntLocked)
		unlockAudio();
}

void polyMatrixCompleteQHandoffAtBoundary(uint8_t patternNum)
{
	completeQHandoffUnlocked(patternNum);
}

void polyMatrixReset(void)
{
	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();

	memset((void *)spools, 0, sizeof (spools));
	memset((void *)destinationReleasePending, 0,
		sizeof (destinationReleasePending));

	if (audioWasntLocked)
		unlockAudio();
}

bool polyMatrixHasAudioWork(void)
{
	if (polyMatrixGetActiveCount() > 0)
		return true;

	for (int32_t i = 0; i < song.numChannels; i++)
	{
		if (destinationReleasePending[i])
			return true;
	}

	return false;
}

bool polyMatrixConsumeDestinationRelease(int32_t destinationChannel)
{
	if (destinationChannel < 0 || destinationChannel >= song.numChannels)
		return false;

	const bool pending = destinationReleasePending[destinationChannel];
	destinationReleasePending[destinationChannel] = false;
	return pending;
}

bool polyMatrixDestinationAvailableToQ(int32_t destinationChannel,
	int16_t handoffPattern)
{
	if (destinationChannel < 0 || destinationChannel >= song.numChannels)
		return false;

	volatile polyMatrixSpool_t *owner = NULL;
	if (findDestination(destinationChannel, &owner) == NULL)
		return !destinationReleasePending[destinationChannel];

	/* During an atomic Poly -> Q handoff, Q may reserve the transferring
	** spool's existing tunnels before Poly publishes their release. */
	return handoffPattern >= 0 && owner != NULL &&
		owner->patternNum == (uint8_t)handoffPattern;
}

void polyMatrixIsolateEventFromMainTransport(note_t *event)
{
	if (event == NULL)
		return;

	/*
	** A Poly spool owns its row traversal. XM position jump, pattern break,
	** pattern loop and pattern delay commands must not mutate the ordinary
	** Pattern/Song transport that may be running underneath it.
	**
	** Fxx remains global because BPM/TPL is the shared performance clock.
	*/
	const bool positionJump = event->efx == 0x0B;
	const bool patternBreak = event->efx == 0x0D;
	const bool extendedTransport =
		event->efx == 0x0E &&
		((event->efxData & 0xF0) == 0x60 ||
		 (event->efxData & 0xF0) == 0xE0);
	if (positionJump || patternBreak || extendedTransport)
	{
		event->efx = 0;
		event->efxData = 0;
	}
}

bool polyMatrixOwnsDestination(int32_t destinationChannel)
{
	return findDestination(destinationChannel, NULL) != NULL;
}

int32_t polyMatrixAdvanceAudio(int32_t destinationChannel, uint16_t tpl,
	const note_t **notes, int32_t maxNotes)
{
	volatile polyMatrixSpool_t *spool = NULL;
	volatile polyMatrixThread_t *thread =
		findDestination(destinationChannel, &spool);
	if (thread == NULL || spool == NULL || notes == NULL || maxNotes <= 0)
		return 0;

	const int32_t numRows = fastTracksPOCGetEffectiveTrackLength(
		spool->patternNum, thread->sourceChannel);
	if (numRows <= 0 || pattern[spool->patternNum] == NULL)
	{
		removeSpoolUnlocked(spool);
		return 0;
	}

	if (thread->waitingAtBoundary)
		return 0;

	int32_t count = 0;
	if (thread->initialPending)
	{
		thread->initialPending = false;
		notes[count++] = &pattern[spool->patternNum]
			[(thread->row * MAX_CHANNELS) + thread->sourceChannel];
	}

	uint32_t numerator = 1;
	uint32_t denominator = 1;
	bool reversed = false;
	if (fastTracksPOCIsEnabled(destinationChannel) &&
		!fastTracksPOCIsClutched(destinationChannel))
	{
		numerator = fastTracksPOCGetRatioNumerator(destinationChannel);
		denominator = fastTracksPOCGetRatioDenominator(destinationChannel);
		reversed = fastTracksPOCIsReversed(destinationChannel);
	}

	if (tpl == 0)
		tpl = 1;

	const uint32_t threshold = denominator * tpl;
	thread->phase += numerator;

	while (thread->phase >= threshold && count < maxNotes)
	{
		thread->phase -= threshold;
		const int32_t previousRow = thread->row;
		thread->row += reversed ? -1 : 1;
		if (thread->row >= numRows)
			thread->row = 0;
		else if (thread->row < 0)
			thread->row = numRows - 1;

		const bool wrapped = (!reversed && thread->row <= previousRow) ||
			(reversed && thread->row >= previousRow);
		if (wrapped && (spool->stopAtWrap || spool->qHandoffRequested))
		{
			const int32_t controlTrack =
				fastTracksPOCGetControlTrack(spool->patternNum);
			const bool controlBoundaryOwned = controlTrack >= 0 &&
				spoolContainsSource(spool, controlTrack);
			if (controlBoundaryOwned && thread->sourceChannel != controlTrack)
			{
				/* This pattern has an audible CONTROL thread, so other local LEN
				** wraps do not quantize an atomic pull or Poly -> Q handoff. */
				notes[count++] = &pattern[spool->patternNum]
					[(thread->row * MAX_CHANNELS) + thread->sourceChannel];
				continue;
			}

			thread->waitingAtBoundary = true;
			if (controlBoundaryOwned || everyThreadIsWaiting(spool))
			{
				if (spool->qHandoffRequested)
				{
					if (controlBoundaryOwned)
						holdAllThreadsAtBoundary(spool);
					spool->qHandoffReady = true;
				}
				else
					removeSpoolUnlocked(spool);
			}
			break;
		}

		notes[count++] = &pattern[spool->patternNum]
			[(thread->row * MAX_CHANNELS) + thread->sourceChannel];
	}

	return count;
}
