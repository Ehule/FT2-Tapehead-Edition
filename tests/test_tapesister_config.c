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
	if (argc != 4)
		return 2;
	editor.configFileLocationU = argv[1];
	loadTapeheadConfig();
	if (strcmp(tapeheadConfig.tapeSisterExchangePath, argv[2]) != 0 ||
		strcmp(tapeheadConfig.tapeSisterExecutablePath, argv[3]) != 0)
	{
		fprintf(stderr, "TapeSister paths were not preserved\n");
		return 1;
	}
	if (sizeof (tapeheadConfig.tapeSisterExchangePath) < 4096 ||
		sizeof (tapeheadConfig.tapeSisterExecutablePath) < 4096)
	{
		fprintf(stderr, "TapeSister path storage is too small\n");
		return 1;
	}

	saveTapeSisterConfigPaths();
	tapeheadConfig.tapeSisterExchangePath[0] = '\0';
	tapeheadConfig.tapeSisterExecutablePath[0] = '\0';
	loadTapeheadConfig();
	if (strcmp(tapeheadConfig.tapeSisterExchangePath, argv[2]) != 0 ||
		strcmp(tapeheadConfig.tapeSisterExecutablePath, argv[3]) != 0)
	{
		fprintf(stderr, "TapeSister UI paths were not saved\n");
		return 1;
	}
	return 0;
}
