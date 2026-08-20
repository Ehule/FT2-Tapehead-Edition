#include <string.h>
#include "ft2_diskop_preview.h"

diskOpPreviewRoute_t diskOpPreviewSelectionRoute(bool browserMode,
	bool previewEnabled, bool isDirectory)
{
	if (!browserMode || isDirectory)
		return DISKOP_PREVIEW_ROUTE_NONE;

	return previewEnabled ? DISKOP_PREVIEW_ROUTE_PRIVATE :
		DISKOP_PREVIEW_ROUTE_LIVE_LOAD;
}

bool diskOpPreviewCapturesInput(bool browserMode, bool previewEnabled)
{
	return browserMode && previewEnabled;
}

bool diskOpPreviewRequestIsCurrent(uint32_t requestSerial,
	uint32_t currentSerial, bool cancelRequested, bool shutdownRequested)
{
	return !cancelRequested && !shutdownRequested &&
		requestSerial == currentSerial;
}

void diskOpPreviewMoveSample(sample_t *destination, sample_t *source)
{
	if (destination == NULL || source == NULL || destination == source)
		return;

	memcpy(destination, source, sizeof (*destination));
	memset(source, 0, sizeof (*source));
}
