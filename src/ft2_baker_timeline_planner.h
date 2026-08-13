#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BAKER_ADAPTIVE_MAX_TPL 31U

typedef struct bakerAdaptiveTimelineRow_t
{
	uint32_t sourceTick;
	uint8_t tpl;
	bool carriesSourceEvents;
} bakerAdaptiveTimelineRow_t;

/* Count the minimum XM rows needed to preserve sourceTickCount canonical
** TPL-1 ticks. Every source tick marked in eventAtTick must begin an output
** row, and no output row may span more than XM's F1F limit. */
uint32_t bakerAdaptiveTimelineRequiredRows(const bool *eventAtTick,
	uint32_t sourceTickCount);

/* Build the pure second-stage timeline plan. This does not move or copy any
** Baker events and does not modify the canonical TPL-1 capture. Each returned
** row begins at sourceTick, lasts tpl source ticks, and reports whether the
** first source tick carries events.
**
** If rowCapacity is too small, rowCount receives the required size and rows
** remains untouched. */
bool bakerAdaptiveTimelinePlan(const bool *eventAtTick,
	uint32_t sourceTickCount, bakerAdaptiveTimelineRow_t *rows,
	uint32_t rowCapacity, uint32_t *rowCount);
