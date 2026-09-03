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
	if (argc != 3)
		return 2;

	editor.configFileLocationU = argv[1];
	loadTapeheadConfig();
	const long expected = strtol(argv[2], NULL, 10);
	if (tapeheadConfig.audioBackend != expected)
	{
		fprintf(stderr, "Expected backend %ld, got %u (%s)\n", expected,
			tapeheadConfig.audioBackend,
			tapeheadAudioBackendName(tapeheadConfig.audioBackend));
		return 1;
	}

	return 0;
}
