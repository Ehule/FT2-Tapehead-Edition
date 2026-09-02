#pragma once

#include <stdint.h>

#define BAKER_ADAPTIVE_XM_MAX_CHANNELS 32U

/* A dependency-free mirror of the seven-byte XM note_t cell. The isolated
** adaptive stage deliberately does not depend on the live replayer globals. */
typedef struct bakerAdaptiveXMCell_t
{
	uint8_t note;
	uint8_t instr;
	uint8_t vol;
	uint8_t efx;
	uint8_t efxData;
	uint8_t tuneType;
	uint8_t tuneData;
} bakerAdaptiveXMCell_t;

typedef enum bakerAdaptiveXMResult_t
{
	BAKER_ADAPTIVE_XM_OK = 0,
	BAKER_ADAPTIVE_XM_INVALID_ARGUMENT,
	BAKER_ADAPTIVE_XM_CAPACITY,
	BAKER_ADAPTIVE_XM_NO_MEMORY,
	BAKER_ADAPTIVE_XM_SOURCE_SPEED
} bakerAdaptiveXMResult_t;

typedef struct bakerAdaptiveXMStats_t
{
	uint32_t sourceTicks;
	uint32_t outputRows;
	uint32_t timingRows;
	uint32_t speedCommands;
	uint8_t initialTPL;
} bakerAdaptiveXMStats_t;

/* Convert a canonical TPL-1 Baker timeline into XM-compatible rows. Source
** cells are arranged sourceTickCount * channels in row-major order.
**
** Every row carrying a source event remains at TPL 1. Only entirely empty
** canonical ticks may be combined, and synthesized F01..F1F commands are
** written on otherwise empty timing rows. This prevents continuous effects,
** retriggers, BPM commands and fully occupied event rows from changing their
** canonical one-tick behavior. Synthesized F01..F1F commands are emitted in
** timingChannel, which callers normally reserve as a dedicated final track.
**
** The destination may alias source. Capacity and validation failures leave it
** untouched. On BAKER_ADAPTIVE_XM_CAPACITY, stats->outputRows reports the
** required row count when stats is non-NULL. Canonical input must not contain
** source F01..F1F commands; encountering one fails losslessly. */
bakerAdaptiveXMResult_t bakerAdaptiveXMBuild(
	const bakerAdaptiveXMCell_t *source, uint32_t sourceTickCount,
	uint8_t channels, uint8_t timingChannel,
	bakerAdaptiveXMCell_t *destination,
	uint32_t rowCapacity, bakerAdaptiveXMStats_t *stats);
