#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL_atomic.h>

#include "ft2_config.h"
#include "ft2_structs.h"

editor_t editor;

void setUserPatternColor(uint8_t field, uint32_t rgb) { (void)field; (void)rgb; }
void getUserPatternColors(uint32_t colors[12]) { memset(colors, 0, sizeof (uint32_t) * 12); }

void SDLCALL SDL_AtomicLock(SDL_SpinLock *lock)
{
	while (__sync_lock_test_and_set(lock, 1))
	{
	}
}

void SDLCALL SDL_AtomicUnlock(SDL_SpinLock *lock)
{
	__sync_lock_release(lock);
}

int main(int argc, char **argv)
{
	if (argc != 5)
		return 2;

	editor.configFileLocationU = argv[1];
	loadTapeheadConfig();
	const bool expected = !strcmp(argv[2], "true");
	if (tapeheadConfig.showSplashScreen != expected)
	{
		fprintf(stderr, "expected showSplashScreen=%s, got %s\n", argv[2],
			tapeheadConfig.showSplashScreen ? "true" : "false");
		return 1;
	}

	const bool expectedFastTracksLEN = !strcmp(argv[3], "true");
	if (tapeheadConfig.fastTracksUseTrackLengths != expectedFastTracksLEN)
	{
		fprintf(stderr, "expected FastTracksUseTrackLengths=%s, got %s\n",
			argv[3], tapeheadConfig.fastTracksUseTrackLengths ? "true" : "false");
		return 1;
	}

	const uint16_t expectedTrackLengthMax = (uint16_t)strtoul(argv[4], NULL, 10);
	if (tapeheadConfig.trackLengthControlMax != expectedTrackLengthMax)
	{
		fprintf(stderr, "expected TrackLengthControlMax=%u, got %u\n",
			expectedTrackLengthMax, tapeheadConfig.trackLengthControlMax);
		return 1;
	}

	saveTapeheadPatternColorMode();

	return 0;
}
