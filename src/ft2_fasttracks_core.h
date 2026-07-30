#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
** Advance one rational FasTracks clock tick.
**
** This is deliberately independent of FT2, SDL, pattern storage and UI state
** so the exact accumulator used by the audio replayer can be tested natively.
** The caller performs one source-position move for each returned crossing.
*/
int32_t fastTracksClockTick(bool *started, int32_t *accumulator, uint16_t *lastTPL,
	uint8_t numerator, uint8_t denominator, uint16_t tpl);
