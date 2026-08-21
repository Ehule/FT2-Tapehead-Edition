#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_video.h"

/*
** FT2's tiny font renderer intentionally supports alphanumerics only. The
** Pattern Editor is currently the sole translation unit that includes this
** header, and its FastTracks status strip formats ratios with a colon. Preserve
** the legacy tiny renderer, then fill the reserved colon slot with two pixels.
*/
static inline void tapeheadTextOutTinyWithColon(int32_t xPos, int32_t yPos,
	char *str, uint32_t color)
{
	textOutTiny(xPos, yPos, str, color);

	for (int32_t i = 0; str[i] != '\0'; i++)
	{
		if (str[i] != ':')
			continue;

		const int32_t colonX = xPos + (i * FONT3_CHAR_W) + 1;
		video.frameBuffer[((yPos + 2) * SCREEN_W) + colonX] = color;
		video.frameBuffer[((yPos + 4) * SCREEN_W) + colonX] = color;
	}
}

/* Keep this scoped to the Pattern Editor translation unit through the PR42
** transport-visual header; other FT2 tiny-text users retain legacy behavior. */
#define textOutTiny tapeheadTextOutTinyWithColon

static inline bool tapeheadTrackUsesIndependentTransportVisual(
	bool transportRunning, bool fastTrackEnabled,
	bool individualLengthEnabled, bool frozen)
{
	return transportRunning &&
		(fastTrackEnabled || individualLengthEnabled || frozen);
}

static inline int32_t tapeheadTransportVisualPageStart(int32_t playbackRow,
	int32_t rowsOnScreen)
{
	if (playbackRow <= 0 || rowsOnScreen <= 0)
		return 0;
	return (playbackRow / rowsOnScreen) * rowsOnScreen;
}
