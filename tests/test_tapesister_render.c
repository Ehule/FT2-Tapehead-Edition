#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ft2_tapesister_render.h"

#define CHECK(condition) do { if (!(condition)) { \
	fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
	return 1; \
} } while (0)

static int testPlans(void)
{
	tapeheadRenderPlan_t plan;
	CHECK(tapeheadRenderPlanInit(&plan, TAPEHEAD_RENDER_PATTERN_MIX,
		8, 3, 0x2A, 4, 8, 125, 6, 48000, 32));
	CHECK(plan.startOrder == 3 && plan.stopOrder == 3);
	CHECK(plan.soloChannel == -1);
	CHECK(strcmp(plan.filename, "Pattern_2A_Order_03_Mix.wav") == 0);

	CHECK(tapeheadRenderPlanInit(&plan, TAPEHEAD_RENDER_PATTERN_TRACK,
		8, 3, 0x2A, 4, 8, 125, 6, 44100, 16));
	CHECK(plan.soloChannel == 4);
	CHECK(strcmp(plan.filename, "Pattern_2A_Order_03_Track_05.wav") == 0);

	CHECK(tapeheadRenderPlanInit(&plan, TAPEHEAD_RENDER_SONG_TRACK,
		8, 3, 0x2A, 4, 8, 125, 6, 44100, 16));
	CHECK(plan.startOrder == 0 && plan.stopOrder == 7 && plan.soloChannel == 4);
	CHECK(plan.pattern == -1);
	CHECK(strcmp(plan.filename, "Song_Track_05.wav") == 0);

	CHECK(tapeheadRenderPlanInit(&plan, TAPEHEAD_RENDER_SONG_MIX,
		8, 3, 0x2A, 4, 8, 125, 6, 44100, 16));
	CHECK(plan.soloChannel == -1);
	CHECK(plan.pattern == -1);
	CHECK(strcmp(plan.filename, "Song_Mix.wav") == 0);
	CHECK(tapeheadRenderPlanInit(&plan, TAPEHEAD_RENDER_SONG_MIX,
		256, 255, 0xFF, 0, 32, 125, 6, 96000, 32));
	CHECK(plan.startOrder == 0 && plan.stopOrder == 255);

	CHECK(!tapeheadRenderPlanInit(&plan, TAPEHEAD_RENDER_SONG_MIX,
		0, 0, 0, 0, 8, 125, 6, 44100, 16));
	CHECK(!tapeheadRenderPlanInit(&plan, TAPEHEAD_RENDER_SONG_TRACK,
		8, 3, 0, 8, 8, 125, 6, 44100, 16));
	CHECK(!tapeheadRenderPlanInit(&plan, TAPEHEAD_RENDER_SONG_MIX,
		8, 3, 0, 0, 8, 125, 6, 44100, 24));
	return 0;
}

static int testMetadata(void)
{
	tapeheadRenderPlan_t plan;
	CHECK(tapeheadRenderPlanInit(&plan, TAPEHEAD_RENDER_PATTERN_TRACK,
		4, 2, 7, 1, 8, 140, 3, 48000, 32));
	FILE *file = tmpfile();
	CHECK(file != NULL);
	CHECK(tapeheadRenderWriteMetadata(file, &plan, 120000));
	rewind(file);
	char contents[2048];
	const size_t length = fread(contents, 1, sizeof (contents) - 1, file);
	contents[length] = '\0';
	fclose(file);
	CHECK(strstr(contents, "TAPEHEAD_RENDER 1\n") != NULL);
	CHECK(strstr(contents, "scope=pattern_track\n") != NULL);
	CHECK(strstr(contents, "track=2\n") != NULL);
	CHECK(strstr(contents, "sample_rate=48000\n") != NULL);
	CHECK(strstr(contents, "frames=120000\n") != NULL);
	CHECK(strstr(contents, "duration_ms=2500\n") != NULL);

	CHECK(tapeheadRenderPlanInit(&plan, TAPEHEAD_RENDER_SONG_MIX,
		4, 2, 7, 1, 8, 140, 3, 48000, 32));
	file = tmpfile();
	CHECK(file != NULL);
	CHECK(tapeheadRenderWriteMetadata(file, &plan, 48000));
	rewind(file);
	const size_t songLength = fread(contents, 1, sizeof (contents) - 1, file);
	contents[songLength] = '\0';
	fclose(file);
	CHECK(strstr(contents, "scope=song_mix\n") != NULL);
	CHECK(strstr(contents, "pattern=-1\n") != NULL);
	CHECK(strstr(contents, "track=0\n") != NULL);
	CHECK(strstr(contents, "duration_ms=1000\n") != NULL);
	return 0;
}

int main(void)
{
	CHECK(TAPEHEAD_RENDER_MAX_TAPESISTER_FRAMES == UINT64_C(100000000));
	if (testPlans() != 0 || testMetadata() != 0)
		return 1;
	puts("TapeSister audio-render planning and metadata tests passed.");
	return 0;
}
