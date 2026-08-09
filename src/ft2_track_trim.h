#pragma once

#include <stdint.h>

#define TAPEHEAD_TRACK_TRIM_MIN 0
#define TAPEHEAD_TRACK_TRIM_UNITY 256
#define TAPEHEAD_TRACK_TRIM_MAX 512

typedef enum tapeheadTrackTrimBand_t
{
	TAPEHEAD_TRACK_TRIM_BAND_GREEN = 0,
	TAPEHEAD_TRACK_TRIM_BAND_YELLOW,
	TAPEHEAD_TRACK_TRIM_BAND_RED
} tapeheadTrackTrimBand_t;

uint16_t tapeheadTrackTrimCeiling(uint8_t percent);
uint16_t tapeheadTrackTrimMapMidi(uint8_t value, uint16_t ceiling);
uint16_t tapeheadTrackTrimClamp(int32_t trim, uint16_t ceiling);
uint16_t tapeheadTrackTrimFillHeight(uint16_t trim, uint16_t height);
tapeheadTrackTrimBand_t tapeheadTrackTrimBand(uint16_t trim);
