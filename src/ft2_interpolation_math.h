#pragma once

#include <stdint.h>

static inline uint8_t interpolationLinearByte(uint8_t start, uint8_t end,
	int32_t position, int32_t span)
{
	const int32_t numerator = ((int32_t)end - start) * position;
	const int32_t rounded = numerator >= 0
		? (numerator + span / 2) / span
		: (numerator - span / 2) / span;
	return (uint8_t)((int32_t)start + rounded);
}
