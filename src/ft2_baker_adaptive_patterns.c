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

bool bakerAdaptivePatternSetAnchorEmptyPatterns(
	bakerAdaptivePatternSet_t *patternSet, uint8_t initialTPL,
	uint16_t *anchorCount)
{
	if (anchorCount != NULL)
		*anchorCount = 0;
	if (patternSet == NULL || initialTPL == 0 || initialTPL > 31 ||
		patternSet->channels == 0 ||
		patternSet->channels > BAKER_ADAPTIVE_XM_MAX_CHANNELS ||
		patternSet->orderCount == 0 ||
		patternSet->orderCount > BAKER_ADAPTIVE_PATTERN_MAX_COUNT ||
		patternSet->patternCount != patternSet->orderCount)
	{
		return false;
	}

	uint8_t currentTPL = initialTPL;
	uint16_t inserted = 0;
	for (uint16_t order = 0; order < patternSet->orderCount; order++)
	{
		const uint8_t pattern = patternSet->orders[order];
		if (pattern >= patternSet->patternCount ||
			patternSet->pattern[pattern] == NULL ||
			patternSet->rowCount[pattern] == 0 ||
			patternSet->rowCount[pattern] > BAKER_ADAPTIVE_PATTERN_MAX_ROWS)
		{
			return false;
		}

		const size_t cells = (size_t)patternSet->rowCount[pattern] *
			patternSet->channels;
		bool empty = true;
		for (size_t cell = 0; cell < cells; cell++)
		{
			const bakerAdaptiveXMCell_t *event =
				&patternSet->pattern[pattern][cell];
			if (event->note != 0 || event->instr != 0 || event->vol != 0 ||
				event->efx != 0 || event->efxData != 0 ||
				event->tuneType != 0 || event->tuneData != 0)
			{
				empty = false;
			}
			if (event->efx == 0x0F && event->efxData > 0 &&
				event->efxData < 0x20)
			{
				currentTPL = event->efxData;
			}
		}

		if (empty)
		{
			bakerAdaptiveXMCell_t *anchor =
				patternSet->pattern[pattern];
			anchor->efx = 0x0F;
			anchor->efxData = currentTPL;
			inserted++;
		}
	}

	if (anchorCount != NULL)
		*anchorCount = inserted;
	return true;
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
