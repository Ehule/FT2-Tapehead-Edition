#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define EXS_MAX_INSTRUMENTS 128
#define EXS_MAX_SAMPLES_PER_INSTRUMENT 16
#define EXS_MAX_ENTRIES (EXS_MAX_INSTRUMENTS * EXS_MAX_SAMPLES_PER_INSTRUMENT)
#define EXS_NAME_CAPACITY 64
#define EXS_PATH_CAPACITY 512

enum
{
	EXS_LOOP_NONE = 0,
	EXS_LOOP_FORWARD = 1,
	EXS_LOOP_PINGPONG = 2
};

typedef struct exsManifestSample_t
{
	uint8_t instrumentIndex, sampleIndex;
	char instrumentName[EXS_NAME_CAPACITY];
	char sampleName[EXS_NAME_CAPACITY];
	char file[EXS_PATH_CAPACITY];
	int32_t lengthFrames, sourceBitDepth;
	int32_t relativeNote, finetune;
	uint32_t defaultVolume, defaultPanning;
	uint8_t loopType;
	int32_t loopStart, loopLength, c4Frequency;
	uint32_t flags;
} exsManifestSample_t;

typedef struct exsManifest_t
{
	uint16_t formatVersion, instrumentCount, sampleCount;
	char sourceModule[EXS_NAME_CAPACITY];
	char exportMode[32];
	exsManifestSample_t *samples;
} exsManifest_t;

void exsManifestInit(exsManifest_t *manifest);
void exsManifestFree(exsManifest_t *manifest);
bool exsManifestParse(FILE *file, exsManifest_t *manifest, char *error,
	size_t errorSize);
bool exsManifestRelativePathIsSafe(const char *path);
void exsStripKnownSampleExtension(char *name);
