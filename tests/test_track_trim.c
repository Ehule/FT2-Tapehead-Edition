#include <assert.h>
#include <stdio.h>
#include "ft2_track_trim.h"

int main(void)
{
	assert(tapeheadTrackTrimCeiling(100) == 256);
	assert(tapeheadTrackTrimCeiling(150) == 384);
	assert(tapeheadTrackTrimCeiling(200) == 512);
	assert(tapeheadTrackTrimCeiling(1) == 256);
	assert(tapeheadTrackTrimCeiling(255) == 512);

	for (uint8_t percent = 100; percent <= 200; percent += 50)
	{
		const uint16_t ceiling = tapeheadTrackTrimCeiling(percent);
		assert(tapeheadTrackTrimMapMidi(0, ceiling) == 0);
		assert(tapeheadTrackTrimMapMidi(127, ceiling) == ceiling);
		assert(tapeheadTrackTrimClamp(ceiling + 4, ceiling) == ceiling);
	}

	assert(tapeheadTrackTrimFillHeight(0, 32) == 0);
	assert(tapeheadTrackTrimFillHeight(128, 32) == 8);
	assert(tapeheadTrackTrimFillHeight(192, 32) == 12);
	assert(tapeheadTrackTrimFillHeight(256, 32) == 16);
	assert(tapeheadTrackTrimFillHeight(512, 32) == 32);
	assert(tapeheadTrackTrimBand(0) == TAPEHEAD_TRACK_TRIM_BAND_GREEN);
	assert(tapeheadTrackTrimBand(191) == TAPEHEAD_TRACK_TRIM_BAND_GREEN);
	assert(tapeheadTrackTrimBand(192) == TAPEHEAD_TRACK_TRIM_BAND_YELLOW);
	assert(tapeheadTrackTrimBand(256) == TAPEHEAD_TRACK_TRIM_BAND_YELLOW);
	assert(tapeheadTrackTrimBand(257) == TAPEHEAD_TRACK_TRIM_BAND_RED);
	puts("Track trim calculation tests passed.");
}
