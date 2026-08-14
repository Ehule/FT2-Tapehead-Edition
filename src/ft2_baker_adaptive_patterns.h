#pragma once

#include <stdint.h>
#include "ft2_baker_adaptive_xm.h"

#define BAKER_ADAPTIVE_PATTERN_MAX_COUNT 256U
#define BAKER_ADAPTIVE_PATTERN_MAX_ROWS 256U

typedef enum bakerAdaptivePatternResult_t
{
	BAKER_ADAPTIVE_PATTERN_OK = 0,
	BAKER_ADAPTIVE_PATTERN_INVALID_ARGUMENT,
	BAKER_ADAPTIVE_PATTERN_CAPACITY,
	BAKER_ADAPTIVE_PATTERN_NO_MEMORY
} bakerAdaptivePatternResult_t;

typedef struct bakerAdaptivePatternStats_t
{
	uint32_t inputRows;
	uint32_t requiredPatterns;
	uint32_t storedCells;
	uint16_t finalPatternRows;
} bakerAdaptivePatternStats_t;

/* An isolated mirror of the linear order/pattern subset needed by an XM
** module. Patterns are uniquely ordered 0..patternCount-1. Each allocation is
** tightly sized to rowCount[pattern] * channels; in particular, the final
** pattern has its exact row count instead of padding the captured endpoint. */
typedef struct bakerAdaptivePatternSet_t
{
	uint16_t patternCount;
	uint16_t orderCount;
	uint16_t nominalPatternRows;
	uint8_t channels;
	uint8_t orders[BAKER_ADAPTIVE_PATTERN_MAX_COUNT];
	uint16_t rowCount[BAKER_ADAPTIVE_PATTERN_MAX_COUNT];
	bakerAdaptiveXMCell_t *pattern[BAKER_ADAPTIVE_PATTERN_MAX_COUNT];
} bakerAdaptivePatternSet_t;

/* Pack linear adaptive-XM rows into legal XM patterns and a unique linear
** order table. destination and *destination must be non-NULL and NULL,
** respectively. The destination pointer remains NULL on every failure.
** patternRows may be 1..256; the eventual Baker integration can restrict its
** UI to the existing 16/32/64/128/256 choices without coupling this pure
** storage stage to UI policy. */
bakerAdaptivePatternResult_t bakerAdaptivePatternSetBuild(
	const bakerAdaptiveXMCell_t *linearRows, uint32_t linearRowCount,
	uint8_t channels, uint16_t patternRows,
	bakerAdaptivePatternSet_t **destination,
	bakerAdaptivePatternStats_t *stats);

void bakerAdaptivePatternSetFree(bakerAdaptivePatternSet_t *patternSet);
