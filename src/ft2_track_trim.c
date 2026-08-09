#include "ft2_track_trim.h"

uint16_t tapeheadTrackTrimCeiling(uint8_t percent)
{
	if (percent < 100) percent = 100;
	if (percent > 200) percent = 200;
	return (uint16_t)(((uint32_t)percent * TAPEHEAD_TRACK_TRIM_UNITY) / 100);
}

uint16_t tapeheadTrackTrimMapMidi(uint8_t value, uint16_t ceiling)
{
	if (value > 127) value = 127;
	return (uint16_t)(((uint32_t)value * ceiling + 63) / 127);
}

uint16_t tapeheadTrackTrimClamp(int32_t trim, uint16_t ceiling)
{
	if (trim < TAPEHEAD_TRACK_TRIM_MIN) return TAPEHEAD_TRACK_TRIM_MIN;
	if (trim > ceiling) return ceiling;
	return (uint16_t)trim;
}

uint16_t tapeheadTrackTrimFillHeight(uint16_t trim, uint16_t height)
{
	if (trim > TAPEHEAD_TRACK_TRIM_MAX) trim = TAPEHEAD_TRACK_TRIM_MAX;
	return (uint16_t)(((uint32_t)trim * height + (TAPEHEAD_TRACK_TRIM_MAX / 2)) /
		TAPEHEAD_TRACK_TRIM_MAX);
}

tapeheadTrackTrimBand_t tapeheadTrackTrimBand(uint16_t trim)
{
	if (trim > TAPEHEAD_TRACK_TRIM_UNITY) return TAPEHEAD_TRACK_TRIM_BAND_RED;
	if (trim >= 192) return TAPEHEAD_TRACK_TRIM_BAND_YELLOW;
	return TAPEHEAD_TRACK_TRIM_BAND_GREEN;
}
