#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_video_damage.h"
#include "ft2_video_scaler.h"

typedef void (*scaleRegionFunc_t)(const uint32_t *, int32_t, int32_t,
	uint32_t *, const tapeheadVideoDamageRect_t *);

static void fillSource(uint32_t *pixels, int32_t width, int32_t height)
{
	for (int32_t y = 0; y < height; y++)
	{
		for (int32_t x = 0; x < width; x++)
		{
			/* Repeated regions plus diagonals exercise both HD styles. */
			const uint32_t band = (uint32_t)(((x / 5) + (y / 7)) & 7);
			pixels[(y * width) + x] = 0xFF000000u |
				(band * 0x001D2B39u);
		}
	}
}

static bool planContains(const tapeheadVideoDamagePlan_t *plan,
	int32_t x, int32_t y)
{
	for (uint16_t i = 0; i < plan->count; i++)
	{
		const tapeheadVideoDamageRect_t *rect = &plan->rects[i];
		if (x >= rect->x && x < rect->x + rect->w &&
			y >= rect->y && y < rect->y + rect->h)
		{
			return true;
		}
	}
	return false;
}

static void testDamagePlanner(void)
{
	enum { WIDTH = 160, HEIGHT = 128 };
	uint32_t previous[WIDTH * HEIGHT];
	uint32_t current[WIDTH * HEIGHT];
	memset(previous, 0x11, sizeof (previous));
	memcpy(current, previous, sizeof (current));

	tapeheadVideoDamagePlan_t plan;
	assert(!tapeheadVideoDamagePlan(current, previous, WIDTH, HEIGHT, &plan));
	assert(tapeheadVideoDamagePlan(current, NULL, WIDTH, HEIGHT, &plan));
	assert(plan.fullFrame && plan.count == 1);

	current[(64 * WIDTH) + 80] ^= 0x00FFFFFFu;
	assert(tapeheadVideoDamagePlan(current, previous, WIDTH, HEIGHT, &plan));
	assert(!plan.fullFrame && plan.count > 0);
	assert(planContains(&plan, 80, 64));
	assert(plan.sourcePixels < (WIDTH * HEIGHT) / 4);

	memset(current, 0x22, sizeof (current));
	assert(tapeheadVideoDamagePlan(current, previous, WIDTH, HEIGHT, &plan));
	assert(plan.fullFrame && plan.sourcePixels == WIDTH * HEIGHT);
}

static void drawOutline(uint32_t *pixels, int32_t width, int32_t height,
	int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
	for (int32_t px = x; px < x + w && px < width; px++)
	{
		pixels[(y * width) + px] = color;
		pixels[((y + h - 1) * width) + px] = color;
	}
	for (int32_t py = y; py < y + h && py < height; py++)
	{
		pixels[(py * width) + x] = color;
		pixels[(py * width) + x + w - 1] = color;
	}
}

static void testEightMovingHeadsStayPartial(void)
{
	enum { WIDTH = 632, HEIGHT = 400 };
	const size_t pixelCount = WIDTH * HEIGHT;
	uint32_t *previous = calloc(pixelCount, sizeof (uint32_t));
	uint32_t *current = calloc(pixelCount, sizeof (uint32_t));
	assert(previous != NULL && current != NULL);

	for (int32_t lane = 0; lane < 8; lane++)
	{
		const int32_t x = 29 + (lane * 75);
		const int32_t oldY = 180 + ((lane & 3) * 16);
		drawOutline(previous, WIDTH, HEIGHT, x, oldY, 72, 8, 0xFFAA0000u);
		drawOutline(current, WIDTH, HEIGHT, x, oldY + 8, 72, 8,
			0xFFAA0000u);
	}

	tapeheadVideoDamagePlan_t plan;
	assert(tapeheadVideoDamagePlan(current, previous, WIDTH, HEIGHT, &plan));
	assert(!plan.fullFrame);
	/* This representative eight-head move rebuilds less than one quarter of
	** the logical screen (and therefore less than one quarter of 3x output). */
	assert(plan.sourcePixels < pixelCount / 4);
	const uint32_t permille = (uint32_t)
		(((uint64_t)plan.sourcePixels * 1000) / pixelCount);
	printf("Representative eight-head damage: %u.%u%% of full frame.\n",
		permille / 10, permille % 10);

	free(current);
	free(previous);
}

static void verifyIncrementalScale(int32_t scale, scaleRegionFunc_t scaler)
{
	enum { WIDTH = 127, HEIGHT = 95 };
	const size_t sourcePixels = WIDTH * HEIGHT;
	const size_t outputPixels = sourcePixels * scale * scale;
	uint32_t *previous = malloc(sourcePixels * sizeof (uint32_t));
	uint32_t *current = malloc(sourcePixels * sizeof (uint32_t));
	uint32_t *incremental = malloc(outputPixels * sizeof (uint32_t));
	uint32_t *expected = malloc(outputPixels * sizeof (uint32_t));
	assert(previous != NULL && current != NULL &&
		incremental != NULL && expected != NULL);

	fillSource(previous, WIDTH, HEIGHT);
	memcpy(current, previous, sourcePixels * sizeof (uint32_t));
	const tapeheadVideoDamageRect_t full = { 0, 0, WIDTH, HEIGHT };
	scaler(previous, WIDTH, HEIGHT, incremental, &full);

	/* Include corners and exact tile boundaries so neighbor-dependent output
	** proves the expanded damage plan is sufficient. */
	static const int32_t points[][2] =
	{
		{ 0, 0 }, { 15, 15 }, { 16, 16 }, { 63, 31 },
		{ 64, 32 }, { WIDTH - 1, HEIGHT - 1 }
	};
	for (size_t i = 0; i < sizeof (points) / sizeof (points[0]); i++)
		current[(points[i][1] * WIDTH) + points[i][0]] ^= 0x0000FFFFu;

	tapeheadVideoDamagePlan_t plan;
	assert(tapeheadVideoDamagePlan(current, previous, WIDTH, HEIGHT, &plan));
	assert(!plan.fullFrame);
	for (uint16_t i = 0; i < plan.count; i++)
		scaler(current, WIDTH, HEIGHT, incremental, &plan.rects[i]);
	scaler(current, WIDTH, HEIGHT, expected, &full);
	assert(!memcmp(incremental, expected, outputPixels * sizeof (uint32_t)));

	free(expected);
	free(incremental);
	free(current);
	free(previous);
}

int main(void)
{
	testDamagePlanner();
	testEightMovingHeadsStayPartial();
	verifyIncrementalScale(2, tapeheadScale2xRoundRegion);
	verifyIncrementalScale(3, tapeheadScale3xRoundRegion);
	verifyIncrementalScale(2, tapeheadScale2xCrispRegion);
	verifyIncrementalScale(3, tapeheadScale3xCrispRegion);
	puts("Video damage planning and incremental HD scaling tests passed.");
	return 0;
}
