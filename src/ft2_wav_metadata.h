#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

typedef enum tapeheadWavLoopKind_t
{
	TAPEHEAD_WAV_LOOP_INVALID = 0,
	TAPEHEAD_WAV_LOOP_FORWARD,
	TAPEHEAD_WAV_LOOP_PINGPONG,
	TAPEHEAD_WAV_LOOP_REVERSE
} tapeheadWavLoopKind_t;

static inline double tapeheadWavC4Rate(uint32_t sampleRate,
	uint32_t unityNote, uint32_t pitchFraction)
{
	if (unityNote > 127)
		return sampleRate;
	const double fraction = pitchFraction * (1.0 / 4294967296.0);
	return sampleRate * exp2((60.0 - (unityNote + fraction)) / 12.0);
}

static inline tapeheadWavLoopKind_t tapeheadWavDecodeLoop(uint32_t loopType,
	uint32_t loopStart, uint32_t loopEnd, uint32_t sampleLength,
	uint32_t *decodedStart, uint32_t *decodedLength)
{
	if (loopType > 2 || loopStart > loopEnd || loopEnd >= sampleLength)
		return TAPEHEAD_WAV_LOOP_INVALID;
	if (decodedStart != NULL)
		*decodedStart = loopStart;
	if (decodedLength != NULL)
		*decodedLength = (loopEnd - loopStart) + 1;
	return (tapeheadWavLoopKind_t)(TAPEHEAD_WAV_LOOP_FORWARD + loopType);
}
