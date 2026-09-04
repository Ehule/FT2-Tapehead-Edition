#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

bool tapeheadTestWritePerformanceWavHeader(FILE *file, uint32_t sampleRate,
	uint8_t bitDepth, uint64_t frames);
bool tapeheadTestPerformanceCaptureFeedState(void);

int SDL_AtomicSet(SDL_atomic_t *atomic, int value)
{
	const int previous = atomic->value;
	atomic->value = value;
	return previous;
}

int SDL_AtomicGet(SDL_atomic_t *atomic)
{
	return atomic->value;
}

SDL_bool SDL_AtomicCAS(SDL_atomic_t *atomic, int oldValue, int newValue)
{
	if (atomic->value != oldValue)
		return SDL_FALSE;
	atomic->value = newValue;
	return SDL_TRUE;
}

static uint16_t readU16LE(const uint8_t *source)
{
	return (uint16_t)(source[0] | ((uint16_t)source[1] << 8));
}

static uint32_t readU32LE(const uint8_t *source)
{
	return source[0] | ((uint32_t)source[1] << 8) |
		((uint32_t)source[2] << 16) | ((uint32_t)source[3] << 24);
}

static void testHeader(uint32_t sampleRate, uint8_t bitDepth, uint32_t frames)
{
	FILE *file = tmpfile();
	assert(file != NULL);
	assert(tapeheadTestWritePerformanceWavHeader(file, sampleRate, bitDepth,
		frames));
	assert(fseek(file, 0, SEEK_SET) == 0);

	uint8_t header[44];
	assert(fread(header, 1, sizeof (header), file) == sizeof (header));
	const uint32_t bytesPerFrame = (bitDepth / 8) * 2;
	const uint32_t dataBytes = frames * bytesPerFrame;
	assert(memcmp(&header[0], "RIFF", 4) == 0);
	assert(readU32LE(&header[4]) == 36U + dataBytes);
	assert(memcmp(&header[8], "WAVEfmt ", 8) == 0);
	assert(readU32LE(&header[16]) == 16);
	assert(readU16LE(&header[20]) == (bitDepth == 16 ? 1 : 3));
	assert(readU16LE(&header[22]) == 2);
	assert(readU32LE(&header[24]) == sampleRate);
	assert(readU32LE(&header[28]) == sampleRate * bytesPerFrame);
	assert(readU16LE(&header[32]) == bytesPerFrame);
	assert(readU16LE(&header[34]) == bitDepth);
	assert(memcmp(&header[36], "data", 4) == 0);
	assert(readU32LE(&header[40]) == dataBytes);
	assert(fclose(file) == 0);
}

int main(void)
{
	testHeader(44100, 16, 12345);
	testHeader(96000, 32, 67890);
	assert(tapeheadTestPerformanceCaptureFeedState());

	FILE *file = tmpfile();
	assert(file != NULL);
	assert(!tapeheadTestWritePerformanceWavHeader(file, 0, 32, 1));
	assert(!tapeheadTestWritePerformanceWavHeader(file, 48000, 24, 1));
	assert(!tapeheadTestWritePerformanceWavHeader(file, 48000, 32,
		UINT32_MAX));
	assert(fclose(file) == 0);

	puts("Performance capture state and WAV header tests passed.");
	return 0;
}
