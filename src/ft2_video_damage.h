#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Damage is detected on the original 632x400 FT2 surface. Small fixed tiles
** make the scan cache-friendly and avoid requiring every legacy drawing
** primitive to publish dirty rectangles. */
#define TAPEHEAD_VIDEO_DAMAGE_TILE_W 16
#define TAPEHEAD_VIDEO_DAMAGE_TILE_H 16
#define TAPEHEAD_VIDEO_DAMAGE_MAX_TILES 2048
#define TAPEHEAD_VIDEO_DAMAGE_MAX_RECTS 32
#define TAPEHEAD_VIDEO_DAMAGE_FULL_PERCENT 70

typedef struct tapeheadVideoDamageRect_t
{
	int32_t x, y, w, h;
} tapeheadVideoDamageRect_t;

typedef struct tapeheadVideoDamagePlan_t
{
	uint16_t count;
	uint32_t sourcePixels;
	bool fullFrame;
	tapeheadVideoDamageRect_t rects[TAPEHEAD_VIDEO_DAMAGE_MAX_RECTS];
} tapeheadVideoDamagePlan_t;

static inline void tapeheadVideoDamageSetFull(tapeheadVideoDamagePlan_t *plan,
	int32_t width, int32_t height)
{
	plan->count = 1;
	plan->sourcePixels = (uint32_t)(width * height);
	plan->fullFrame = true;
	plan->rects[0].x = 0;
	plan->rects[0].y = 0;
	plan->rects[0].w = width;
	plan->rects[0].h = height;
}

static inline bool tapeheadVideoDamagePlan(const uint32_t *current,
	const uint32_t *previous, int32_t width, int32_t height,
	tapeheadVideoDamagePlan_t *plan)
{
	if (current == NULL || plan == NULL || width <= 0 || height <= 0)
		return false;

	memset(plan, 0, sizeof (*plan));
	if (previous == NULL)
	{
		tapeheadVideoDamageSetFull(plan, width, height);
		return true;
	}

	const int32_t tilesX = (width + TAPEHEAD_VIDEO_DAMAGE_TILE_W - 1) /
		TAPEHEAD_VIDEO_DAMAGE_TILE_W;
	const int32_t tilesY = (height + TAPEHEAD_VIDEO_DAMAGE_TILE_H - 1) /
		TAPEHEAD_VIDEO_DAMAGE_TILE_H;
	const int32_t tileCount = tilesX * tilesY;
	if (tileCount <= 0 || tileCount > TAPEHEAD_VIDEO_DAMAGE_MAX_TILES)
	{
		tapeheadVideoDamageSetFull(plan, width, height);
		return true;
	}

	bool changed[TAPEHEAD_VIDEO_DAMAGE_MAX_TILES] = { false };
	bool expanded[TAPEHEAD_VIDEO_DAMAGE_MAX_TILES] = { false };
	bool anyChanged = false;
	for (int32_t tileY = 0; tileY < tilesY; tileY++)
	{
		const int32_t y0 = tileY * TAPEHEAD_VIDEO_DAMAGE_TILE_H;
		const int32_t tileH = height - y0 < TAPEHEAD_VIDEO_DAMAGE_TILE_H
			? height - y0 : TAPEHEAD_VIDEO_DAMAGE_TILE_H;
		for (int32_t tileX = 0; tileX < tilesX; tileX++)
		{
			const int32_t x0 = tileX * TAPEHEAD_VIDEO_DAMAGE_TILE_W;
			const int32_t tileW = width - x0 < TAPEHEAD_VIDEO_DAMAGE_TILE_W
				? width - x0 : TAPEHEAD_VIDEO_DAMAGE_TILE_W;
			bool tileChanged = false;
			for (int32_t y = 0; y < tileH; y++)
			{
				const int32_t offset = ((y0 + y) * width) + x0;
				if (memcmp(&current[offset], &previous[offset],
					(size_t)tileW * sizeof (uint32_t)) != 0)
				{
					tileChanged = true;
					break;
				}
			}

			if (tileChanged)
			{
				changed[(tileY * tilesX) + tileX] = true;
				anyChanged = true;
			}
		}
	}

	if (!anyChanged)
		return false;

	/* Every HD filter samples the immediate source neighbors. Expanding by one
	** logical tile is conservative, keeps the rectangles non-overlapping after
	** coalescing, and guarantees exact output at damage boundaries. */
	for (int32_t tileY = 0; tileY < tilesY; tileY++)
	{
		for (int32_t tileX = 0; tileX < tilesX; tileX++)
		{
			if (!changed[(tileY * tilesX) + tileX])
				continue;

			const int32_t minY = tileY > 0 ? tileY - 1 : tileY;
			const int32_t maxY = tileY + 1 < tilesY ? tileY + 1 : tileY;
			const int32_t minX = tileX > 0 ? tileX - 1 : tileX;
			const int32_t maxX = tileX + 1 < tilesX ? tileX + 1 : tileX;
			for (int32_t y = minY; y <= maxY; y++)
			{
				for (int32_t x = minX; x <= maxX; x++)
					expanded[(y * tilesX) + x] = true;
			}
		}
	}

	/* Convert horizontal tile runs into rectangles and extend an identical run
	** from the previous tile row vertically. */
	for (int32_t tileY = 0; tileY < tilesY; tileY++)
	{
		int32_t tileX = 0;
		while (tileX < tilesX)
		{
			while (tileX < tilesX && !expanded[(tileY * tilesX) + tileX])
				tileX++;
			if (tileX >= tilesX)
				break;

			const int32_t runStart = tileX;
			while (tileX < tilesX && expanded[(tileY * tilesX) + tileX])
				tileX++;
			const int32_t runEnd = tileX;
			const int32_t x = runStart * TAPEHEAD_VIDEO_DAMAGE_TILE_W;
			const int32_t right = runEnd * TAPEHEAD_VIDEO_DAMAGE_TILE_W < width
				? runEnd * TAPEHEAD_VIDEO_DAMAGE_TILE_W : width;
			const int32_t y = tileY * TAPEHEAD_VIDEO_DAMAGE_TILE_H;
			const int32_t bottom = y + TAPEHEAD_VIDEO_DAMAGE_TILE_H < height
				? y + TAPEHEAD_VIDEO_DAMAGE_TILE_H : height;

			bool merged = false;
			for (uint16_t i = 0; i < plan->count; i++)
			{
				tapeheadVideoDamageRect_t *rect = &plan->rects[i];
				if (rect->x == x && rect->w == right - x &&
					rect->y + rect->h == y)
				{
					rect->h = bottom - rect->y;
					merged = true;
					break;
				}
			}

			if (!merged)
			{
				if (plan->count >= TAPEHEAD_VIDEO_DAMAGE_MAX_RECTS)
				{
					tapeheadVideoDamageSetFull(plan, width, height);
					return true;
				}

				tapeheadVideoDamageRect_t *rect = &plan->rects[plan->count++];
				rect->x = x;
				rect->y = y;
				rect->w = right - x;
				rect->h = bottom - y;
			}
		}
	}

	for (uint16_t i = 0; i < plan->count; i++)
	{
		const tapeheadVideoDamageRect_t *rect = &plan->rects[i];
		plan->sourcePixels += (uint32_t)(rect->w * rect->h);
	}

	const uint32_t fullPixels = (uint32_t)(width * height);
	if ((uint64_t)plan->sourcePixels * 100 >=
		(uint64_t)fullPixels * TAPEHEAD_VIDEO_DAMAGE_FULL_PERCENT)
	{
		tapeheadVideoDamageSetFull(plan, width, height);
	}

	return true;
}
