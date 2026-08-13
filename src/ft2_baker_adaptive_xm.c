#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_baker_adaptive_xm.h"
#include "ft2_baker_timeline_planner.h"

static bool cellIsEmpty(const bakerAdaptiveXMCell_t *cell)
{
	return cell->note == 0 && cell->instr == 0 && cell->vol == 0 &&
		cell->efx == 0 && cell->efxData == 0 && cell->tuneType == 0 &&
		cell->tuneData == 0;
}

static bool cellIsSourceSpeed(const bakerAdaptiveXMCell_t *cell)
{
	return cell->efx == 0x0F && cell->efxData > 0 && cell->efxData < 0x20;
}

static void publishStats(bakerAdaptiveXMStats_t *destination,
	const bakerAdaptiveXMStats_t *source)
{
	if (destination != NULL)
		*destination = *source;
}

bakerAdaptiveXMResult_t bakerAdaptiveXMBuild(
	const bakerAdaptiveXMCell_t *source, uint32_t sourceTickCount,
	uint8_t channels, bakerAdaptiveXMCell_t *destination,
	uint32_t rowCapacity, bakerAdaptiveXMStats_t *stats)
{
	bakerAdaptiveXMStats_t resultStats = { 0 };
	resultStats.sourceTicks = sourceTickCount;
	resultStats.initialTPL = 1;

	if (channels == 0 || channels > BAKER_ADAPTIVE_XM_MAX_CHANNELS)
	{
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_XM_INVALID_ARGUMENT;
	}

	if (sourceTickCount == 0)
	{
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_XM_OK;
	}

	if (source == NULL || destination == NULL ||
		sourceTickCount > SIZE_MAX / channels ||
		(size_t)sourceTickCount * channels >
			SIZE_MAX / sizeof (bakerAdaptiveXMCell_t))
	{
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_XM_INVALID_ARGUMENT;
	}

	bool *eventAtTick = calloc(sourceTickCount, sizeof (*eventAtTick));
	bool *safeBoundary = calloc(sourceTickCount, sizeof (*safeBoundary));
	if (eventAtTick == NULL || safeBoundary == NULL)
	{
		free(safeBoundary);
		free(eventAtTick);
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_XM_NO_MEMORY;
	}

	for (uint32_t tick = 0; tick < sourceTickCount; tick++)
	{
		const bakerAdaptiveXMCell_t *row = &source[(size_t)tick * channels];
		for (uint8_t channel = 0; channel < channels; channel++)
		{
			if (cellIsSourceSpeed(&row[channel]))
			{
				free(safeBoundary);
				free(eventAtTick);
				publishStats(stats, &resultStats);
				return BAKER_ADAPTIVE_XM_SOURCE_SPEED;
			}

			if (!cellIsEmpty(&row[channel]))
				eventAtTick[tick] = true;
		}

		if (!eventAtTick[tick])
			continue;

		/* The event tick and the following boundary keep its row at TPL 1.
		** The preceding boundary creates a one-tick empty reset row so the
		** current speed is also 1 before the event is read. */
		safeBoundary[tick] = true;
		if (tick > 0)
			safeBoundary[tick - 1] = true;
		if (tick + 1 < sourceTickCount)
			safeBoundary[tick + 1] = true;
	}

	const uint32_t requiredRows = bakerAdaptiveTimelineRequiredRows(
		safeBoundary, sourceTickCount);
	resultStats.outputRows = requiredRows;
	if (rowCapacity < requiredRows)
	{
		free(safeBoundary);
		free(eventAtTick);
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_XM_CAPACITY;
	}

	#if SIZE_MAX <= UINT32_MAX
	if (requiredRows > SIZE_MAX / sizeof (bakerAdaptiveTimelineRow_t))
	{
		free(safeBoundary);
		free(eventAtTick);
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_XM_INVALID_ARGUMENT;
	}
	#endif

	if (requiredRows > SIZE_MAX / channels ||
		(size_t)requiredRows * channels >
			SIZE_MAX / sizeof (bakerAdaptiveXMCell_t))
	{
		free(safeBoundary);
		free(eventAtTick);
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_XM_INVALID_ARGUMENT;
	}

	bakerAdaptiveTimelineRow_t *plan = malloc(
		(size_t)requiredRows * sizeof (*plan));
	bakerAdaptiveXMCell_t *rendered = calloc(
		(size_t)requiredRows * channels, sizeof (*rendered));
	if (plan == NULL || rendered == NULL)
	{
		free(rendered);
		free(plan);
		free(safeBoundary);
		free(eventAtTick);
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_XM_NO_MEMORY;
	}

	uint32_t plannedRows = 0;
	if (!bakerAdaptiveTimelinePlan(safeBoundary, sourceTickCount, plan,
		requiredRows, &plannedRows) || plannedRows != requiredRows)
	{
		free(rendered);
		free(plan);
		free(safeBoundary);
		free(eventAtTick);
		publishStats(stats, &resultStats);
		return BAKER_ADAPTIVE_XM_INVALID_ARGUMENT;
	}

	uint8_t currentTPL = 1;
	for (uint32_t rowIndex = 0; rowIndex < requiredRows; rowIndex++)
	{
		const bakerAdaptiveTimelineRow_t *planned = &plan[rowIndex];
		bakerAdaptiveXMCell_t *output = &rendered[(size_t)rowIndex * channels];
		if (eventAtTick[planned->sourceTick])
		{
			/* Safe boundaries make these conditions construction invariants. */
			if (planned->tpl != 1 || currentTPL != 1)
			{
				free(rendered);
				free(plan);
				free(safeBoundary);
				free(eventAtTick);
				publishStats(stats, &resultStats);
				return BAKER_ADAPTIVE_XM_INVALID_ARGUMENT;
			}

			memcpy(output, &source[(size_t)planned->sourceTick * channels],
				(size_t)channels * sizeof (*output));
		}
		else
		{
			resultStats.timingRows++;
			if (planned->tpl != currentTPL)
			{
				output[0].efx = 0x0F;
				output[0].efxData = planned->tpl;
				currentTPL = planned->tpl;
				resultStats.speedCommands++;
			}
		}
	}

	memcpy(destination, rendered,
		(size_t)requiredRows * channels * sizeof (*destination));
	free(rendered);
	free(plan);
	free(safeBoundary);
	free(eventAtTick);
	publishStats(stats, &resultStats);
	return BAKER_ADAPTIVE_XM_OK;
}
