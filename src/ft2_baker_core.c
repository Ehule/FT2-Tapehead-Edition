#include <string.h>
#include "ft2_baker_core.h"

void bakerChannelAllocatorReset(bakerChannelAllocator_t *allocator,
	uint8_t sourceChannelCount)
{
	if (sourceChannelCount > BAKER_ALLOCATOR_CHANNELS)
		sourceChannelCount = BAKER_ALLOCATOR_CHANNELS;

	memset(allocator->owner, -1, sizeof (allocator->owner));
	allocator->sourceChannelCount = sourceChannelCount;
	for (int32_t i = 0; i < BAKER_ALLOCATOR_CHANNELS; i++)
	{
		allocator->lastChannel[i] = (uint8_t)i;
		if (i < sourceChannelCount)
			allocator->owner[i] = (int8_t)i;
	}
}

int32_t bakerChannelAllocatorRoute(bakerChannelAllocator_t *allocator,
	int32_t sourceChannel, uint32_t occupiedMask, bool *relocated)
{
	if (relocated != NULL)
		*relocated = false;

	if (allocator == NULL || sourceChannel < 0 ||
		sourceChannel >= allocator->sourceChannelCount)
	{
		return -1;
	}

	/* Continue a relocated logical stream on the same XM channel whenever the
	** current row leaves that cell free. */
	int32_t destination = allocator->lastChannel[sourceChannel];
	if ((occupiedMask & (UINT32_C(1) << destination)) == 0)
	{
		if (relocated != NULL)
			*relocated = destination != sourceChannel;
		return destination;
	}

	/* Reuse an earlier spill channel owned by this source stream before
	** consuming another of XM's 32 channels. */
	for (int32_t i = 0; i < BAKER_ALLOCATOR_CHANNELS; i++)
	{
		if (allocator->owner[i] == sourceChannel &&
			(occupiedMask & (UINT32_C(1) << i)) == 0)
		{
			allocator->lastChannel[sourceChannel] = (uint8_t)i;
			if (relocated != NULL)
				*relocated = i != sourceChannel;
			return i;
		}
	}

	/* Claim an unused channel only when every channel already belonging to the
	** logical source track is occupied on this destination row. */
	for (int32_t i = 0; i < BAKER_ALLOCATOR_CHANNELS; i++)
	{
		if (allocator->owner[i] < 0 &&
			(occupiedMask & (UINT32_C(1) << i)) == 0)
		{
			allocator->owner[i] = (int8_t)sourceChannel;
			allocator->lastChannel[sourceChannel] = (uint8_t)i;
			if (relocated != NULL)
				*relocated = i != sourceChannel;
			return i;
		}
	}

	return -1;
}

int32_t bakerChannelAllocatorOwner(const bakerChannelAllocator_t *allocator,
	int32_t outputChannel)
{
	if (allocator == NULL || outputChannel < 0 ||
		outputChannel >= BAKER_ALLOCATOR_CHANNELS)
	{
		return -1;
	}

	return allocator->owner[outputChannel];
}

bool bakerTimelineShouldAdvance(bool tickResolution, bool songPlaying,
	uint16_t currentTick)
{
	return songPlaying && (tickResolution || currentTick == 1);
}
