#include "ft2_diskop_browser.h"
#include <stddef.h>

static int32_t clamp(int32_t value, int32_t minimum, int32_t maximum)
{
	if (value < minimum)
		return minimum;
	if (value > maximum)
		return maximum;
	return value;
}

bool diskOpBrowserMoveSelection(int32_t fileCount, int32_t visibleCount,
	int32_t currentDirPos, int32_t currentVisibleSelection, int32_t delta,
	int32_t *newDirPos, int32_t *newVisibleSelection)
{
	if (fileCount <= 0 || visibleCount <= 0 || newDirPos == NULL ||
		newVisibleSelection == NULL || (delta != -1 && delta != 1))
	{
		return false;
	}

	currentDirPos = clamp(currentDirPos, 0, fileCount - 1);
	int32_t absoluteEntry;
	if (currentVisibleSelection < 0)
	{
		absoluteEntry = delta < 0
			? clamp(currentDirPos + visibleCount - 1, 0, fileCount - 1)
			: currentDirPos;
	}
	else
	{
		absoluteEntry = currentDirPos + currentVisibleSelection + delta;
		absoluteEntry = clamp(absoluteEntry, 0, fileCount - 1);
	}

	if (absoluteEntry < currentDirPos)
		currentDirPos = absoluteEntry;
	else if (absoluteEntry >= currentDirPos + visibleCount)
		currentDirPos = absoluteEntry - visibleCount + 1;

	*newDirPos = currentDirPos;
	*newVisibleSelection = absoluteEntry - currentDirPos;
	return true;
}

int32_t diskOpBrowserSampleSlotDelta(bool ctrlPressed, bool shiftPressed,
	bool altPressed, int32_t arrowDelta)
{
	if (!ctrlPressed || shiftPressed || altPressed ||
		(arrowDelta != -1 && arrowDelta != 1))
	{
		return 0;
	}

	return arrowDelta;
}

bool diskOpBrowserIsDoubleClick(int32_t previousEntry,
	uint32_t previousTime, int32_t entry, uint32_t now, uint32_t threshold)
{
	return previousEntry >= 0 && previousEntry == entry &&
		now - previousTime <= threshold;
}
