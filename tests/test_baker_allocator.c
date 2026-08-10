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
	bakerAudibilityState_t audibility;
	bool relocated;
	bool ordinaryMute[BAKER_ALLOCATOR_CHANNELS] = { false };
	bool performanceMute[BAKER_ALLOCATOR_CHANNELS] = { false };
	bakerTimelinePosition_t position;

	assert(BAKE_PATTERN_ROWS == 256);
	assert(BAKE_MAX_TICKS == 65536);
	assert(BAKE_OUTPUT_TPL == 1);
	assert(bakerEffectIsSourceSpeed(0x0F, 0x01));
	assert(bakerEffectIsSourceSpeed(0x0F, 0x1F));
	assert(!bakerEffectIsSourceSpeed(0x0F, 0x20));
	assert(bakerEffectIsTempo(0x0F, 0x20));
	assert(bakerEffectIsTempo(0x0F, 0xFF));
	assert(!bakerEffectIsTempo(0x0F, 0x1F));
	assert(bakerTimelinePosition(0, &position));
	assert(position.order == 0 && position.pattern == 0 && position.row == 0);
	assert(bakerTimelinePosition(63, &position) && position.row == 63);
	assert(bakerTimelinePosition(64, &position) && position.order == 0 && position.row == 64);
	assert(bakerTimelinePosition(255, &position) && position.order == 0 && position.row == 255);
	assert(bakerTimelinePosition(256, &position) && position.order == 1 && position.row == 0);
	assert(bakerTimelinePosition(65535, &position));
	assert(position.order == 255 && position.pattern == 255 && position.row == 255);
	assert(!bakerTimelinePosition(65536, &position));
	assert(!bakerTimelinePosition(65537, &position));

	/* One call represents one actual playback tick, independent of the source
	** TPL. This is the accumulated timeline used across mid-row speed changes. */
	for (uint16_t tpl = 1; tpl <= 12; tpl += tpl == 1 ? 2 : 3)
	{
		int32_t rows = 0;
		for (uint16_t tick = 0; tick < tpl; tick++)
			rows += bakerTimelineShouldAdvance(true, true, tick);
		assert(rows == tpl);
	}

	/* Effective state is the union of structural mute causes. Releasing one
	** cause cannot reveal a channel still held by the other. */
	ordinaryMute[1] = true;
	performanceMute[2] = true;
	bakerAudibilitySnapshot(&audibility, ordinaryMute, performanceMute, 4);
	assert(bakerChannelIsAudible(&audibility, 0));
	assert(!bakerChannelIsAudible(&audibility, 1));
	assert(!bakerChannelIsAudible(&audibility, 2));
	performanceMute[1] = true;
	ordinaryMute[1] = false;
	assert(bakerAudibilityUpdate(&audibility, ordinaryMute, performanceMute, 4) == 0);
	assert(!bakerChannelIsAudible(&audibility, 1));
	performanceMute[1] = false;
	assert(bakerAudibilityUpdate(&audibility, ordinaryMute, performanceMute, 4) == 0);
	assert(bakerChannelIsAudible(&audibility, 1));
	ordinaryMute[0] = true;
	assert(bakerAudibilityUpdate(&audibility, ordinaryMute, performanceMute, 4) == 1);

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
