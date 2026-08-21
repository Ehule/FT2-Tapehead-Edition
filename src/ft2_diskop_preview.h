#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ft2_replayer.h"

typedef enum diskOpPreviewRoute_t
{
	DISKOP_PREVIEW_ROUTE_NONE = 0,
	DISKOP_PREVIEW_ROUTE_PRIVATE,
	DISKOP_PREVIEW_ROUTE_LIVE_LOAD
} diskOpPreviewRoute_t;

diskOpPreviewRoute_t diskOpPreviewSelectionRoute(bool browserMode,
	bool previewEnabled, bool isDirectory);
bool diskOpPreviewCapturesInput(bool browserMode, bool previewEnabled);
bool diskOpPreviewRequestIsCurrent(uint32_t requestSerial,
	uint32_t currentSerial, bool cancelRequested, bool shutdownRequested);
void diskOpPreviewMoveSample(sample_t *destination, sample_t *source);
