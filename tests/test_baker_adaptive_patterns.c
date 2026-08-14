#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_baker_adaptive_patterns.h"

static void assertExactFlattening(const bakerAdaptiveXMCell_t *linearRows,
	uint32_t linearRowCount, uint8_t channels,
	const bakerAdaptivePatternSet_t *patternSet)
{
	uint32_t flattenedRows = 0;
	assert(patternSet->patternCount == patternSet->orderCount);
	for (uint16_t order = 0; order < patternSet->orderCount; order++)
	{
		const uint8_t pattern = patternSet->orders[order];
		assert(pattern == order);
		assert(patternSet->pattern[pattern] != NULL);
		assert(patternSet->rowCount[pattern] >= 1);
		assert(patternSet->rowCount[pattern] <=
			patternSet->nominalPatternRows);
		const size_t cells = (size_t)patternSet->rowCount[pattern] * channels;
		assert(memcmp(patternSet->pattern[pattern],
			&linearRows[(size_t)flattenedRows * channels],
			cells * sizeof (*linearRows)) == 0);
		flattenedRows += patternSet->rowCount[pattern];
	}
	assert(flattenedRows == linearRowCount);

	for (uint16_t pattern = patternSet->patternCount;
		pattern < BAKER_ADAPTIVE_PATTERN_MAX_COUNT; pattern++)
	{
		assert(patternSet->pattern[pattern] == NULL);
		assert(patternSet->rowCount[pattern] == 0);
	}
}

static void fillDistinctRows(bakerAdaptiveXMCell_t *rows, uint32_t rowCount,
	uint8_t channels)
{
	for (uint32_t row = 0; row < rowCount; row++)
	{
		for (uint8_t channel = 0; channel < channels; channel++)
		{
			rows[(size_t)row * channels + channel] = (bakerAdaptiveXMCell_t)
				{ (uint8_t)(1 + row % 96), (uint8_t)(1 + channel),
				  (uint8_t)(0x10 + channel), (uint8_t)(row & 0x0F),
				  (uint8_t)(row + channel),
				  (uint8_t)(channel == 1 ? 0x4D : 0),
				  (uint8_t)(channel == 1 ? row : 0) };
		}
	}
}

static void testExactAndPartialPatterns(void)
{
	enum { channels = 3, rowCount = 33 };
	bakerAdaptiveXMCell_t rows[rowCount * channels];
	fillDistinctRows(rows, rowCount, channels);
	bakerAdaptiveXMCell_t unchanged[rowCount * channels];
	memcpy(unchanged, rows, sizeof (rows));
	bakerAdaptivePatternSet_t *patterns = NULL;
	bakerAdaptivePatternStats_t stats;
	assert(bakerAdaptivePatternSetBuild(rows, rowCount, channels, 16,
		&patterns, &stats) == BAKER_ADAPTIVE_PATTERN_OK);
	assert(patterns != NULL);
	assert(patterns->patternCount == 3 && patterns->orderCount == 3);
	assert(patterns->nominalPatternRows == 16 && patterns->channels == channels);
	assert(patterns->rowCount[0] == 16 && patterns->rowCount[1] == 16 &&
		patterns->rowCount[2] == 1);
	assert(stats.inputRows == rowCount && stats.requiredPatterns == 3);
	assert(stats.finalPatternRows == 1 &&
		stats.storedCells == rowCount * channels);
	assert(memcmp(rows, unchanged, sizeof (rows)) == 0);
	assertExactFlattening(rows, rowCount, channels, patterns);
	bakerAdaptivePatternSetFree(patterns);

	patterns = NULL;
	assert(bakerAdaptivePatternSetBuild(rows, 32, channels, 16, &patterns,
		&stats) == BAKER_ADAPTIVE_PATTERN_OK);
	assert(patterns->patternCount == 2 && patterns->rowCount[1] == 16);
	assert(stats.finalPatternRows == 16);
	assertExactFlattening(rows, 32, channels, patterns);
	bakerAdaptivePatternSetFree(patterns);
}

static void testTimingRowsSurvivePatternBoundaries(void)
{
	enum { channels = 2, ticks = 100 };
	bakerAdaptiveXMCell_t canonical[ticks * channels];
	memset(canonical, 0, sizeof (canonical));
	canonical[0].note = 12;
	canonical[50 * channels + 1].note = 24;
	canonical[50 * channels + 1].tuneType = 0x4E;
	canonical[50 * channels + 1].tuneData = 0x19;
	canonical[99 * channels].note = 36;
	bakerAdaptiveXMCell_t adaptive[ticks * channels];
	bakerAdaptiveXMStats_t emitterStats;
	assert(bakerAdaptiveXMBuild(canonical, ticks, channels, adaptive, ticks,
		&emitterStats) == BAKER_ADAPTIVE_XM_OK);
	assert(emitterStats.outputRows > 4);

	/* Tiny patterns force timing and F01 reset rows across several order
	** boundaries. Packing must remain a byte-exact storage operation. */
	bakerAdaptivePatternSet_t *patterns = NULL;
	bakerAdaptivePatternStats_t stats;
	assert(bakerAdaptivePatternSetBuild(adaptive, emitterStats.outputRows,
		channels, 3, &patterns, &stats) == BAKER_ADAPTIVE_PATTERN_OK);
	assert(patterns->patternCount > 1);
	assertExactFlattening(adaptive, emitterStats.outputRows, channels, patterns);
	bakerAdaptivePatternSetFree(patterns);
}

static void testAllPatternGeometries(void)
{
	static const uint16_t geometry[] = { 1, 16, 32, 64, 128, 256 };
	enum { channels = 4, rowCount = 513 };
	bakerAdaptiveXMCell_t rows[rowCount * channels];
	fillDistinctRows(rows, rowCount, channels);

	for (uint32_t i = 0; i < sizeof (geometry) / sizeof (geometry[0]); i++)
	{
		bakerAdaptivePatternSet_t *patterns = NULL;
		bakerAdaptivePatternStats_t stats;
		const bakerAdaptivePatternResult_t result =
			bakerAdaptivePatternSetBuild(rows, rowCount, channels, geometry[i],
				&patterns, &stats);
		if (geometry[i] == 1)
		{
			assert(result == BAKER_ADAPTIVE_PATTERN_CAPACITY);
			assert(patterns == NULL && stats.requiredPatterns == rowCount);
		}
		else
		{
			assert(result == BAKER_ADAPTIVE_PATTERN_OK);
			assertExactFlattening(rows, rowCount, channels, patterns);
			bakerAdaptivePatternSetFree(patterns);
		}
	}
}

static void testMaximumCapacity(void)
{
	const uint32_t rowCount = UINT32_C(65536);
	const uint8_t channels = 1;
	bakerAdaptiveXMCell_t *rows = malloc(rowCount * sizeof (*rows));
	assert(rows != NULL);
	fillDistinctRows(rows, rowCount, channels);
	bakerAdaptivePatternSet_t *patterns = NULL;
	bakerAdaptivePatternStats_t stats;
	assert(bakerAdaptivePatternSetBuild(rows, rowCount, channels, 256,
		&patterns, &stats) == BAKER_ADAPTIVE_PATTERN_OK);
	assert(patterns->patternCount == 256 && patterns->orderCount == 256);
	assert(patterns->orders[255] == 255 && patterns->rowCount[255] == 256);
	assert(stats.requiredPatterns == 256 && stats.finalPatternRows == 256);
	assertExactFlattening(rows, rowCount, channels, patterns);
	bakerAdaptivePatternSetFree(patterns);

	patterns = NULL;
	assert(bakerAdaptivePatternSetBuild(rows, rowCount, channels, 255,
		&patterns, &stats) == BAKER_ADAPTIVE_PATTERN_CAPACITY);
	assert(patterns == NULL && stats.requiredPatterns == 258);
	free(rows);
}

static void testTransactionalArguments(void)
{
	bakerAdaptiveXMCell_t row = { 1, 2, 3, 4, 5, 0x4D, 6 };
	bakerAdaptivePatternStats_t stats;
	bakerAdaptivePatternSet_t *patterns = NULL;
	assert(bakerAdaptivePatternSetBuild(NULL, 1, 1, 16, &patterns, &stats) ==
		BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT);
	assert(patterns == NULL);
	assert(bakerAdaptivePatternSetBuild(&row, 0, 1, 16, &patterns, &stats) ==
		BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT);
	assert(patterns == NULL);
	assert(bakerAdaptivePatternSetBuild(&row, 1, 0, 16, &patterns, &stats) ==
		BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT);
	assert(patterns == NULL);
	assert(bakerAdaptivePatternSetBuild(&row, 1, 33, 16, &patterns, &stats) ==
		BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT);
	assert(patterns == NULL);
	assert(bakerAdaptivePatternSetBuild(&row, 1, 1, 0, &patterns, &stats) ==
		BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT);
	assert(patterns == NULL);
	assert(bakerAdaptivePatternSetBuild(&row, 1, 1, 257, &patterns, &stats) ==
		BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT);
	assert(patterns == NULL);
	assert(bakerAdaptivePatternSetBuild(&row, 1, 1, 16, NULL, &stats) ==
		BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT);

	assert(bakerAdaptivePatternSetBuild(&row, 1, 1, 16, &patterns, &stats) ==
		BAKER_ADAPTIVE_PATTERN_OK);
	bakerAdaptivePatternSet_t *unchanged = patterns;
	assert(bakerAdaptivePatternSetBuild(&row, 1, 1, 16, &patterns, &stats) ==
		BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT);
	assert(patterns == unchanged);
	bakerAdaptivePatternSetFree(patterns);
	bakerAdaptivePatternSetFree(NULL);
}

int main(void)
{
	testExactAndPartialPatterns();
	testTimingRowsSurvivePatternBoundaries();
	testAllPatternGeometries();
	testMaximumCapacity();
	testTransactionalArguments();
	puts("Baker adaptive XM pattern packing tests passed.");
	return 0;
}
