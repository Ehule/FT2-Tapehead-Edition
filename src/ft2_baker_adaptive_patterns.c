#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_baker_adaptive_patterns.h"

static void publishStats(bakerAdaptivePatternStats_t *destination,
	const bakerAdaptivePatternStats_t *source)
{
	if (destination != NULL)
		*destination = *source;
}

void bakerAdaptivePatternSetFree(bakerAdaptivePatternSet_t *patternSet)
{
	if (patternSet == NULL)
		return;

	for (uint32_t pattern = 0; pattern < BAKER_ADAPTIVE_PATTERN_MAX_COUNT;
		pattern++)
	{
		free(patternSet->pattern[pattern]);
	}
	free(patternSet);
}

bakerAdaptivePatternResult_t bakerAdaptivePatternSetBuild(
	const bakerAdaptiveXMCell_t *linearRows, uint32_t linearRowCount,
	uint8_t channels, uint16_t patternRows,
	bakerAdaptivePatternSet_t **destination,
	bakerAdaptivePatternStats_t *stats)
{
	bakerAdaptivePatternStats_t resultStats = { 0 };
	resultStats.inputRows = linearRowCount;

	if (destination == NULL || *destination != NULL || linearRows == NULL ||
		linearRowCount == 0 || channels == 0 ||
		channels > BAKER_ADAPTIVE_XM_MAX_CHANNELS || patternRows == 0 ||
		patternRows > BAKER_ADAPTIVE_PATTERN_MAX_ROWS)
	{
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT;
	}

	const uint32_t requiredPatterns = linearRowCount / patternRows +
		(linearRowCount % patternRows != 0);
	resultStats.requiredPatterns = requiredPatterns;
	resultStats.finalPatternRows = (uint16_t)(linearRowCount % patternRows);
	if (resultStats.finalPatternRows == 0)
		resultStats.finalPatternRows = patternRows;

	if (requiredPatterns > BAKER_ADAPTIVE_PATTERN_MAX_COUNT)
	{
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_PATTERN_CAPACITY;
	}

	if (linearRowCount > SIZE_MAX / channels ||
		(size_t)linearRowCount * channels >
			SIZE_MAX / sizeof (bakerAdaptiveXMCell_t))
	{
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT;
	}

	bakerAdaptivePatternSet_t *candidate = calloc(1, sizeof (*candidate));
	if (candidate == NULL)
	{
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_PATTERN_NO_MEMORY;
	}

	candidate->patternCount = (uint16_t)requiredPatterns;
	candidate->orderCount = (uint16_t)requiredPatterns;
	candidate->nominalPatternRows = patternRows;
	candidate->channels = channels;

	uint32_t sourceRow = 0;
	for (uint32_t pattern = 0; pattern < requiredPatterns; pattern++)
	{
		const uint32_t remainingRows = linearRowCount - sourceRow;
		const uint16_t rows = (uint16_t)(remainingRows > patternRows
			? patternRows : remainingRows);
		const size_t cells = (size_t)rows * channels;

		candidate->orders[pattern] = (uint8_t)pattern;
		candidate->rowCount[pattern] = rows;
		candidate->pattern[pattern] = malloc(cells *
			sizeof (*candidate->pattern[pattern]));
		if (candidate->pattern[pattern] == NULL)
		{
			bakerAdaptivePatternSetFree(candidate);
			publishStats(stats, &resultStats);
			return BAKER_ADAPTIVE_PATTERN_NO_MEMORY;
		}

		memcpy(candidate->pattern[pattern],
			&linearRows[(size_t)sourceRow * channels],
			cells * sizeof (*linearRows));
		sourceRow += rows;
	}

	resultStats.storedCells = linearRowCount * channels;
	*destination = candidate;
	publishStats(stats, &resultStats);
	return BAKER_ADAPTIVE_PATTERN_OK;
}
