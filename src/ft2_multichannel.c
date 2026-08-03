#include <stdbool.h>
#include "ft2_multichannel.h"

uint16_t channelOutputBusMask[MAX_CHANNELS];
uint16_t channelMonoOutputMask[MAX_CHANNELS];

static bool routingInitialized, monoRoutingInitialized, monoRoutingCustomized;
static uint8_t monoRoutingOutputCount;

void initializeChannelOutputRouting(void)
{
	if (routingInitialized)
		return;

	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		channelOutputBusMask[i] = 1; // Bus A

	routingInitialized = true;
}

void initializeMonoChannelOutputRouting(uint8_t outputChannelCount)
{
	if (outputChannelCount < 1)
		outputChannelCount = 1;
	else if (outputChannelCount > TAPEHEAD_MAX_OUTPUT_BUSES)
		outputChannelCount = TAPEHEAD_MAX_OUTPUT_BUSES;

	if (monoRoutingInitialized &&
		(monoRoutingCustomized || monoRoutingOutputCount == outputChannelCount))
	{
		return;
	}

	/*
	** Give the physical tracker lanes an immediately useful hardware layout:
	** Track 1 -> A, Track 2 -> B, and so on, repeating when there are more
	** tracker lanes than physical outputs. The stereo routing bank remains
	** untouched and returns exactly as it was when Mono Out is disabled.
	*/
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		channelMonoOutputMask[i] = (uint16_t)(1U << (i % outputChannelCount));

	monoRoutingInitialized = true;
	monoRoutingOutputCount = outputChannelCount;
}

uint8_t getChannelPrimaryOutputBus(int32_t channelIndex)
{
	if (channelIndex < 0 || channelIndex >= MAX_CHANNELS)
		return 0;

	uint16_t mask = channelOutputBusMask[channelIndex];
	if (mask == 0)
		return 0;

	/*
	** Bus A can be used as a "To Main" duplicate alongside one auxiliary bus.
	** Prefer the highest enabled bus for the scope marker and cycle behavior.
	*/
	for (int32_t bus = TAPEHEAD_MAX_OUTPUT_BUSES - 1; bus > 0; bus--)
	{
		if (mask & (1U << bus))
			return (uint8_t)bus;
	}

	return 0;
}

uint8_t getChannelPrimaryMonoOutput(int32_t channelIndex)
{
	if (channelIndex < 0 || channelIndex >= MAX_CHANNELS)
		return 0;

	const uint16_t mask = channelMonoOutputMask[channelIndex];
	if (mask == 0)
		return 0;

	for (int32_t output = TAPEHEAD_MAX_OUTPUT_BUSES - 1; output > 0; output--)
	{
		if (mask & (1U << output))
			return (uint8_t)output;
	}

	return 0;
}

uint16_t getChannelOutputMask(int32_t channelIndex, bool monoOutputMode)
{
	if (channelIndex < 0 || channelIndex >= MAX_CHANNELS)
		return 1;

	return monoOutputMode
		? channelMonoOutputMask[channelIndex]
		: channelOutputBusMask[channelIndex];
}

void cycleChannelOutputBus(int32_t channelIndex, uint8_t outputBusCount)
{
	if (channelIndex < 0 || channelIndex >= MAX_CHANNELS)
		return;

	if (outputBusCount < 1)
		outputBusCount = 1;
	else if (outputBusCount > TAPEHEAD_MAX_OUTPUT_BUSES)
		outputBusCount = TAPEHEAD_MAX_OUTPUT_BUSES;

	const uint8_t currentBus = getChannelPrimaryOutputBus(channelIndex);
	const uint8_t nextBus = (uint8_t)((currentBus + 1) % outputBusCount);
	channelOutputBusMask[channelIndex] = (uint16_t)(1U << nextBus);
}

void cycleChannelMonoOutput(int32_t channelIndex, uint8_t outputChannelCount)
{
	if (channelIndex < 0 || channelIndex >= MAX_CHANNELS)
		return;

	if (outputChannelCount < 1)
		outputChannelCount = 1;
	else if (outputChannelCount > TAPEHEAD_MAX_OUTPUT_BUSES)
		outputChannelCount = TAPEHEAD_MAX_OUTPUT_BUSES;

	const uint8_t currentOutput = getChannelPrimaryMonoOutput(channelIndex);
	const uint8_t nextOutput = (uint8_t)((currentOutput + 1) % outputChannelCount);
	channelMonoOutputMask[channelIndex] = (uint16_t)(1U << nextOutput);
	monoRoutingCustomized = true;
}

void toggleChannelMainOutput(int32_t channelIndex)
{
	if (channelIndex < 0 || channelIndex >= MAX_CHANNELS)
		return;

	const uint8_t primaryBus = getChannelPrimaryOutputBus(channelIndex);
	if (primaryBus == 0)
	{
		channelOutputBusMask[channelIndex] = 1;
		return;
	}

	channelOutputBusMask[channelIndex] ^= 1;
	if (channelOutputBusMask[channelIndex] == 0)
		channelOutputBusMask[channelIndex] = (uint16_t)(1U << primaryBus);
}
