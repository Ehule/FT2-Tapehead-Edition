#include <stddef.h>
#include "ft2_baker_timeline_planner.h"

static uint32_t planTimeline(const bool *eventAtTick, uint32_t sourceTickCount,
	bakerAdaptiveTimelineRow_t *rows)
{
	uint32_t outputRow = 0;
	uint32_t sourceTick = 0;

	while (sourceTick < sourceTickCount)
	{
		uint32_t nextEventTick = sourceTick + 1;
		while (nextEventTick < sourceTickCount && !eventAtTick[nextEventTick])
			nextEventTick++;

		const uint32_t segmentEnd = nextEventTick < sourceTickCount
			? nextEventTick : sourceTickCount;
		while (sourceTick < segmentEnd)
		{
			const uint32_t remaining = segmentEnd - sourceTick;
			const uint8_t tpl = (uint8_t)(remaining > BAKER_ADAPTIVE_MAX_TPL
				? BAKER_ADAPTIVE_MAX_TPL : remaining);

			if (rows != NULL)
			{
				rows[outputRow].sourceTick = sourceTick;
				rows[outputRow].tpl = tpl;
				rows[outputRow].carriesSourceEvents = eventAtTick[sourceTick];
			}

			outputRow++;
			sourceTick += tpl;
		}
	}

	return outputRow;
}

uint32_t bakerAdaptiveTimelineRequiredRows(const bool *eventAtTick,
	uint32_t sourceTickCount)
{
	if (sourceTickCount == 0)
		return 0;
	if (eventAtTick == NULL)
		return 0;

	return planTimeline(eventAtTick, sourceTickCount, NULL);
}

bool bakerAdaptiveTimelinePlan(const bool *eventAtTick,
	uint32_t sourceTickCount, bakerAdaptiveTimelineRow_t *rows,
	uint32_t rowCapacity, uint32_t *rowCount)
{
	if (rowCount == NULL)
		return false;

	*rowCount = 0;
	if (sourceTickCount == 0)
		return true;
	if (eventAtTick == NULL)
		return false;

	const uint32_t requiredRows = planTimeline(eventAtTick,
		sourceTickCount, NULL);
	*rowCount = requiredRows;
	if (rows == NULL || rowCapacity < requiredRows)
		return false;

	planTimeline(eventAtTick, sourceTickCount, rows);
	return true;
}
