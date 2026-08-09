#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL_atomic.h>
#include "ft2_config.h"
#include "ft2_midi_map.h"
#include "ft2_structs.h"

editor_t editor;

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

static int expectChannel(int32_t track, uint8_t expected)
{
	const uint8_t actual = tapeheadConfig.midiDubTrackChannels[track - 1] + 1;
	if (actual == expected)
		return 0;

	fprintf(stderr, "track %d: expected MIDI channel %u, got %u\n",
		(int)track, expected, actual);
	return 1;
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "usage: %s /path/to/FT2.CFG expected-brightness\n", argv[0]);
		return 2;
	}

	editor.configFileLocationU = argv[1];
	loadTapeheadConfig();
	if (tapeheadConfig.apc40RGBBrightness != (uint8_t)atoi(argv[2]))
	{
		fprintf(stderr, "expected RGB brightness %s, got %u\n", argv[2],
			tapeheadConfig.apc40RGBBrightness);
		return 1;
	}
	if (!tapeheadConfig.midiPerformanceControl ||
		!tapeheadMidiMapIsEnabled() || tapeheadMidiMapGetBindingCount() != 2)
	{
		fprintf(stderr, "generic MIDI performance mapping config failed\n");
		return 1;
	}
	if (strcmp(tapeheadConfig.midiControlInput, "APC40 mkII") ||
		strcmp(tapeheadConfig.midiControlOutput, "APC40 mkII MIDI Out") ||
		tapeheadConfig.midiProfile != TAPEHEAD_MIDI_PROFILE_APC40_MK2 ||
		!tapeheadConfig.patternJogIncludeFastTracks ||
		tapeheadConfig.patternJogAudition !=
			TAPEHEAD_PATTERN_JOG_AUDITION_MANUAL_PINGPONG ||
		!tapeheadConfig.transportFreezeAudioCut ||
		!tapeheadConfig.transportFreezePedalHold ||
		!tapeheadConfig.transportFreezeNavigationAudition ||
		!tapeheadConfig.transportFreezeResumeRetrigger)
	{
		fprintf(stderr, "control-surface device-name config failed\n");
		return 1;
	}

	int failures = 0;
	for (int32_t track = 1; track <= MAX_CHANNELS; track++)
	{
		uint8_t expected = (uint8_t)(((track - 1) & 15) + 1);
		if (track == 1) expected = 16;
		if (track == 2) expected = 9;
		if (track == 17) expected = 4;
		if (track == 32) expected = 1;
		failures += expectChannel(track, expected);
	}

	if (failures != 0)
		return 1;

	puts("MIDI Dub config routing tests passed (32 tracks).\n");
	return 0;
}
