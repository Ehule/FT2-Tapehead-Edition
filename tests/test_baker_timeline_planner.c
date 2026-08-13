#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_baker_timeline_planner.h"

static void assertExactExpansion(const bool *eventAtTick,
	uint32_t sourceTickCount, const bakerAdaptiveTimelineRow_t *rows,
	uint32_t rowCount)
{
	uint32_t expandedTick = 0;
	uint32_t sourceEvents = 0;
	uint32_t plannedEvents = 0;

	for (uint32_t tick = 0; tick < sourceTickCount; tick++)
		sourceEvents += eventAtTick[tick];

	for (uint32_t row = 0; row < rowCount; row++)
	{
		assert(rows[row].sourceTick == expandedTick);
		assert(rows[row].tpl >= 1 && rows[row].tpl <= BAKER_ADAPTIVE_MAX_TPL);
		assert(rows[row].carriesSourceEvents == eventAtTick[expandedTick]);
		plannedEvents += rows[row].carriesSourceEvents;

		const uint32_t rowEnd = expandedTick + rows[row].tpl;
		assert(rowEnd <= sourceTickCount);
		for (uint32_t tick = expandedTick + 1; tick < rowEnd; tick++)
			assert(!eventAtTick[tick]);

		expandedTick = rowEnd;
	}

	assert(expandedTick == sourceTickCount);
	assert(plannedEvents == sourceEvents);
}

static uint32_t independentlyCountMinimum(const bool *eventAtTick,
	uint32_t sourceTickCount)
{
	uint32_t rows = 0;
	uint32_t boundary = 0;

	while (boundary < sourceTickCount)
	{
		uint32_t nextBoundary = boundary + 1;
		while (nextBoundary < sourceTickCount && !eventAtTick[nextBoundary])
			nextBoundary++;

		const uint32_t distance = (nextBoundary < sourceTickCount
			? nextBoundary : sourceTickCount) - boundary;
		rows += (distance + BAKER_ADAPTIVE_MAX_TPL - 1) /
			BAKER_ADAPTIVE_MAX_TPL;
		boundary += distance;
	}

	return rows;
}

static void testPlan(const bool *eventAtTick, uint32_t sourceTickCount)
{
	const uint32_t required = bakerAdaptiveTimelineRequiredRows(eventAtTick,
		sourceTickCount);
	assert(required == independentlyCountMinimum(eventAtTick, sourceTickCount));

	bakerAdaptiveTimelineRow_t *rows = required > 0
		? malloc(required * sizeof (*rows)) : NULL;
	assert(required == 0 || rows != NULL);

	uint32_t rowCount = UINT32_MAX;
	assert(bakerAdaptiveTimelinePlan(eventAtTick, sourceTickCount, rows,
		required, &rowCount));
	assert(rowCount == required);
	assertExactExpansion(eventAtTick, sourceTickCount, rows, rowCount);
	free(rows);
}

int main(void)
{
	uint32_t rowCount = UINT32_MAX;
	assert(bakerAdaptiveTimelineRequiredRows(NULL, 0) == 0);
	assert(bakerAdaptiveTimelinePlan(NULL, 0, NULL, 0, &rowCount));
	assert(rowCount == 0);
	assert(!bakerAdaptiveTimelinePlan(NULL, 1, NULL, 0, &rowCount));
	assert(rowCount == 0);
	assert(!bakerAdaptiveTimelinePlan(NULL, 0, NULL, 0, NULL));

	bool oneSilentTick[] = { false };
	testPlan(oneSilentTick, 1);

	bool dense[] = { true, true, true, true, true, true };
	bakerAdaptiveTimelineRow_t denseRows[6];
	assert(bakerAdaptiveTimelinePlan(dense, 6, denseRows, 6, &rowCount));
	assert(rowCount == 6);
	for (uint32_t row = 0; row < rowCount; row++)
	{
		assert(denseRows[row].sourceTick == row);
		assert(denseRows[row].tpl == 1);
		assert(denseRows[row].carriesSourceEvents);
	}

	/* A musically irregular event sequence produces the intended adaptive
	** 6, 3, 1, 1, 12 TPL plan without delaying an event inside a row. */
	bool irregular[23] = { false };
	irregular[0] = irregular[6] = irregular[9] = true;
	irregular[10] = irregular[11] = true;
	bakerAdaptiveTimelineRow_t irregularRows[5];
	assert(bakerAdaptiveTimelinePlan(irregular, 23, irregularRows, 5,
		&rowCount));
	assert(rowCount == 5);
	static const uint8_t expectedTPL[] = { 6, 3, 1, 1, 12 };
	for (uint32_t row = 0; row < rowCount; row++)
		assert(irregularRows[row].tpl == expectedTPL[row]);
	assertExactExpansion(irregular, 23, irregularRows, rowCount);

	/* Leading and trailing silence are part of the canonical performance. */
	bool silenceAroundEvents[20] = { false };
	silenceAroundEvents[5] = silenceAroundEvents[7] = true;
	bakerAdaptiveTimelineRow_t silenceRows[3];
	assert(bakerAdaptiveTimelinePlan(silenceAroundEvents, 20, silenceRows, 3,
		&rowCount));
	assert(rowCount == 3);
	assert(silenceRows[0].sourceTick == 0 && silenceRows[0].tpl == 5 &&
		!silenceRows[0].carriesSourceEvents);
	assert(silenceRows[1].sourceTick == 5 && silenceRows[1].tpl == 2 &&
		silenceRows[1].carriesSourceEvents);
	assert(silenceRows[2].sourceTick == 7 && silenceRows[2].tpl == 13 &&
		silenceRows[2].carriesSourceEvents);
	assertExactExpansion(silenceAroundEvents, 20, silenceRows, rowCount);

	/* Gaps beyond F1F become additional empty timing rows. */
	bool longGap[71] = { false };
	longGap[0] = longGap[70] = true;
	bakerAdaptiveTimelineRow_t longGapRows[4];
	assert(bakerAdaptiveTimelinePlan(longGap, 71, longGapRows, 4, &rowCount));
	assert(rowCount == 4);
	assert(longGapRows[0].sourceTick == 0 && longGapRows[0].tpl == 31 &&
		longGapRows[0].carriesSourceEvents);
	assert(longGapRows[1].sourceTick == 31 && longGapRows[1].tpl == 31 &&
		!longGapRows[1].carriesSourceEvents);
	assert(longGapRows[2].sourceTick == 62 && longGapRows[2].tpl == 8 &&
		!longGapRows[2].carriesSourceEvents);
	assert(longGapRows[3].sourceTick == 70 && longGapRows[3].tpl == 1 &&
		longGapRows[3].carriesSourceEvents);
	assertExactExpansion(longGap, 71, longGapRows, rowCount);

	/* Insufficient output space reports the exact requirement and leaves the
	** caller's buffer untouched. */
	bakerAdaptiveTimelineRow_t sentinels[2];
	memset(sentinels, 0xA5, sizeof (sentinels));
	bakerAdaptiveTimelineRow_t unchanged[2];
	memcpy(unchanged, sentinels, sizeof (unchanged));
	assert(!bakerAdaptiveTimelinePlan(silenceAroundEvents, 20, sentinels, 2,
		&rowCount));
	assert(rowCount == 3);
	assert(memcmp(sentinels, unchanged, sizeof (sentinels)) == 0);

	/* Deterministic mixed-density sweeps prove exact expansion and minimum-row
	** planning across many boundary combinations. */
	uint32_t state = UINT32_C(0x13579BDF);
	for (uint32_t length = 1; length <= 257; length++)
	{
		bool events[257] = { false };
		for (uint32_t tick = 0; tick < length; tick++)
		{
			state = state * UINT32_C(1664525) + UINT32_C(1013904223);
			events[tick] = (state & 7U) == 0;
		}
		testPlan(events, length);
	}

	/* At the existing maximum capture length, silence compresses by almost
	** 31:1 while a fully occupied performance remains byte-timeline neutral. */
	const uint32_t maximumTicks = UINT32_C(65536);
	bool *maximum = calloc(maximumTicks, sizeof (*maximum));
	assert(maximum != NULL);
	assert(bakerAdaptiveTimelineRequiredRows(maximum, maximumTicks) == 2115);
	testPlan(maximum, maximumTicks);
	for (uint32_t tick = 0; tick < maximumTicks; tick++)
		maximum[tick] = true;
	assert(bakerAdaptiveTimelineRequiredRows(maximum, maximumTicks) == maximumTicks);
	testPlan(maximum, maximumTicks);
	free(maximum);

	puts("Baker adaptive timeline planner tests passed.");
	return 0;
}
