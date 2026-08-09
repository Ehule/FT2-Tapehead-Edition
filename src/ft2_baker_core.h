#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BAKER_ALLOCATOR_CHANNELS 32

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
