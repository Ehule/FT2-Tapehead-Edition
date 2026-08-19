#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
** Advance one rational FasTracks clock tick.
**
** This is deliberately independent of FT2, SDL, pattern storage and UI state
** so the exact accumulator used by the audio replayer can be tested natively.
** The caller performs one source-position move for each returned crossing.
*/
int32_t fastTracksClockTick(bool *started, int32_t *accumulator, uint16_t *lastTPL,
	uint8_t numerator, uint8_t denominator, uint16_t tpl);


typedef enum fastTracksSharedBoundaryAction_t
{
	FAST_TRACKS_SHARED_BOUNDARY_CONTINUE = 0,
	FAST_TRACKS_SHARED_BOUNDARY_WRAP_PHYSICAL,
	FAST_TRACKS_SHARED_BOUNDARY_TRANSITION
} fastTracksSharedBoundaryAction_t;

/*
** Resolve the exact shared-cycle decision used by the replayer after a normal
** row-boundary evaluation. The logical master row may be shorter or longer
** than the safe physical FT2 row domain.
*/
fastTracksSharedBoundaryAction_t fastTracksResolveSharedBoundary(
	bool topologyActive, bool privateControl, bool masterRowAdvanced,
	uint32_t masterCycleRow, uint16_t sharedBoundary, int32_t physicalRow,
	int32_t physicalRows, bool positionJump, bool privateControlBoundary);

/* True when a longer logical shared cycle is currently traversing the blank
** extension beyond the physical pattern container. */
bool fastTracksSharedCycleUsesBlankRow(bool lengthTopologyBypassed,
	uint16_t sharedBoundary, int32_t physicalRows, uint32_t masterCycleRow);
