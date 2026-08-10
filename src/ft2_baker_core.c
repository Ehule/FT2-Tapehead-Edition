#include <stdio.h>
#include <string.h>
#include "ft2_baker_core.h"

bool bakerPatternRowsValid(uint16_t patternRows)
{
	return patternRows == 16 || patternRows == 32 || patternRows == 64 ||
		patternRows == 128 || patternRows == 256;
}

uint32_t bakerCapacityTicks(uint16_t patternRows)
{
	return bakerPatternRowsValid(patternRows) ? patternRows * BAKE_MAX_PATTERNS : 0;
}

bool bakerTimelinePosition(uint64_t absoluteTick, uint16_t patternRows,
	bakerTimelinePosition_t *position)
{
	const uint32_t capacityTicks = bakerCapacityTicks(patternRows);
	if (position == NULL || capacityTicks == 0 || absoluteTick >= capacityTicks)
		return false;

	position->order = (uint16_t)(absoluteTick / patternRows);
	position->pattern = position->order; /* baked patterns are unique and linear */
	position->row = (uint16_t)(absoluteTick % patternRows);
	return position->order < BAKE_MAX_PATTERNS &&
		position->pattern < BAKE_MAX_PATTERNS && position->row < patternRows;
}

void bakerFormatTimingEstimates(uint16_t patternRows, uint16_t bpm,
	char *patternText, uint32_t patternTextSize,
	char *maximumText, uint32_t maximumTextSize)
{
	if (!bakerPatternRowsValid(patternRows)) patternRows = BAKE_DEFAULT_PATTERN_ROWS;
	if (bpm == 0) bpm = 1;
	const double patternSeconds = patternRows * 2.5 / bpm;
	const uint32_t maximumSeconds = (uint32_t)(patternRows * BAKE_MAX_PATTERNS * 2.5 / bpm + 0.5);
	if (patternSeconds < 10.0)
		snprintf(patternText, patternTextSize, "Pattern:  ~%.2g sec", patternSeconds);
	else
		snprintf(patternText, patternTextSize, "Pattern:  ~%.0f sec", patternSeconds);
	const uint32_t minutes = maximumSeconds / 60;
	snprintf(maximumText, maximumTextSize, "Maximum:  ~%u:%02u at %u BPM",
		minutes, maximumSeconds % 60, bpm);
}

bool bakerEffectIsSourceSpeed(uint8_t effect, uint8_t parameter)
{
	return effect == 0x0F && parameter > 0 && parameter < 0x20;
}

bool bakerEffectIsTempo(uint8_t effect, uint8_t parameter)
{
	return effect == 0x0F && parameter >= 0x20;
}

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

void bakerAudibilitySnapshot(bakerAudibilityState_t *state,
	const bool *ordinaryMute, const bool *performanceMute, uint8_t channels)
{
	if (state == NULL)
		return;
	memset(state, 0, sizeof (*state));
	if (channels > BAKER_ALLOCATOR_CHANNELS)
		channels = BAKER_ALLOCATOR_CHANNELS;
	for (uint8_t i = 0; i < channels; i++)
		state->audible[i] = !ordinaryMute[i] && !performanceMute[i];
}

uint32_t bakerAudibilityUpdate(bakerAudibilityState_t *state,
	const bool *ordinaryMute, const bool *performanceMute, uint8_t channels)
{
	if (state == NULL)
		return 0;
	if (channels > BAKER_ALLOCATOR_CHANNELS)
		channels = BAKER_ALLOCATOR_CHANNELS;
	uint32_t becameMuted = 0;
	for (uint8_t i = 0; i < channels; i++)
	{
		const bool audible = !ordinaryMute[i] && !performanceMute[i];
		if (state->audible[i] && !audible)
			becameMuted |= UINT32_C(1) << i;
		state->audible[i] = audible;
	}
	return becameMuted;
}

bool bakerChannelIsAudible(const bakerAudibilityState_t *state,
	int32_t channel)
{
	return state != NULL && channel >= 0 &&
		channel < BAKER_ALLOCATOR_CHANNELS && state->audible[channel];
}
