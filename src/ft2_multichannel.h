#pragma once

#include <stdint.h>
#include "ft2_header.h"

/*
** Tapehead exposes a fixed-capacity logical bus bank. The configured output
** count determines how many stereo pairs are requested from SDL, while the
** routing masks remain independent of the current audio device.
*/
#define TAPEHEAD_MAX_OUTPUT_BUSES 16

extern uint16_t channelOutputBusMask[MAX_CHANNELS];
extern uint16_t channelMonoOutputMask[MAX_CHANNELS];

void initializeChannelOutputRouting(void);
void initializeMonoChannelOutputRouting(uint8_t outputChannelCount);
void cycleChannelOutputBus(int32_t channelIndex, uint8_t outputBusCount);
void cycleChannelMonoOutput(int32_t channelIndex, uint8_t outputChannelCount);
void toggleChannelMainOutput(int32_t channelIndex);
uint8_t getChannelPrimaryOutputBus(int32_t channelIndex);
uint8_t getChannelPrimaryMonoOutput(int32_t channelIndex);
uint16_t getChannelOutputMask(int32_t channelIndex, bool monoOutputMode);
