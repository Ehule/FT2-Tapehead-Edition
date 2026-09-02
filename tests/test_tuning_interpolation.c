#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "ft2_interpolation_math.h"

int main(void)
{
	/* M80..M87 is a one-cent rise on every row. */
	for (int32_t row = 0; row <= 7; row++)
		assert(interpolationLinearByte(0x80, 0x87, row, 7) == 0x80 + row);

	/* Nxx depth supports ascending and descending ramps with symmetric
	** nearest-integer rounding. */
	assert(interpolationLinearByte(0x04, 0x10, 1, 3) == 0x08);
	assert(interpolationLinearByte(0x04, 0x10, 2, 3) == 0x0C);
	assert(interpolationLinearByte(0x10, 0x04, 1, 3) == 0x0C);
	assert(interpolationLinearByte(0x10, 0x04, 2, 3) == 0x08);

	assert(interpolationLinearByte(0x00, 0xFF, 0, 9) == 0x00);
	assert(interpolationLinearByte(0x00, 0xFF, 9, 9) == 0xFF);
	puts("Mxx/Nxx interpolation math tests passed.");
	return 0;
}
