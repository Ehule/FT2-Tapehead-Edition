#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../src/ft2_fasttracks_core.h"

static void testFirstTickPublishesWithoutCrossing(void)
{
	bool started = false;
	int32_t accumulator = 0;
	uint16_t lastTPL = 0;
	assert(fastTracksClockTick(&started, &accumulator, &lastTPL, 5, 1, 1) == 0);
	assert(started);
	assert(accumulator == 0);
	assert(lastTPL == 1);
}

static void testFiveToOneAtTPL1(void)
{
	bool started = true;
	int32_t accumulator = 0;
	uint16_t lastTPL = 1;
	assert(fastTracksClockTick(&started, &accumulator, &lastTPL, 5, 1, 1) == 5);
	assert(accumulator == 0);
}

static void testFiveToOneAtTPL2KeepsRemainder(void)
{
	bool started = true;
	int32_t accumulator = 0;
	uint16_t lastTPL = 2;
	assert(fastTracksClockTick(&started, &accumulator, &lastTPL, 5, 1, 2) == 2);
	assert(accumulator == 1);
	assert(fastTracksClockTick(&started, &accumulator, &lastTPL, 5, 1, 2) == 3);
	assert(accumulator == 0);
}

static void testSlowerRatioAccumulates(void)
{
	bool started = true;
	int32_t accumulator = 0;
	uint16_t lastTPL = 6;
	int32_t crossings = 0;
	for (int32_t i = 0; i < 12; i++)
		crossings += fastTracksClockTick(&started, &accumulator, &lastTPL, 1, 2, 6);
	assert(crossings == 1);
	assert(accumulator == 0);
}

static void testTPLChangePreservesPhase(void)
{
	bool started = true;
	int32_t accumulator = 5;
	uint16_t lastTPL = 6;
	assert(fastTracksClockTick(&started, &accumulator, &lastTPL, 1, 1, 3) == 1);
	assert(accumulator == 0);
	assert(lastTPL == 3);
}

static void testNaturalSharedBoundaryShortensPhysicalPattern(void)
{
	assert(fastTracksResolveSharedBoundary(true, false, true, 6, 7,
		6, 64, false, false) == FAST_TRACKS_SHARED_BOUNDARY_CONTINUE);
	assert(fastTracksResolveSharedBoundary(true, false, true, 7, 7,
		7, 64, false, false) == FAST_TRACKS_SHARED_BOUNDARY_TRANSITION);
}

static void testLongSharedBoundaryWrapsPhysicalContainer(void)
{
	assert(fastTracksResolveSharedBoundary(true, false, true, 16, 24,
		16, 16, false, false) == FAST_TRACKS_SHARED_BOUNDARY_WRAP_PHYSICAL);
	assert(fastTracksResolveSharedBoundary(true, false, true, 24, 24,
		8, 16, false, false) == FAST_TRACKS_SHARED_BOUNDARY_TRANSITION);
	assert(fastTracksSharedCycleUsesBlankRow(false, 24, 16, 16));
	assert(fastTracksSharedCycleUsesBlankRow(false, 24, 16, 40));
	assert(!fastTracksSharedCycleUsesBlankRow(true, 24, 16, 16));
}

static void testPrivateControlAndBypassAuthority(void)
{
	assert(fastTracksResolveSharedBoundary(true, true, true, 64, 13,
		64, 64, false, false) == FAST_TRACKS_SHARED_BOUNDARY_WRAP_PHYSICAL);
	assert(fastTracksResolveSharedBoundary(true, true, false, 64, 13,
		0, 64, false, true) == FAST_TRACKS_SHARED_BOUNDARY_TRANSITION);
	assert(fastTracksResolveSharedBoundary(false, false, true, 3, 3,
		3, 64, false, false) == FAST_TRACKS_SHARED_BOUNDARY_CONTINUE);
	assert(fastTracksResolveSharedBoundary(false, false, true, 64, 3,
		64, 64, false, false) == FAST_TRACKS_SHARED_BOUNDARY_TRANSITION);
}

static void testExplicitJumpAlwaysWins(void)
{
	assert(fastTracksResolveSharedBoundary(true, true, false, 1, 96,
		1, 16, true, false) == FAST_TRACKS_SHARED_BOUNDARY_TRANSITION);
}

static void testRuntimeBoundaryChangesResolveOnRowEvaluation(void)
{
	/* Shrinking behind the current logical position transitions at the next
	** ordinary row evaluation, never by rewinding inside the current tick. */
	assert(fastTracksResolveSharedBoundary(true, false, false, 38, 16,
		38, 64, false, false) == FAST_TRACKS_SHARED_BOUNDARY_CONTINUE);
	assert(fastTracksResolveSharedBoundary(true, false, true, 39, 16,
		39, 64, false, false) == FAST_TRACKS_SHARED_BOUNDARY_TRANSITION);

	/* Expanding before the old boundary removes that boundary immediately. */
	assert(fastTracksResolveSharedBoundary(true, false, true, 16, 64,
		16, 64, false, false) == FAST_TRACKS_SHARED_BOUNDARY_CONTINUE);
}

int main(void)
{
	testFirstTickPublishesWithoutCrossing();
	testFiveToOneAtTPL1();
	testFiveToOneAtTPL2KeepsRemainder();
	testSlowerRatioAccumulates();
	testTPLChangePreservesPhase();
	testNaturalSharedBoundaryShortensPhysicalPattern();
	testLongSharedBoundaryWrapsPhysicalContainer();
	testPrivateControlAndBypassAuthority();
	testExplicitJumpAlwaysWins();
	testRuntimeBoundaryChangesResolveOnRowEvaluation();
	puts("10 native FasTracks core tests passed.");
	return 0;
}
