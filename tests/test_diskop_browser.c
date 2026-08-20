#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "ft2_diskop_browser.h"

static void testSelectionMovement(void)
{
	int32_t dirPos, selection;
	assert(diskOpBrowserMoveSelection(30, 15, 0, -1, 1,
		&dirPos, &selection));
	assert(dirPos == 0 && selection == 0);

	assert(diskOpBrowserMoveSelection(30, 15, 0, -1, -1,
		&dirPos, &selection));
	assert(dirPos == 0 && selection == 14);

	assert(diskOpBrowserMoveSelection(30, 15, 0, 14, 1,
		&dirPos, &selection));
	assert(dirPos == 1 && selection == 14);

	assert(diskOpBrowserMoveSelection(30, 15, 5, 0, -1,
		&dirPos, &selection));
	assert(dirPos == 4 && selection == 0);

	assert(diskOpBrowserMoveSelection(3, 15, 0, 2, 1,
		&dirPos, &selection));
	assert(dirPos == 0 && selection == 2);
}

static void testDoubleClick(void)
{
	assert(diskOpBrowserIsDoubleClick(7, 1000, 7, 1400, 400));
	assert(!diskOpBrowserIsDoubleClick(7, 1000, 7, 1401, 400));
	assert(!diskOpBrowserIsDoubleClick(7, 1000, 8, 1200, 400));
	assert(!diskOpBrowserIsDoubleClick(-1, 0, 0, 100, 400));

	/* Unsigned subtraction keeps SDL tick wraparound well-defined. */
	assert(diskOpBrowserIsDoubleClick(2, UINT32_MAX - 50, 2, 25, 100));
}

int main(void)
{
	testSelectionMovement();
	testDoubleClick();
	puts("Disk Op browser selection tests passed.");
	return 0;
}
