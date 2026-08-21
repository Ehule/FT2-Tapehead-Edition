#pragma once

#include <stdbool.h>
#include <stdint.h>

bool diskOpBrowserMoveSelection(int32_t fileCount, int32_t visibleCount,
	int32_t currentDirPos, int32_t currentVisibleSelection, int32_t delta,
	int32_t *newDirPos, int32_t *newVisibleSelection);
int32_t diskOpBrowserSampleSlotDelta(bool ctrlPressed, bool shiftPressed,
	bool altPressed, int32_t arrowDelta);
bool diskOpBrowserIsDoubleClick(int32_t previousEntry,
	uint32_t previousTime, int32_t entry, uint32_t now, uint32_t threshold);
