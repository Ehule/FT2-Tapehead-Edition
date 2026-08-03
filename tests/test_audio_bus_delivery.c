#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../src/ft2_audio.h"

song_t song;

int main(void)
{
	float peakA, peakB;
	float monoPeaks[4];

	assert(tapeheadTestRouteSyntheticVoice(1, 2, 1, &peakA, &peakB));
	assert(peakA > 0.0f);
	assert(peakB == 0.0f);

	/* A stale global stereo count must not fold an explicit JACK B render to A. */
	assert(tapeheadTestRouteSyntheticVoice(2, 2, 1, &peakA, &peakB));
	assert(peakA == 0.0f);
	assert(peakB > 0.0f);

	/* To Main duplicates one voice into both buses without advancing it twice. */
	assert(tapeheadTestRouteSyntheticVoice(3, 2, 1, &peakA, &peakB));
	assert(peakA > 0.0f);
	assert(peakB > 0.0f);

	/* Every mono destination must be exclusive, regardless of XM panning. */
	for (uint8_t destination = 0; destination < 4; destination++)
	{
		assert(tapeheadTestRouteSyntheticMonoVoice(destination, 0,
			monoPeaks, 4));
		for (uint8_t output = 0; output < 4; output++)
			assert((monoPeaks[output] > 0.0f) == (output == destination));

		assert(tapeheadTestRouteSyntheticMonoVoice(destination, 255,
			monoPeaks, 4));
		for (uint8_t output = 0; output < 4; output++)
			assert((monoPeaks[output] > 0.0f) == (output == destination));
	}

	/* The Sample deck is mixed after the tracker voice pool and reaches its
	** own selected bus without occupying an XM channel. */
	assert(tapeheadTestRouteSyntheticSampleLauncherVoice(1, &peakA, &peakB));
	assert(peakA == 0.0f);
	assert(peakB > 0.0f);

	printf("Stereo and mono audio delivery tests passed.\n");
	return 0;
}
