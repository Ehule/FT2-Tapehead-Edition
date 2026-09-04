#include <stdio.h>
#include "ft2_replayer.h"

#define CHECK(condition) do { if (!(condition)) { \
	fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
	return 1; \
} } while (0)

int main(void)
{
	tapeheadBlockLoopSpec_t spec;
	CHECK(tapeheadBlockLoopSpecInit(&spec, 0x2A, 64, 16, 32, 2, 4,
		8, 125, 6));
	CHECK(spec.pattern == 0x2A);
	CHECK(spec.rowStart == 16 && spec.rowEnd == 32);
	CHECK(spec.channelStart == 2 && spec.channelEnd == 4);
	CHECK(spec.initialBPM == 125 && spec.initialSpeed == 6);
	CHECK(!tapeheadBlockLoopSpecInit(&spec, 0, 64, 16, 16, 2, 4,
		8, 125, 6));
	CHECK(!tapeheadBlockLoopSpecInit(&spec, 0, 64, 16, 65, 2, 4,
		8, 125, 6));
	CHECK(!tapeheadBlockLoopSpecInit(&spec, 0, 64, 16, 32, 4, 2,
		8, 125, 6));
	CHECK(!tapeheadBlockLoopSpecInit(&spec, 0, 64, 16, 32, 2, 8,
		8, 125, 6));
	CHECK(!tapeheadBlockLoopSpecInit(&spec, 0, 64, 16, 32, 2, 4,
		8, 125, 0));
	puts("Block Loop selection contract tests passed.");
	return 0;
}
