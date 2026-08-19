#pragma once

#include <stdint.h>
#include "ft2_video_damage.h"

/* Region-capable forms of Tapehead's existing HD filters. A complete-frame
** rectangle is byte-for-byte equivalent to the original implementations. */
static inline void tapeheadScale2xRoundRegion(const uint32_t *source,
	int32_t sourceW, int32_t sourceH, uint32_t *destination,
	const tapeheadVideoDamageRect_t *rect)
{
	const int32_t destinationW = sourceW * 2;
	const int32_t endX = rect->x + rect->w;
	const int32_t endY = rect->y + rect->h;
	for (int32_t y = rect->y; y < endY; y++)
	{
		const uint32_t *srcPrev = &source[((y > 0) ? y-1 : y) * sourceW];
		const uint32_t *srcCurr = &source[y * sourceW];
		const uint32_t *srcNext = &source[((y < sourceH-1) ? y+1 : y) * sourceW];
		uint32_t *dst0 = &destination[(y * 2) * destinationW];
		uint32_t *dst1 = dst0 + destinationW;

		for (int32_t x = rect->x; x < endX; x++)
		{
			const int32_t xPrev = (x > 0) ? x-1 : x;
			const int32_t xNext = (x < sourceW-1) ? x+1 : x;
			const uint32_t b = srcPrev[x];
			const uint32_t d = srcCurr[xPrev];
			const uint32_t e = srcCurr[x];
			const uint32_t f = srcCurr[xNext];
			const uint32_t h = srcNext[x];
			const int32_t dx = x * 2;

			dst0[dx+0] = (d == b && b != f && d != h) ? d : e;
			dst0[dx+1] = (b == f && b != d && f != h) ? f : e;
			dst1[dx+0] = (d == h && d != b && h != f) ? d : e;
			dst1[dx+1] = (h == f && d != h && b != f) ? f : e;
		}
	}
}

static inline void tapeheadScale3xRoundRegion(const uint32_t *source,
	int32_t sourceW, int32_t sourceH, uint32_t *destination,
	const tapeheadVideoDamageRect_t *rect)
{
	const int32_t destinationW = sourceW * 3;
	const int32_t endX = rect->x + rect->w;
	const int32_t endY = rect->y + rect->h;
	for (int32_t y = rect->y; y < endY; y++)
	{
		const uint32_t *srcPrev = &source[((y > 0) ? y-1 : y) * sourceW];
		const uint32_t *srcCurr = &source[y * sourceW];
		const uint32_t *srcNext = &source[((y < sourceH-1) ? y+1 : y) * sourceW];
		uint32_t *dst0 = &destination[(y * 3) * destinationW];
		uint32_t *dst1 = dst0 + destinationW;
		uint32_t *dst2 = dst1 + destinationW;

		for (int32_t x = rect->x; x < endX; x++)
		{
			const int32_t xPrev = (x > 0) ? x-1 : x;
			const int32_t xNext = (x < sourceW-1) ? x+1 : x;
			const uint32_t a = srcPrev[xPrev];
			const uint32_t b = srcPrev[x];
			const uint32_t c = srcPrev[xNext];
			const uint32_t d = srcCurr[xPrev];
			const uint32_t e = srcCurr[x];
			const uint32_t f = srcCurr[xNext];
			const uint32_t g = srcNext[xPrev];
			const uint32_t h = srcNext[x];
			const uint32_t i = srcNext[xNext];
			const int32_t dx = x * 3;

			dst0[dx+0] = (d == b && d != h && b != f) ? d : e;
			dst0[dx+1] = (((d == b && d != h && b != f && e != c) ||
				(b == f && b != d && f != h && e != a))) ? b : e;
			dst0[dx+2] = (b == f && b != d && f != h) ? f : e;

			dst1[dx+0] = (((d == b && d != h && b != f && e != g) ||
				(d == h && d != b && h != f && e != a))) ? d : e;
			dst1[dx+1] = e;
			dst1[dx+2] = (((b == f && b != d && f != h && e != i) ||
				(h == f && d != h && b != f && e != c))) ? f : e;

			dst2[dx+0] = (d == h && d != b && h != f) ? d : e;
			dst2[dx+1] = (((d == h && d != b && h != f && e != i) ||
				(h == f && d != h && b != f && e != g))) ? h : e;
			dst2[dx+2] = (h == f && d != h && b != f) ? f : e;
		}
	}
}

static inline void tapeheadScale2xCrispRegion(const uint32_t *source,
	int32_t sourceW, int32_t sourceH, uint32_t *destination,
	const tapeheadVideoDamageRect_t *rect)
{
	const int32_t destinationW = sourceW * 2;
	const int32_t endX = rect->x + rect->w;
	const int32_t endY = rect->y + rect->h;
	for (int32_t y = rect->y; y < endY; y++)
	{
		const uint32_t *srcPrev = &source[((y > 0) ? y-1 : y) * sourceW];
		const uint32_t *srcCurr = &source[y * sourceW];
		const uint32_t *srcNext = &source[((y < sourceH-1) ? y+1 : y) * sourceW];
		uint32_t *dst0 = &destination[(y * 2) * destinationW];
		uint32_t *dst1 = dst0 + destinationW;

		for (int32_t x = rect->x; x < endX; x++)
		{
			const int32_t xPrev = (x > 0) ? x-1 : x;
			const int32_t xNext = (x < sourceW-1) ? x+1 : x;
			const uint32_t up = srcPrev[x];
			const uint32_t left = srcCurr[xPrev];
			const uint32_t e = srcCurr[x];
			const uint32_t right = srcCurr[xNext];
			const uint32_t down = srcNext[x];
			const int32_t dx = x * 2;

			dst0[dx+0] = e;
			dst0[dx+1] = e;
			dst1[dx+0] = e;
			dst1[dx+1] = e;

			if (up != down && left != right)
			{
				if (up == left && up != e) dst0[dx+0] = up;
				if (up == right && up != e) dst0[dx+1] = up;
				if (down == left && down != e) dst1[dx+0] = down;
				if (down == right && down != e) dst1[dx+1] = down;
			}
		}
	}
}

static inline void tapeheadScale3xCrispRegion(const uint32_t *source,
	int32_t sourceW, int32_t sourceH, uint32_t *destination,
	const tapeheadVideoDamageRect_t *rect)
{
	const int32_t destinationW = sourceW * 3;
	const int32_t endX = rect->x + rect->w;
	const int32_t endY = rect->y + rect->h;
	for (int32_t y = rect->y; y < endY; y++)
	{
		const uint32_t *srcPrev = &source[((y > 0) ? y-1 : y) * sourceW];
		const uint32_t *srcCurr = &source[y * sourceW];
		const uint32_t *srcNext = &source[((y < sourceH-1) ? y+1 : y) * sourceW];
		uint32_t *dst0 = &destination[(y * 3) * destinationW];
		uint32_t *dst1 = dst0 + destinationW;
		uint32_t *dst2 = dst1 + destinationW;

		for (int32_t x = rect->x; x < endX; x++)
		{
			const int32_t xPrev = (x > 0) ? x-1 : x;
			const int32_t xNext = (x < sourceW-1) ? x+1 : x;
			const uint32_t up = srcPrev[x];
			const uint32_t left = srcCurr[xPrev];
			const uint32_t e = srcCurr[x];
			const uint32_t right = srcCurr[xNext];
			const uint32_t down = srcNext[x];
			const int32_t dx = x * 3;

			dst0[dx+0] = e; dst0[dx+1] = e; dst0[dx+2] = e;
			dst1[dx+0] = e; dst1[dx+1] = e; dst1[dx+2] = e;
			dst2[dx+0] = e; dst2[dx+1] = e; dst2[dx+2] = e;

			if (up != down && left != right)
			{
				if (up == left && up != e) dst0[dx+0] = up;
				if (up == right && up != e) dst0[dx+2] = up;
				if (down == left && down != e) dst2[dx+0] = down;
				if (down == right && down != e) dst2[dx+2] = down;
			}
		}
	}
}
