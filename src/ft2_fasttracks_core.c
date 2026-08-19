#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ft2_fasttracks_core.h"

int32_t fastTracksClockTick(bool *started, int32_t *accumulator, uint16_t *lastTPL,
	uint8_t numerator, uint8_t denominator, uint16_t tpl)
{
	if (started == NULL || accumulator == NULL || lastTPL == NULL ||
		numerator == 0 || denominator == 0)
	{
		return 0;
	}

	if (tpl == 0)
		tpl = 1;

	uint16_t oldTPL = *lastTPL;
	if (oldTPL == 0)
		oldTPL = tpl;

	if (oldTPL != tpl)
	{
		/* Preserve normalized sub-row phase across an Fxx TPL change. */
		*accumulator = (int32_t)(((int64_t)*accumulator * tpl) / oldTPL);
	}
	*lastTPL = tpl;

	/* The first tick publishes the starting row without retriggering it. */
	if (!*started)
	{
		*started = true;
		return 0;
	}

	const int32_t threshold = denominator * tpl;
	*accumulator += numerator;

	int32_t crossings = 0;
	while (*accumulator >= threshold)
	{
		*accumulator -= threshold;
		crossings++;
	}

	return crossings;
}

fastTracksSharedBoundaryAction_t fastTracksResolveSharedBoundary(
	bool topologyActive, bool privateControl, bool masterRowAdvanced,
	uint32_t masterCycleRow, uint16_t sharedBoundary, int32_t physicalRow,
	int32_t physicalRows, bool positionJump, bool privateControlBoundary)
{
	if (physicalRows < 1)
		physicalRows = 1;
	if (sharedBoundary < 1)
		sharedBoundary = (uint16_t)physicalRows;

	/* Explicit FT2 jumps always win. A private CONTROL completion is only an
	** authority while the LEN/CONTROL topology is engaged. */
	if (positionJump || (topologyActive && privateControlBoundary))
		return FAST_TRACKS_SHARED_BOUNDARY_TRANSITION;

	if (topologyActive)
	{
		/* Private CONTROL advances in rational FastTracks time and announces its
		** own completed cycle above. Standard CONTROL and the natural longest-LEN
		** topology share the monotonically increasing master-cycle counter. */
		if (!privateControl && masterRowAdvanced &&
			masterCycleRow >= sharedBoundary)
		{
			return FAST_TRACKS_SHARED_BOUNDARY_TRANSITION;
		}

		if (physicalRow >= physicalRows)
			return FAST_TRACKS_SHARED_BOUNDARY_WRAP_PHYSICAL;

		return FAST_TRACKS_SHARED_BOUNDARY_CONTINUE;
	}

	return physicalRow >= physicalRows
		? FAST_TRACKS_SHARED_BOUNDARY_TRANSITION
		: FAST_TRACKS_SHARED_BOUNDARY_CONTINUE;
}

bool fastTracksSharedCycleUsesBlankRow(bool lengthTopologyBypassed,
	uint16_t sharedBoundary, int32_t physicalRows, uint32_t masterCycleRow)
{
	if (lengthTopologyBypassed || physicalRows < 1 ||
		sharedBoundary <= (uint16_t)physicalRows)
	{
		return false;
	}

	return masterCycleRow >= (uint32_t)physicalRows;
}
