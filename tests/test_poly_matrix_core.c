#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/ft2_audio.h"
#include "../src/ft2_poly_matrix.h"

audio_t audio;
song_t song;
note_t *pattern[MAX_PATTERNS];
int16_t patternNumRows[MAX_PATTERNS];

static bool fastTrackEnabled[MAX_CHANNELS];
static bool fastTrackClutched[MAX_CHANNELS];
static bool fastTrackReversed[MAX_CHANNELS];
static uint8_t fastTrackNumerator[MAX_CHANNELS];
static uint8_t fastTrackDenominator[MAX_CHANNELS];
static bool qOwnedDestination[MAX_CHANNELS];
static bool patternExposed[MAX_PATTERNS];
static uint16_t trackLength[MAX_PATTERNS][MAX_CHANNELS];
static int8_t controlTrack[MAX_PATTERNS];

uint16_t fastTracksPOCGetEffectiveTrackLength(uint16_t patternNumber,
	int32_t channelIndex)
{
	const uint16_t storedLength = trackLength[patternNumber][channelIndex];
	if (storedLength == 0 || storedLength > patternNumRows[patternNumber])
		return patternNumRows[patternNumber];
	return storedLength;
}

int8_t fastTracksPOCGetControlTrack(uint16_t patternNumber)
{
	return controlTrack[patternNumber];
}

bool patternLauncherTileIsLaunchable(uint8_t patternNum)
{
	if (!patternExposed[patternNum] || pattern[patternNum] == NULL)
		return false;
	for (int32_t row = 0; row < patternNumRows[patternNum]; row++)
	{
		for (uint8_t channel = 0; channel < song.numChannels; channel++)
		{
			const note_t *event = &pattern[patternNum][row * MAX_CHANNELS + channel];
			if (event->note || event->instr || event->vol || event->efx || event->efxData)
				return true;
		}
	}
	return false;
}

bool patternLauncherOwnsDestination(int32_t destinationChannel)
{
	return destinationChannel >= 0 && destinationChannel < MAX_CHANNELS &&
		qOwnedDestination[destinationChannel];
}

void lockAudio(void)
{
	audio.locked = true;
}

void unlockAudio(void)
{
	audio.locked = false;
}

bool fastTracksPOCIsEnabled(int32_t channelIndex)
{
	return fastTrackEnabled[channelIndex];
}

bool fastTracksPOCIsClutched(int32_t channelIndex)
{
	return fastTrackClutched[channelIndex];
}

bool fastTracksPOCIsReversed(int32_t channelIndex)
{
	return fastTrackReversed[channelIndex];
}

uint8_t fastTracksPOCGetRatioNumerator(int32_t channelIndex)
{
	return fastTrackNumerator[channelIndex];
}

uint8_t fastTracksPOCGetRatioDenominator(int32_t channelIndex)
{
	return fastTrackDenominator[channelIndex];
}

static void resetFixture(void)
{
	memset(&audio, 0, sizeof (audio));
	memset(&song, 0, sizeof (song));
	memset(pattern, 0, sizeof (pattern));
	memset(patternNumRows, 0, sizeof (patternNumRows));
	memset(fastTrackEnabled, 0, sizeof (fastTrackEnabled));
	memset(fastTrackClutched, 0, sizeof (fastTrackClutched));
	memset(fastTrackReversed, 0, sizeof (fastTrackReversed));
	memset(qOwnedDestination, 0, sizeof (qOwnedDestination));
	memset(patternExposed, 1, sizeof (patternExposed));
	memset(trackLength, 0, sizeof (trackLength));
	memset(controlTrack, -1, sizeof (controlTrack));

	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		fastTrackNumerator[i] = 1;
		fastTrackDenominator[i] = 1;
	}

	song.numChannels = 4;
	polyMatrixReset();
}

static void testControlTrackOwnsGracefulBoundary(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];
	const note_t *events[FAST_TRACKS_MAX_CROSSINGS_PER_TICK];

	resetFixture();
	memset(source, 0, sizeof (source));
	pattern[26] = source;
	patternNumRows[26] = 4;
	trackLength[26][0] = 2;
	controlTrack[26] = 0;
	source[0].note = 48;
	source[1].note = 52;

	assert(polyMatrixTogglePattern(26, false));
	assert(polyMatrixAdvanceAudio(0, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 2);
	assert(polyMatrixAdvanceAudio(1, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 2);
	assert(polyMatrixTogglePattern(26, false));

	assert(polyMatrixAdvanceAudio(0, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 0);
	assert(!polyMatrixIsPatternActive(26));
	assert(polyMatrixConsumeDestinationRelease(0));
	assert(polyMatrixConsumeDestinationRelease(1));
}

static void testIndependentTrackLengthWrap(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];
	const note_t *events[FAST_TRACKS_MAX_CROSSINGS_PER_TICK];

	resetFixture();
	memset(source, 0, sizeof (source));
	pattern[25] = source;
	patternNumRows[25] = 4;
	trackLength[25][0] = 2;
	source[(0 * MAX_CHANNELS) + 0].note = 48;
	source[(1 * MAX_CHANNELS) + 0].note = 49;
	source[(2 * MAX_CHANNELS) + 0].note = 50;

	assert(polyMatrixTogglePattern(25, false));
	assert(polyMatrixAdvanceAudio(0, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 2);
	assert(events[0]->note == 48);
	assert(events[1]->note == 49);
	assert(polyMatrixAdvanceAudio(0, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 1);
	assert(events[0]->note == 48);
}

static void testInitialRowAndOneToOneClock(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];
	const note_t *events[FAST_TRACKS_MAX_CROSSINGS_PER_TICK];

	resetFixture();
	memset(source, 0, sizeof (source));
	pattern[7] = source;
	patternNumRows[7] = 4;
	source[(0 * MAX_CHANNELS) + 1].note = 48;
	source[(1 * MAX_CHANNELS) + 1].note = 49;

	assert(polyMatrixTogglePattern(7, false));
	assert(polyMatrixGetDestination(7) == 1);
	assert(polyMatrixAdvanceAudio(1, 6, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 1);
	assert(events[0]->note == 48);

	for (int32_t i = 0; i < 4; i++)
		assert(polyMatrixAdvanceAudio(1, 6, events,
			FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 0);

	assert(polyMatrixAdvanceAudio(1, 6, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 1);
	assert(events[0]->note == 49);
}

static void testOccupiedTunnelWrapsForward(void)
{
	static note_t sourceA[MAX_PATT_LEN * MAX_CHANNELS];
	static note_t sourceB[MAX_PATT_LEN * MAX_CHANNELS];

	resetFixture();
	pattern[8] = sourceA;
	pattern[9] = sourceB;
	patternNumRows[8] = patternNumRows[9] = 4;
	sourceA[1].note = 48;
	sourceB[1].note = 49;

	assert(polyMatrixTogglePattern(8, false));
	assert(polyMatrixTogglePattern(9, false));
	assert(polyMatrixGetDestination(8) == 1);
	assert(polyMatrixGetDestination(9) == 2);
}

static void testQOwnedTunnelWrapsForward(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];

	resetFixture();
	memset(source, 0, sizeof (source));
	pattern[18] = source;
	patternNumRows[18] = 4;
	source[0].note = 48;
	qOwnedDestination[0] = true;

	assert(polyMatrixTogglePattern(18, false));
	assert(polyMatrixGetDestinationForSource(18, 0) == 1);
}

static void testPolySlotLabelsRemainStable(void)
{
	static note_t sources[5][MAX_PATT_LEN * MAX_CHANNELS];

	resetFixture();
	for (uint8_t i = 0; i < 5; i++)
	{
		const uint8_t patternNum = (uint8_t)(20 + i);
		memset(sources[i], 0, sizeof (sources[i]));
		pattern[patternNum] = sources[i];
		patternNumRows[patternNum] = 4;
		sources[i][0].note = (uint8_t)(48 + i);
	}

	assert(polyMatrixTogglePattern(20, false));
	assert(polyMatrixTogglePattern(21, false));
	assert(polyMatrixTogglePattern(22, false));
	assert(polyMatrixTogglePattern(23, false));
	assert(polyMatrixGetPatternSlot(20) == 1);
	assert(polyMatrixGetPatternSlot(21) == 2);
	assert(polyMatrixGetPatternSlot(22) == 3);
	assert(polyMatrixGetPatternSlot(23) == 4);

	/* Removing one spool doesn't renumber the others. The next throw reuses
	** the free visual slot. */
	assert(polyMatrixTogglePattern(21, true));
	assert(polyMatrixGetPatternSlot(21) == 0);
	assert(polyMatrixGetPatternSlot(22) == 3);
	assert(polyMatrixConsumeDestinationRelease(1));
	assert(polyMatrixTogglePattern(24, false));
	assert(polyMatrixGetPatternSlot(24) == 2);
}

static void testGracefulPullStopsAtWrap(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];
	const note_t *events[FAST_TRACKS_MAX_CROSSINGS_PER_TICK];

	resetFixture();
	pattern[10] = source;
	patternNumRows[10] = 2;
	source[0].note = 48;

	assert(polyMatrixTogglePattern(10, false));
	assert(polyMatrixAdvanceAudio(0, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 2);
	assert(polyMatrixTogglePattern(10, false));
	assert(polyMatrixPatternStopPending(10));
	assert(polyMatrixAdvanceAudio(0, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 0);
	assert(!polyMatrixIsPatternActive(10));
}

static void testImmediatePullQueuesDestinationRelease(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];

	resetFixture();
	pattern[11] = source;
	patternNumRows[11] = 4;
	source[0].note = 48;
	assert(polyMatrixTogglePattern(11, false));
	assert(polyMatrixHasAudioWork());

	assert(polyMatrixTogglePattern(11, true));
	assert(!polyMatrixIsPatternActive(11));
	assert(polyMatrixHasAudioWork());
	assert(polyMatrixConsumeDestinationRelease(0));
	assert(!polyMatrixConsumeDestinationRelease(0));
	assert(!polyMatrixHasAudioWork());
}

static void testGracefulPullQueuesDestinationRelease(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];
	const note_t *events[FAST_TRACKS_MAX_CROSSINGS_PER_TICK];

	resetFixture();
	pattern[12] = source;
	patternNumRows[12] = 2;
	source[1].note = 48;

	assert(polyMatrixTogglePattern(12, false));
	assert(polyMatrixAdvanceAudio(1, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 2);
	assert(polyMatrixTogglePattern(12, false));
	assert(polyMatrixAdvanceAudio(1, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 0);
	assert(!polyMatrixIsPatternActive(12));
	assert(polyMatrixConsumeDestinationRelease(1));
}

static void testAutomaticBundleRoutingIgnoresCursor(void)
{
	static note_t sourceA[MAX_PATT_LEN * MAX_CHANNELS];
	static note_t sourceB[MAX_PATT_LEN * MAX_CHANNELS];

	resetFixture();
	song.numChannels = 6;
	memset(sourceA, 0, sizeof (sourceA));
	memset(sourceB, 0, sizeof (sourceB));
	pattern[13] = sourceA;
	pattern[14] = sourceB;
	patternNumRows[13] = patternNumRows[14] = 4;

	/* Note data and control-only data both count as populated threads. */
	sourceA[(0 * MAX_CHANNELS) + 0].note = 48;
	sourceA[(3 * MAX_CHANNELS) + 2].efx = 0x0F;
	sourceA[(3 * MAX_CHANNELS) + 2].efxData = 0x7D;
	sourceB[(0 * MAX_CHANNELS) + 0].note = 52;

	assert(polyMatrixTogglePattern(13, false));
	assert(polyMatrixGetDestinationForSource(13, 0) == 0);
	assert(polyMatrixGetDestinationForSource(13, 1) == -1);
	assert(polyMatrixGetDestinationForSource(13, 2) == 2);

	/* Pattern 13 owns tunnel 0, so Pattern 14's Track 1 wraps to tunnel 1. */
	assert(polyMatrixTogglePattern(14, false));
	assert(polyMatrixGetDestinationForSource(14, 0) == 1);
}

static void testBundleGracefulPullIsAtomic(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];
	const note_t *events[FAST_TRACKS_MAX_CROSSINGS_PER_TICK];

	resetFixture();
	memset(source, 0, sizeof (source));
	pattern[15] = source;
	patternNumRows[15] = 2;
	source[0].note = 48;
	source[1].note = 52;

	fastTrackEnabled[1] = true;
	fastTrackNumerator[1] = 1;
	fastTrackDenominator[1] = 2;

	assert(polyMatrixTogglePattern(15, false));
	assert(polyMatrixAdvanceAudio(0, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 2);
	assert(polyMatrixAdvanceAudio(1, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 1);
	assert(polyMatrixTogglePattern(15, false));

	/* Fast tunnel 0 reaches its boundary first, but the bundle stays owned. */
	assert(polyMatrixAdvanceAudio(0, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 0);
	assert(polyMatrixIsPatternActive(15));

	for (int32_t i = 0; i < 3; i++)
		polyMatrixAdvanceAudio(1, 1, events,
			FAST_TRACKS_MAX_CROSSINGS_PER_TICK);
	assert(!polyMatrixIsPatternActive(15));
	assert(polyMatrixConsumeDestinationRelease(0));
	assert(polyMatrixConsumeDestinationRelease(1));
}

static void testPolyToQHandoffWaitsForWholeBundle(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];
	const note_t *events[FAST_TRACKS_MAX_CROSSINGS_PER_TICK];
	uint8_t handoffPattern = 0;

	resetFixture();
	memset(source, 0, sizeof (source));
	pattern[16] = source;
	patternNumRows[16] = 2;
	source[0].note = 48;
	source[1].note = 52;

	assert(polyMatrixTogglePattern(16, false));
	assert(polyMatrixAdvanceAudio(0, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 2);
	assert(polyMatrixAdvanceAudio(1, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 2);
	assert(polyMatrixRequestQHandoff(16));
	assert(polyMatrixPatternQHandoffPending(16));

	assert(polyMatrixAdvanceAudio(0, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 0);
	assert(!polyMatrixClaimReadyQHandoff(&handoffPattern));
	assert(polyMatrixAdvanceAudio(1, 1, events,
		FAST_TRACKS_MAX_CROSSINGS_PER_TICK) == 0);
	assert(polyMatrixClaimReadyQHandoff(&handoffPattern));
	assert(handoffPattern == 16);

	/* The Poly bundle holds its tunnels until Q accepts the transfer. */
	assert(polyMatrixIsPatternActive(16));
	polyMatrixCompleteQHandoff(16);
	assert(!polyMatrixIsPatternActive(16));
}

static void testMoreThanEightThreadsIsRejected(void)
{
	static note_t source[MAX_PATT_LEN * MAX_CHANNELS];

	resetFixture();
	song.numChannels = 10;
	memset(source, 0, sizeof (source));
	pattern[17] = source;
	patternNumRows[17] = 4;
	for (int32_t channelIndex = 0; channelIndex < 9; channelIndex++)
		source[channelIndex].note = (uint8_t)(48 + channelIndex);

	assert(!polyMatrixTogglePattern(17, false));
	assert(!polyMatrixIsPatternActive(17));
}

static void testPolyEventsCannotSteerMainTransport(void)
{
	note_t event;
	memset(&event, 0, sizeof (event));
	event.note = 48;
	event.instr = 1;

	const uint8_t isolatedEffects[] = { 0x0B, 0x0D };
	for (uint8_t i = 0; i < sizeof (isolatedEffects); i++)
	{
		event.efx = isolatedEffects[i];
		event.efxData = 0x12;
		polyMatrixIsolateEventFromMainTransport(&event);
		assert(event.note == 48);
		assert(event.instr == 1);
		assert(event.efx == 0);
		assert(event.efxData == 0);
	}

	event.efx = 0x0E;
	event.efxData = 0x63;
	polyMatrixIsolateEventFromMainTransport(&event);
	assert(event.efx == 0);

	event.efx = 0x0E;
	event.efxData = 0xE2;
	polyMatrixIsolateEventFromMainTransport(&event);
	assert(event.efx == 0);

	/* Shared tempo and ordinary per-channel effects remain intact. */
	event.efx = 0x0F;
	event.efxData = 0x7D;
	polyMatrixIsolateEventFromMainTransport(&event);
	assert(event.efx == 0x0F);
	assert(event.efxData == 0x7D);
}

static void testUnavailablePatternsCannotStartPoly(void)
{
	static note_t source[4 * MAX_CHANNELS];
	resetFixture();
	memset(source, 0, sizeof (source));
	pattern[23] = source;
	patternNumRows[23] = 4;
	assert(!polyMatrixTogglePattern(23, false));
	assert(!polyMatrixIsPatternActive(23));

	source[0].note = 48;
	patternExposed[23] = false;
	assert(!polyMatrixTogglePattern(23, false));
	assert(!polyMatrixIsPatternActive(23));

	patternExposed[23] = true;
	assert(polyMatrixTogglePattern(23, false));
	assert(polyMatrixIsPatternActive(23));
	patternExposed[23] = false;
	assert(polyMatrixTogglePattern(23, true));
	assert(!polyMatrixIsPatternActive(23));
}

int main(void)
{
	testInitialRowAndOneToOneClock();
	testIndependentTrackLengthWrap();
	testControlTrackOwnsGracefulBoundary();
	testOccupiedTunnelWrapsForward();
	testQOwnedTunnelWrapsForward();
	testPolySlotLabelsRemainStable();
	testGracefulPullStopsAtWrap();
	testImmediatePullQueuesDestinationRelease();
	testGracefulPullQueuesDestinationRelease();
	testAutomaticBundleRoutingIgnoresCursor();
	testBundleGracefulPullIsAtomic();
	testPolyToQHandoffWaitsForWholeBundle();
	testMoreThanEightThreadsIsRejected();
	testPolyEventsCannotSteerMainTransport();
	testUnavailablePatternsCannotStartPoly();
	puts("15 native Poly Matrix core tests passed.");
	return 0;
}
