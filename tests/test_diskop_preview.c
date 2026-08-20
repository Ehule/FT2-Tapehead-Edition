#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ft2_diskop_preview.h"

static void testSelectionRouting(void)
{
	assert(diskOpPreviewSelectionRoute(true, true, false) ==
		DISKOP_PREVIEW_ROUTE_PRIVATE);
	assert(diskOpPreviewSelectionRoute(true, false, false) ==
		DISKOP_PREVIEW_ROUTE_LIVE_LOAD);
	assert(diskOpPreviewSelectionRoute(true, true, true) ==
		DISKOP_PREVIEW_ROUTE_NONE);
	assert(diskOpPreviewSelectionRoute(false, true, false) ==
		DISKOP_PREVIEW_ROUTE_NONE);
	assert(diskOpPreviewCapturesInput(true, true));
	assert(!diskOpPreviewCapturesInput(true, false));
	assert(!diskOpPreviewCapturesInput(false, true));
}

static void testNewestRequestAndShutdown(void)
{
	assert(diskOpPreviewRequestIsCurrent(9, 9, false, false));
	assert(!diskOpPreviewRequestIsCurrent(8, 9, false, false));
	assert(!diskOpPreviewRequestIsCurrent(9, 9, true, false));
	assert(!diskOpPreviewRequestIsCurrent(9, 9, false, true));
}

static void testSampleOwnershipTransfer(void)
{
	int8_t sampleData[4] = { 1, 2, 3, 4 };
	sample_t source, destination;
	memset(&source, 0, sizeof (source));
	memset(&destination, 0, sizeof (destination));
	source.dataPtr = sampleData;
	source.origDataPtr = sampleData;
	source.length = 4;
	source.volume = 64;

	diskOpPreviewMoveSample(&destination, &source);
	assert(destination.dataPtr == sampleData);
	assert(destination.origDataPtr == sampleData);
	assert(destination.length == 4);
	assert(destination.volume == 64);
	assert(source.dataPtr == NULL);
	assert(source.origDataPtr == NULL);
	assert(source.length == 0);
}

int main(void)
{
	testSelectionRouting();
	testNewestRequestAndShutdown();
	testSampleOwnershipTransfer();
	puts("Disk Op preview state tests passed.");
	return 0;
}
