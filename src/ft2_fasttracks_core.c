#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ft2_fasttracks_core.h"

int32_t fastTracksClockTick(bool *started, int32_t *accumulator, uint16_t *lastTPL,
	uint8_t numerator, uint8_t denominator, uint16_t tpl)
{
	if (started == NULL || accumulator == NULL || lastTPL == NULL ||
		numerator == 0 || denominator == 0)
	{
		return 0;
	}

	if (tpl == 0)
		tpl = 1;

	uint16_t oldTPL = *lastTPL;
	if (oldTPL == 0)
		oldTPL = tpl;

	if (oldTPL != tpl)
	{
		/* Preserve normalized sub-row phase across an Fxx TPL change. */
		*accumulator = (int32_t)(((int64_t)*accumulator * tpl) / oldTPL);
	}
	*lastTPL = tpl;

	/* The first tick publishes the starting row without retriggering it. */
	if (!*started)
	{
		*started = true;
		return 0;
	}

	const int32_t threshold = denominator * tpl;
	*accumulator += numerator;

	int32_t crossings = 0;
	while (*accumulator >= threshold)
	{
		*accumulator -= threshold;
		crossings++;
	}

	return crossings;
}
