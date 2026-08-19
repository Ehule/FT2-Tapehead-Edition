#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ft2_header.h"
#include "ft2_structs.h"
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

/*
** PR42 is a renderer policy, not a second transport. These helpers deliberately
** consume state that the replayer already owns and keep only one startup-time
** configuration cache. Missing/old tapehead.ini files default to the TapeHead
** behavior requested by PR42.
*/
static inline bool tapeheadPerTrackTransportVisualsEnabled(void)
{
	static int8_t cached = -1;
	if (cached >= 0)
		return cached != 0;

	if (editor.configFileLocationU == NULL)
		return true;

#ifdef _WIN32
	static const UNICHAR cfgName[] = L"FT2.CFG";
	static const UNICHAR iniName[] = L"tapehead.ini";
#else
	static const UNICHAR cfgName[] = "FT2.CFG";
	static const UNICHAR iniName[] = "tapehead.ini";
#endif

	const size_t pathLen = UNICHAR_STRLEN(editor.configFileLocationU);
	const size_t cfgLen = UNICHAR_STRLEN(cfgName);
	const size_t iniLen = UNICHAR_STRLEN(iniName);
	if (pathLen < cfgLen)
	{
		cached = 1;
		return true;
	}

	UNICHAR *path = (UNICHAR *)malloc((pathLen - cfgLen + iniLen + 1) *
		sizeof (UNICHAR));
	if (path == NULL)
		return true;

	UNICHAR_STRCPY(path, editor.configFileLocationU);
	path[pathLen - cfgLen] = 0;
	UNICHAR_STRCAT(path, iniName);

	FILE *f = UNICHAR_FOPEN(path, "r");
	free(path);
	if (f == NULL)
	{
		cached = 1;
		return true;
	}

	cached = 1;
	bool inPatternSection = false;
	char line[512];
	while (fgets(line, sizeof (line), f) != NULL)
	{
		char *text = line;
		while (isspace((unsigned char)*text)) text++;
		char *end = text + strlen(text);
		while (end > text && isspace((unsigned char)end[-1])) end--;
		*end = '\0';

		if (*text == '\0' || *text == ';' || *text == '#')
			continue;
		if (*text == '[')
		{
			inPatternSection = !_stricmp(text, "[Pattern]");
			continue;
		}
		if (!inPatternSection)
			continue;

		char *equals = strchr(text, '=');
		if (equals == NULL)
			continue;
		*equals = '\0';
		char *key = text;
		char *value = equals + 1;
		while (*key != '\0' && isspace((unsigned char)key[strlen(key)-1]))
			key[strlen(key)-1] = '\0';
		while (isspace((unsigned char)*value)) value++;

		if (_stricmp(key, "PerTrackTransportVisuals"))
			continue;

		/* tapehead.ini deliberately allows repeated sections/keys and the
		** repository convention is last-value-wins. Keep scanning after every
		** recognized assignment instead of locking in the first one. */
		if (!_stricmp(value, "false") || !_stricmp(value, "no") ||
			!_stricmp(value, "off") || !strcmp(value, "0"))
		{
			cached = 0;
		}
		else if (!_stricmp(value, "true") || !_stricmp(value, "yes") ||
			!_stricmp(value, "on") || !strcmp(value, "1"))
		{
			cached = 1;
		}
	}

	fclose(f);
	return cached != 0;
}

static inline bool tapeheadTrackUsesIndependentTransportVisual(
	bool featureEnabled, bool transportRunning, bool fastTrackEnabled,
	bool individualLengthEnabled, bool frozen)
{
	return featureEnabled && transportRunning &&
		(fastTrackEnabled || individualLengthEnabled || frozen);
}

static inline int32_t tapeheadTransportVisualPageStart(int32_t playbackRow,
	int32_t rowsOnScreen)
{
	if (playbackRow <= 0 || rowsOnScreen <= 0)
		return 0;
	return (playbackRow / rowsOnScreen) * rowsOnScreen;
}
