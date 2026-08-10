#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BAKER_ALLOCATOR_CHANNELS 32
#define BAKE_PATTERN_ROWS 256U
#define BAKE_MAX_PATTERNS 256U
#define BAKE_MAX_TICKS (BAKE_PATTERN_ROWS * BAKE_MAX_PATTERNS)
#define BAKE_OUTPUT_TPL 1U

typedef struct bakerTimelinePosition_t
{
	uint16_t order;
	uint16_t pattern;
	uint16_t row;
} bakerTimelinePosition_t;

bool bakerTimelinePosition(uint64_t absoluteTick, bakerTimelinePosition_t *position);
bool bakerEffectIsSourceSpeed(uint8_t effect, uint8_t parameter);
bool bakerEffectIsTempo(uint8_t effect, uint8_t parameter);

typedef struct bakerChannelAllocator_t
{
	int8_t owner[BAKER_ALLOCATOR_CHANNELS];
	uint8_t lastChannel[BAKER_ALLOCATOR_CHANNELS];
	uint8_t sourceChannelCount;
} bakerChannelAllocator_t;

void bakerChannelAllocatorReset(bakerChannelAllocator_t *allocator,
	uint8_t sourceChannelCount);
int32_t bakerChannelAllocatorRoute(bakerChannelAllocator_t *allocator,
	int32_t sourceChannel, uint32_t occupiedMask, bool *relocated);
int32_t bakerChannelAllocatorOwner(const bakerChannelAllocator_t *allocator,
	int32_t outputChannel);

/* The ordinary baker advances once per tracker row. A Fast Tracks bake must
** advance once per replayer tick, because private row crossings can occur
** between master rows. */
bool bakerTimelineShouldAdvance(bool tickResolution, bool songPlaying,
	uint16_t currentTick);

typedef struct bakerAudibilityState_t
{
	bool audible[BAKER_ALLOCATOR_CHANNELS];
} bakerAudibilityState_t;

void bakerAudibilitySnapshot(bakerAudibilityState_t *state,
	const bool *ordinaryMute, const bool *performanceMute, uint8_t channels);
uint32_t bakerAudibilityUpdate(bakerAudibilityState_t *state,
	const bool *ordinaryMute, const bool *performanceMute, uint8_t channels);
bool bakerChannelIsAudible(const bakerAudibilityState_t *state,
	int32_t channel);
