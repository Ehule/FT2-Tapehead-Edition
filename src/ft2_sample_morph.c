#include <string.h>
#include "ft2_sample_morph.h"
#include "ft2_replayer.h"

#define SAMPLE_MORPH_TRACK_COUNT 8

static bool morphArmed;
static uint8_t selectedOrdinal[SAMPLE_MORPH_TRACK_COUNT];
static bool selectionInitialized[SAMPLE_MORPH_TRACK_COUNT];

static bool trackIsActive(int32_t channelIndex)
{
	return channelIndex >= 0 && channelIndex < SAMPLE_MORPH_TRACK_COUNT &&
		channelIndex < song.numChannels;
}

static bool sampleIsPopulated(const sample_t *sample)
{
	return sample != NULL && sample->dataPtr != NULL && sample->length > 0;
}

static uint8_t getPopulatedSamples(uint8_t instrumentNum,
	uint8_t samples[MAX_SMP_PER_INST])
{
	if (instrumentNum == 0 || instrumentNum > MAX_INST ||
		instr[instrumentNum] == NULL)
	{
		return 0;
	}

	uint8_t count = 0;
	for (uint8_t i = 0; i < MAX_SMP_PER_INST; i++)
	{
		if (sampleIsPopulated(&instr[instrumentNum]->smp[i]))
			samples[count++] = i;
	}
	return count;
}

static uint8_t currentInstrument(int32_t channelIndex)
{
	return trackIsActive(channelIndex) ? channel[channelIndex].instrNum : 0;
}

bool sampleMorphIsArmed(void)
{
	return morphArmed;
}

bool sampleMorphSetArmed(bool armed)
{
	if (morphArmed == armed)
		return false;

	if (armed)
	{
		for (int32_t channelIndex = 0; channelIndex < SAMPLE_MORPH_TRACK_COUNT;
			channelIndex++)
		{
			if (!trackIsActive(channelIndex) || selectionInitialized[channelIndex])
				continue;

			uint8_t samples[MAX_SMP_PER_INST];
			const uint8_t count = getPopulatedSamples(
				currentInstrument(channelIndex), samples);
			for (uint8_t ordinal = 0; ordinal < count; ordinal++)
			{
				if (samples[ordinal] == channel[channelIndex].smpNum)
				{
					selectedOrdinal[channelIndex] = ordinal;
					break;
				}
			}
			selectionInitialized[channelIndex] = true;
		}
	}
	morphArmed = armed;
	return true;
}

bool sampleMorphToggleArmed(void)
{
	return sampleMorphSetArmed(!morphArmed);
}

bool sampleMorphSetFromController(int32_t channelIndex, uint8_t value)
{
	if (!morphArmed || !trackIsActive(channelIndex))
		return false;

	uint8_t samples[MAX_SMP_PER_INST];
	const uint8_t count = getPopulatedSamples(currentInstrument(channelIndex),
		samples);
	if (count <= 1)
		return false;

	const uint8_t ordinal = (uint8_t)(((uint32_t)value * (count - 1) + 63) /
		127);
	if (selectedOrdinal[channelIndex] == ordinal)
		return false;

	selectedOrdinal[channelIndex] = ordinal;
	selectionInitialized[channelIndex] = true;
	return true;
}

bool sampleMorphStep(int32_t channelIndex, int32_t delta)
{
	if (!morphArmed || !trackIsActive(channelIndex) || delta == 0)
		return false;

	uint8_t samples[MAX_SMP_PER_INST];
	const uint8_t count = getPopulatedSamples(currentInstrument(channelIndex),
		samples);
	if (count <= 1)
		return false;

	const int32_t oldOrdinal = selectedOrdinal[channelIndex] < count
		? selectedOrdinal[channelIndex] : count - 1;
	int32_t ordinal = oldOrdinal + delta;
	if (ordinal < 0) ordinal = 0;
	if (ordinal >= count) ordinal = count - 1;
	selectedOrdinal[channelIndex] = (uint8_t)ordinal;
	selectionInitialized[channelIndex] = true;
	return ordinal != oldOrdinal;
}

bool sampleMorphStepAll(int32_t delta)
{
	bool changed = false;
	for (int32_t i = 0; i < SAMPLE_MORPH_TRACK_COUNT; i++)
		changed |= sampleMorphStep(i, delta);
	return changed;
}

uint8_t sampleMorphResolve(uint8_t channelIndex, uint8_t instrumentNum,
	uint8_t defaultSample)
{
	if (!morphArmed || channelIndex >= SAMPLE_MORPH_TRACK_COUNT)
		return defaultSample & 0x0F;

	uint8_t samples[MAX_SMP_PER_INST];
	const uint8_t count = getPopulatedSamples(instrumentNum, samples);
	if (count == 0)
		return defaultSample & 0x0F;

	uint8_t ordinal = selectedOrdinal[channelIndex];
	if (ordinal >= count)
		ordinal = count - 1;
	return samples[ordinal];
}

uint8_t sampleMorphGetSelectedSample(int32_t channelIndex)
{
	if (!trackIsActive(channelIndex))
		return 0;

	uint8_t samples[MAX_SMP_PER_INST];
	const uint8_t count = getPopulatedSamples(currentInstrument(channelIndex),
		samples);
	if (count == 0)
		return 0;

	uint8_t ordinal = selectedOrdinal[channelIndex];
	if (ordinal >= count)
		ordinal = count - 1;
	return samples[ordinal];
}

uint8_t sampleMorphGetPopulatedCount(int32_t channelIndex)
{
	uint8_t samples[MAX_SMP_PER_INST];
	return trackIsActive(channelIndex)
		? getPopulatedSamples(currentInstrument(channelIndex), samples) : 0;
}

uint8_t sampleMorphGetRingValue(int32_t channelIndex)
{
	const uint8_t count = sampleMorphGetPopulatedCount(channelIndex);
	if (count <= 1)
		return 0;

	uint8_t ordinal = selectedOrdinal[channelIndex];
	if (ordinal >= count)
		ordinal = count - 1;
	return (uint8_t)(((uint32_t)ordinal * 127 + ((count - 1) / 2)) /
		(count - 1));
}

void sampleMorphResetForLoadedModule(void)
{
	morphArmed = false;
	memset(selectedOrdinal, 0, sizeof (selectedOrdinal));
	memset(selectionInitialized, 0, sizeof (selectionInitialized));
}
