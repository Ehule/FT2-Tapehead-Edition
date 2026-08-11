#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ft2_sysreqs.h"

uint16_t textWidth(const char *text)
{
	return (uint16_t)(strlen(text) * 6);
}

static void assertSafe(const systemRequestLayout_t *layout)
{
	assert(layout->lineCount > 0);
	assert(layout->frameX + layout->frameWidth <= 632);
	assert(layout->frameY + layout->frameHeight <= 400);
	assert(layout->headlineX >= layout->frameX);
	assert(layout->buttonY > layout->textY +
		(layout->lineCount - 1) * 12);
	for (uint16_t i = 0; i < layout->lineCount; i++)
	{
		assert(layout->lineWidths[i] <= 300);
		assert(layout->lineX[i] >= (int16_t)layout->frameX);
		assert((uint16_t)layout->lineX[i] + layout->lineWidths[i] <=
			layout->frameX + layout->frameWidth);
	}
}

int main(void)
{
	systemRequestLayout_t layout;
	assert(systemRequestCalculateLayout("System message", "Short message", 80,
		249, &layout));
	assert(layout.lineCount == 1);
	assert(layout.frameWidth == 184); /* 14 headline glyphs * 6 + old 100px */
	assert(layout.frameY == 249 && layout.frameHeight == 67);
	assert(layout.textY == 273 && layout.buttonY == 291);
	assertSafe(&layout);

	assert(systemRequestCalculateLayout("System message", "first line\nsecond line",
		80, 249, &layout));
	assert(layout.lineCount == 2);
	assert(!strcmp(layout.lines[0], "first line"));
	assert(!strcmp(layout.lines[1], "second line"));
	assertSafe(&layout);

	static const char *messages[] =
	{
		"Tapehead live bake complete: 0 relocated, 0 duplicates merged, 0 microtonal commands preserved, 0 Sample Matrix launches skipped.",
		"Standard XM live bake complete: 0 relocated, 0 duplicates merged, 0 microtonal commands stripped, 0 Sample Matrix launches skipped.",
		"Tapehead live bake complete: 4294967295 relocated, 4294967295 duplicates merged, 4294967295 microtonal commands preserved, 4294967295 Sample Matrix launches skipped.",
		"Standard XM live bake complete: 4294967295 relocated, 4294967295 duplicates merged, 4294967295 microtonal commands stripped, 4294967295 Sample Matrix launches skipped.",
		"ThisSingleTokenIsDeliberatelyFarTooLongToFitInsideTheContentWidthAndMustBeSplitWithoutEscapingTheFrameMargins"
	};
	for (size_t i = 0; i < sizeof messages / sizeof messages[0]; i++)
	{
		assert(systemRequestCalculateLayout("System message", messages[i], 80,
			249, &layout));
		assert(layout.lineCount > 1);
		assertSafe(&layout);
	}

	puts("System request wrapping tests passed.");
	return 0;
}
