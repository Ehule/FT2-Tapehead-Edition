#include <stdio.h>
#include <stdint.h>
#include "ft2_config.h"
#include "ft2_structs.h"

editor_t editor;

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
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s /path/to/FT2.CFG\n", argv[0]);
		return 2;
	}

	editor.configFileLocationU = argv[1];
	loadTapeheadConfig();

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
