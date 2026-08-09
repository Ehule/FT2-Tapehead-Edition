#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Runtime-only per-track sample override for future note triggers. */
bool sampleMorphIsArmed(void);
bool sampleMorphSetArmed(bool armed);
bool sampleMorphToggleArmed(void);
bool sampleMorphSetFromController(int32_t channelIndex, uint8_t value);
bool sampleMorphStep(int32_t channelIndex, int32_t delta);
bool sampleMorphStepAll(int32_t delta);
uint8_t sampleMorphResolve(uint8_t channelIndex, uint8_t instrumentNum,
	uint8_t defaultSample);
uint8_t sampleMorphGetSelectedSample(int32_t channelIndex);
uint8_t sampleMorphGetPopulatedCount(int32_t channelIndex);
uint8_t sampleMorphGetRingValue(int32_t channelIndex);
void sampleMorphResetForLoadedModule(void);
