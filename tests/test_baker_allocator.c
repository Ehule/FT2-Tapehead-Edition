#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "ft2_baker_core.h"
#include "ft2_fasttracks_core.h"

static void testRatioOnTickTimeline(uint8_t numerator, uint8_t denominator,
	uint16_t tpl, int32_t ticks)
{
	bool started = false;
	int32_t accumulator = 0;
	uint16_t lastTPL = 0;
	int32_t bakeRow = -1;
	int32_t previousCrossingRow = -1;
	int32_t actualCrossings = 0;

	for (int32_t tick = 0; tick < ticks; tick++)
	{
		if (bakerTimelineShouldAdvance(true, true, tpl))
			bakeRow++;

		const int32_t crossings = fastTracksClockTick(&started, &accumulator,
			&lastTPL, numerator, denominator, tpl);
		assert(crossings <= 1); /* true for the tested TPL 18 ratio bank */
		if (crossings == 1)
		{
			assert(bakeRow > previousCrossingRow);
			previousCrossingRow = bakeRow;
			actualCrossings++;
		}
	}

	const int32_t expectedCrossings = ((ticks - 1) * numerator) / (denominator * tpl);
	assert(actualCrossings == expectedCrossings);
}

int main(void)
{
	bakerChannelAllocator_t allocator;
	bool relocated;

	bakerChannelAllocatorReset(&allocator, 8);
	assert(bakerChannelAllocatorRoute(&allocator, 2, 0, &relocated) == 2);
	assert(!relocated);

	/* Source channel 2 spills into the first free channel when its preferred
	** cell is already occupied. */
	assert(bakerChannelAllocatorRoute(&allocator, 2, UINT32_C(1) << 2,
		&relocated) == 8);
	assert(relocated);
	assert(bakerChannelAllocatorOwner(&allocator, 8) == 2);

	/* The logical stream remains bound to its spill channel on later rows. */
	assert(bakerChannelAllocatorRoute(&allocator, 2, 0, &relocated) == 8);
	assert(relocated);

	/* If that spill cell is occupied, reuse the source stream's earlier lane
	** rather than claiming another channel. */
	assert(bakerChannelAllocatorRoute(&allocator, 2, UINT32_C(1) << 8,
		&relocated) == 2);
	assert(!relocated);

	/* A 32-channel source has no unowned spill lane and reports an honest
	** allocation failure when its only owned lane is occupied. */
	bakerChannelAllocatorReset(&allocator, 32);
	assert(bakerChannelAllocatorRoute(&allocator, 7, UINT32_C(1) << 7,
		&relocated) == -1);

	/* Ordinary baking advances only at FT2's pre-decrement row boundary. */
	assert(bakerTimelineShouldAdvance(false, true, 1));
	assert(!bakerTimelineShouldAdvance(false, true, 2));
	assert(!bakerTimelineShouldAdvance(false, false, 1));

	/* Fast Tracks and Live baking give every audio/replayer tick a destination
	** row. Ratios such as 5:1 therefore remain representable even when their
	** private crossings happen between master rows. */
	for (uint16_t tick = 1; tick <= 31; tick++)
		assert(bakerTimelineShouldAdvance(true, true, tick));

	/* Exercise the TPL shown in the failing module. Neutral, fractional, mixed
	** and extreme ratios all receive distinct destination rows whenever their
	** private transport crosses a source row. */
	testRatioOnTickTimeline(1, 1, 18, 181);
	testRatioOnTickTimeline(1, 2, 18, 181);
	testRatioOnTickTimeline(17, 16, 18, 181);
	testRatioOnTickTimeline(3, 2, 18, 181);
	testRatioOnTickTimeline(5, 1, 18, 181);

	puts("Baker allocator and timeline tests passed.");
	return 0;
}
