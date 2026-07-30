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

int main(void)
{
	testFirstTickPublishesWithoutCrossing();
	testFiveToOneAtTPL1();
	testFiveToOneAtTPL2KeepsRemainder();
	testSlowerRatioAccumulates();
	testTPLChangePreservesPhase();
	puts("5 native FasTracks core tests passed.");
	return 0;
}
