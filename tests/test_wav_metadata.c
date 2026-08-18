#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "ft2_wav_metadata.h"

static void testRootAndFineTuning(void)
{
	assert(fabs(tapeheadWavC4Rate(48000, 60, 0) - 48000.0) < 0.001);
	assert(fabs(tapeheadWavC4Rate(48000, 72, 0) - 24000.0) < 0.001);
	const double halfSemitone = tapeheadWavC4Rate(48000, 60, UINT32_C(0x80000000));
	assert(fabs(halfSemitone - (48000.0 * exp2(-0.5 / 12.0))) < 0.001);
	assert(tapeheadWavC4Rate(44100, 200, 123) == 44100.0);
}

static void testLoopKindsAndBounds(void)
{
	uint32_t start = 0, length = 0;
	assert(tapeheadWavDecodeLoop(0, 2, 9, 10, &start, &length) ==
		TAPEHEAD_WAV_LOOP_FORWARD);
	assert(start == 2 && length == 8);
	assert(tapeheadWavDecodeLoop(1, 3, 7, 10, &start, &length) ==
		TAPEHEAD_WAV_LOOP_PINGPONG);
	assert(tapeheadWavDecodeLoop(2, 4, 4, 10, &start, &length) ==
		TAPEHEAD_WAV_LOOP_REVERSE);
	assert(start == 4 && length == 1);
	assert(tapeheadWavDecodeLoop(2, 8, 7, 10, &start, &length) ==
		TAPEHEAD_WAV_LOOP_INVALID);
	assert(tapeheadWavDecodeLoop(2, 0, 10, 10, &start, &length) ==
		TAPEHEAD_WAV_LOOP_INVALID);
	assert(tapeheadWavDecodeLoop(3, 0, 1, 10, &start, &length) ==
		TAPEHEAD_WAV_LOOP_INVALID);
}

int main(void)
{
	testRootAndFineTuning();
	testLoopKindsAndBounds();
	puts("WAV metadata tests passed.");
	return 0;
}
