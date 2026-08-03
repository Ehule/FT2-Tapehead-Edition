#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/ft2_multichannel.h"

int main(void)
{
	initializeChannelOutputRouting();

	for (int32_t channel = 0; channel < MAX_CHANNELS; channel++)
	{
		assert(channelOutputBusMask[channel] == 1);
		assert(getChannelPrimaryOutputBus(channel) == 0);
	}

	initializeMonoChannelOutputRouting(4);
	for (int32_t channel = 0; channel < MAX_CHANNELS; channel++)
	{
		assert(channelMonoOutputMask[channel] == (1U << (channel & 3)));
		assert(getChannelPrimaryMonoOutput(channel) == (channel & 3));
		assert(getChannelOutputMask(channel, true) == channelMonoOutputMask[channel]);
		assert(getChannelOutputMask(channel, false) == channelOutputBusMask[channel]);
	}

	cycleChannelMonoOutput(0, 4);
	assert(channelMonoOutputMask[0] == 2);
	cycleChannelMonoOutput(0, 4);
	assert(channelMonoOutputMask[0] == 4);
	cycleChannelMonoOutput(0, 4);
	assert(channelMonoOutputMask[0] == 8);
	cycleChannelMonoOutput(0, 4);
	assert(channelMonoOutputMask[0] == 1);

	cycleChannelOutputBus(3, 2);
	assert(channelOutputBusMask[3] == 2);
	assert(getChannelPrimaryOutputBus(3) == 1);

	toggleChannelMainOutput(3);
	assert(channelOutputBusMask[3] == 3);
	assert(getChannelPrimaryOutputBus(3) == 1);

	toggleChannelMainOutput(3);
	assert(channelOutputBusMask[3] == 2);

	cycleChannelOutputBus(3, 2);
	assert(channelOutputBusMask[3] == 1);
	assert(getChannelPrimaryOutputBus(3) == 0);

	cycleChannelOutputBus(4, TAPEHEAD_MAX_OUTPUT_BUSES);
	assert(channelOutputBusMask[4] == 2);
	for (int32_t bus = 1; bus < TAPEHEAD_MAX_OUTPUT_BUSES; bus++)
		cycleChannelOutputBus(4, TAPEHEAD_MAX_OUTPUT_BUSES);
	assert(channelOutputBusMask[4] == 1);

	channelOutputBusMask[5] = 0;
	assert(getChannelPrimaryOutputBus(5) == 0);
	toggleChannelMainOutput(5);
	assert(channelOutputBusMask[5] == 1);

	printf("Multichannel routing tests passed.\n");
	return 0;
}
