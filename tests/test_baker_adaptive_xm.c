#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_baker_adaptive_xm.h"

static bool cellIsEmpty(const bakerAdaptiveXMCell_t *cell)
{
	static const bakerAdaptiveXMCell_t empty;
	return memcmp(cell, &empty, sizeof (*cell)) == 0;
}

static uint8_t rowTPL(const bakerAdaptiveXMCell_t *row, uint8_t channels,
	uint8_t currentTPL)
{
	for (uint8_t channel = 0; channel < channels; channel++)
	{
		if (row[channel].efx == 0x0F && row[channel].efxData > 0 &&
			row[channel].efxData < 0x20)
		{
			return row[channel].efxData;
		}
	}

	return currentTPL;
}

static void testSeparatedEventsAndTimingRows(void)
{
	enum { channels = 4, ticks = 7 };
	bakerAdaptiveXMCell_t source[ticks * channels];
	memset(source, 0, sizeof (source));
	source[0].note = 12;
	source[0].instr = 3;
	source[6 * channels + 3] = (bakerAdaptiveXMCell_t)
		{ 24, 4, 0x31, 0x04, 0x82, 0x4D, 0x12 };
	bakerAdaptiveXMCell_t unchanged[ticks * channels];
	memcpy(unchanged, source, sizeof (source));
	bakerAdaptiveXMCell_t output[ticks * channels];
	memset(output, 0xA5, sizeof (output));
	bakerAdaptiveXMStats_t stats;

	assert(bakerAdaptiveXMBuild(source, ticks, channels, channels - 1, output, ticks,
		&stats) == BAKER_ADAPTIVE_XM_OK);
	assert(memcmp(source, unchanged, sizeof (source)) == 0);
	assert(stats.sourceTicks == ticks && stats.outputRows == 4);
	assert(stats.timingRows == 2 && stats.speedCommands == 2);
	assert(stats.initialTPL == 1);
	assert(memcmp(&output[0], &source[0], channels * sizeof (*source)) == 0);
	assert(output[channels + channels - 1].efx == 0x0F &&
		output[channels + channels - 1].efxData == 4);
	assert(output[2 * channels + channels - 1].efx == 0x0F &&
		output[2 * channels + channels - 1].efxData == 1);
	assert(memcmp(&output[3 * channels], &source[6 * channels],
		channels * sizeof (*source)) == 0);

	for (uint32_t row = 1; row <= 2; row++)
	{
		for (uint8_t channel = 0; channel < channels; channel++)
		{
			if (channel == channels - 1)
				continue;
			assert(cellIsEmpty(&output[row * channels + channel]));
		}
	}
}

static void testTempoAndFullEventRowStayAtTPL1(void)
{
	enum { channels = 32, ticks = 10 };
	bakerAdaptiveXMCell_t source[ticks * channels];
	memset(source, 0, sizeof (source));
	for (uint8_t channel = 0; channel < channels; channel++)
	{
		source[8 * channels + channel].note = (uint8_t)(channel + 1);
		source[8 * channels + channel].efx = 0x04;
		source[8 * channels + channel].efxData = (uint8_t)(0x10 + channel);
	}
	source[8 * channels + 0].efx = 0x0F;
	source[8 * channels + 0].efxData = 0x7D;
	bakerAdaptiveXMCell_t output[ticks * channels];
	bakerAdaptiveXMStats_t stats;
	assert(bakerAdaptiveXMBuild(source, ticks, channels, channels - 1, output, ticks,
		&stats) == BAKER_ADAPTIVE_XM_OK);

	uint32_t expandedTick = 0;
	uint8_t currentTPL = 1;
	bool sawEvent = false;
	for (uint32_t row = 0; row < stats.outputRows; row++)
	{
		currentTPL = rowTPL(&output[row * channels], channels, currentTPL);
		if (memcmp(&output[row * channels], &source[8 * channels],
			channels * sizeof (*source)) == 0)
		{
			assert(expandedTick == 8);
			assert(currentTPL == 1);
			assert(output[row * channels].efxData == 0x7D);
			sawEvent = true;
		}
		expandedTick += currentTPL;
	}
	assert(sawEvent && expandedTick == ticks);
}

static void testDenseTimelineIsByteIdentical(void)
{
	enum { channels = 3, ticks = 64 };
	bakerAdaptiveXMCell_t source[ticks * channels];
	for (uint32_t tick = 0; tick < ticks; tick++)
	{
		for (uint8_t channel = 0; channel < channels; channel++)
		{
			source[tick * channels + channel] = (bakerAdaptiveXMCell_t)
				{ (uint8_t)(1 + tick % 96), (uint8_t)(channel + 1),
				  (uint8_t)(0x10 + channel), 0x04, (uint8_t)tick,
				  (uint8_t)(channel == 1 ? 0x4D : 0),
				  (uint8_t)(channel == 1 ? tick : 0) };
		}
	}
	bakerAdaptiveXMCell_t output[ticks * channels];
	bakerAdaptiveXMStats_t stats;
	assert(bakerAdaptiveXMBuild(source, ticks, channels, channels - 1, output, ticks,
		&stats) == BAKER_ADAPTIVE_XM_OK);
	assert(stats.outputRows == ticks && stats.timingRows == 0 &&
		stats.speedCommands == 0);
	assert(memcmp(source, output, sizeof (source)) == 0);
}

static void testLongSilenceAndExactExpansion(void)
{
	enum { channels = 2, ticks = 71 };
	bakerAdaptiveXMCell_t source[ticks * channels];
	memset(source, 0, sizeof (source));
	source[0].note = 1;
	source[70 * channels + 1].note = 2;
	bakerAdaptiveXMCell_t output[ticks * channels];
	bakerAdaptiveXMStats_t stats;
	assert(bakerAdaptiveXMBuild(source, ticks, channels, channels - 1, output, ticks,
		&stats) == BAKER_ADAPTIVE_XM_OK);

	uint32_t expandedTick = 0;
	uint8_t currentTPL = 1;
	uint32_t eventCount = 0;
	for (uint32_t row = 0; row < stats.outputRows; row++)
	{
		bakerAdaptiveXMCell_t *out = &output[row * channels];
		currentTPL = rowTPL(out, channels, currentTPL);
		if (out[0].note == 1)
		{
			assert(expandedTick == 0 && currentTPL == 1);
			eventCount++;
		}
		if (out[1].note == 2)
		{
			assert(expandedTick == 70 && currentTPL == 1);
			eventCount++;
		}
		assert(currentTPL >= 1 && currentTPL <= 31);
		expandedTick += currentTPL;
	}
	assert(eventCount == 2 && expandedTick == ticks);
}

static void testTransactionalFailuresAndAliasing(void)
{
	enum { channels = 2, ticks = 12 };
	bakerAdaptiveXMCell_t source[ticks * channels];
	memset(source, 0, sizeof (source));
	source[0].note = 1;
	source[11 * channels].note = 2;
	bakerAdaptiveXMCell_t output[ticks * channels];
	memset(output, 0xA5, sizeof (output));
	bakerAdaptiveXMCell_t sentinels[ticks * channels];
	memcpy(sentinels, output, sizeof (output));
	bakerAdaptiveXMStats_t stats;
	assert(bakerAdaptiveXMBuild(source, ticks, channels, channels - 1, output, 2, &stats) ==
		BAKER_ADAPTIVE_XM_CAPACITY);
	assert(stats.outputRows == 4);
	assert(memcmp(output, sentinels, sizeof (output)) == 0);

	bakerAdaptiveXMCell_t sourceSpeed[ticks * channels];
	memcpy(sourceSpeed, source, sizeof (source));
	sourceSpeed[0].efx = 0x0F;
	sourceSpeed[0].efxData = 0x06;
	assert(bakerAdaptiveXMBuild(sourceSpeed, ticks, channels, channels - 1, output, ticks,
		&stats) == BAKER_ADAPTIVE_XM_SOURCE_SPEED);
	assert(memcmp(output, sentinels, sizeof (output)) == 0);

	bakerAdaptiveXMCell_t inPlace[ticks * channels];
	memcpy(inPlace, source, sizeof (source));
	bakerAdaptiveXMCell_t separate[ticks * channels];
	assert(bakerAdaptiveXMBuild(source, ticks, channels, channels - 1, separate, ticks,
		&stats) == BAKER_ADAPTIVE_XM_OK);
	const uint32_t outputRows = stats.outputRows;
	assert(bakerAdaptiveXMBuild(inPlace, ticks, channels, channels - 1, inPlace, ticks,
		&stats) == BAKER_ADAPTIVE_XM_OK);
	assert(stats.outputRows == outputRows);
	assert(memcmp(inPlace, separate,
		(size_t)outputRows * channels * sizeof (*inPlace)) == 0);
}

static void testArgumentsAndMaximums(void)
{
	bakerAdaptiveXMStats_t stats;
	assert(bakerAdaptiveXMBuild(NULL, 0, 1, 0, NULL, 0, &stats) ==
		BAKER_ADAPTIVE_XM_OK);
	assert(stats.outputRows == 0 && stats.initialTPL == 1);
	assert(bakerAdaptiveXMBuild(NULL, 1, 1, 0, NULL, 0, &stats) ==
		BAKER_ADAPTIVE_XM_INVALID_ARGUMENT);
	assert(bakerAdaptiveXMBuild(NULL, 0, 0, 0, NULL, 0, &stats) ==
		BAKER_ADAPTIVE_XM_INVALID_ARGUMENT);
	assert(bakerAdaptiveXMBuild(NULL, 0, 1, 1, NULL, 0, &stats) ==
		BAKER_ADAPTIVE_XM_INVALID_ARGUMENT);

	const uint32_t maximumTicks = UINT32_C(65536);
	bakerAdaptiveXMCell_t *source = calloc(maximumTicks, sizeof (*source));
	bakerAdaptiveXMCell_t *output = calloc(maximumTicks, sizeof (*output));
	assert(source != NULL && output != NULL);
	assert(bakerAdaptiveXMBuild(source, maximumTicks, 1, 0, output,
		maximumTicks, &stats) == BAKER_ADAPTIVE_XM_OK);
	assert(stats.outputRows == 2115 && stats.initialTPL == 1);
	for (uint32_t tick = 0; tick < maximumTicks; tick++)
		source[tick].note = 1;
	assert(bakerAdaptiveXMBuild(source, maximumTicks, 1, 0, output,
		maximumTicks, &stats) == BAKER_ADAPTIVE_XM_OK);
	assert(stats.outputRows == maximumTicks && stats.timingRows == 0);
	assert(memcmp(source, output, maximumTicks * sizeof (*source)) == 0);
	free(output);
	free(source);
}

static void testDeterministicMixedTimelines(void)
{
	enum { channels = 3, maximumTicks = 257 };
	bakerAdaptiveXMCell_t source[maximumTicks * channels];
	bakerAdaptiveXMCell_t output[maximumTicks * channels];
	uint32_t state = UINT32_C(0x6A09E667);

	for (uint32_t ticks = 1; ticks <= maximumTicks; ticks++)
	{
		memset(source, 0, sizeof (source));
		for (uint32_t tick = 0; tick < ticks; tick++)
		{
			state = state * UINT32_C(1664525) + UINT32_C(1013904223);
			if ((state & 7U) != 0)
				continue;

			const uint8_t channel = (uint8_t)((state >> 8) % channels);
			bakerAdaptiveXMCell_t *cell = &source[tick * channels + channel];
			cell->note = (uint8_t)(1 + tick % 96);
			cell->instr = (uint8_t)(1 + channel);
			cell->tuneType = (state & 0x10000U) != 0 ? 0x4D : 0x4E;
			cell->tuneData = (uint8_t)state;
			if ((state & 0x20000U) != 0)
			{
				cell->efx = 0x0F;
				cell->efxData = (uint8_t)(0x20 + (state % 0xE0));
			}
		}

		bakerAdaptiveXMStats_t stats;
		assert(bakerAdaptiveXMBuild(source, ticks, channels, channels - 1, output, ticks,
			&stats) == BAKER_ADAPTIVE_XM_OK);
		uint32_t expandedTick = 0;
		uint8_t currentTPL = 1;
		for (uint32_t row = 0; row < stats.outputRows; row++)
		{
			bakerAdaptiveXMCell_t *out = &output[row * channels];
			const bool sourceEvent = !cellIsEmpty(
				&source[expandedTick * channels]) ||
				!cellIsEmpty(&source[expandedTick * channels + 1]) ||
				!cellIsEmpty(&source[expandedTick * channels + 2]);
			currentTPL = rowTPL(out, channels, currentTPL);
			if (sourceEvent)
			{
				assert(currentTPL == 1);
				assert(memcmp(out, &source[expandedTick * channels],
					channels * sizeof (*out)) == 0);
			}
			else
			{
				for (uint8_t channel = 0; channel < channels; channel++)
				{
					if (channel == channels - 1 && out[channel].efx == 0x0F &&
						out[channel].efxData > 0 && out[channel].efxData < 0x20)
					{
						bakerAdaptiveXMCell_t timing = { 0 };
						timing.efx = 0x0F;
						timing.efxData = out[channel].efxData;
						assert(memcmp(&out[channel], &timing,
							sizeof (timing)) == 0);
					}
					else
					{
						assert(cellIsEmpty(&out[channel]));
					}
				}
			}

			assert(expandedTick + currentTPL <= ticks);
			for (uint32_t skipped = 1; skipped < currentTPL; skipped++)
			{
				for (uint8_t channel = 0; channel < channels; channel++)
					assert(cellIsEmpty(&source[(expandedTick + skipped) *
						channels + channel]));
			}
			expandedTick += currentTPL;
		}
		assert(expandedTick == ticks);
	}
}

int main(void)
{
	testSeparatedEventsAndTimingRows();
	testTempoAndFullEventRowStayAtTPL1();
	testDenseTimelineIsByteIdentical();
	testLongSilenceAndExactExpansion();
	testTransactionalFailuresAndAliasing();
	testArgumentsAndMaximums();
	testDeterministicMixedTimelines();
	puts("Baker adaptive XM emitter tests passed.");
	return 0;
}
