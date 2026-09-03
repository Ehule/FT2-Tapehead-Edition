#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define TAPEHEAD_RENDER_FILENAME_CAPACITY 128
#define TAPEHEAD_RENDER_MAX_TAPESISTER_FRAMES UINT64_C(100000000)

typedef enum tapeheadRenderScope_t
{
	TAPEHEAD_RENDER_PATTERN_MIX = 0,
	TAPEHEAD_RENDER_PATTERN_TRACK,
	TAPEHEAD_RENDER_SONG_TRACK,
	TAPEHEAD_RENDER_SONG_MIX,
	TAPEHEAD_RENDER_SCOPE_COUNT
} tapeheadRenderScope_t;

typedef struct tapeheadRenderPlan_t
{
	tapeheadRenderScope_t scope;
	uint8_t startOrder, stopOrder, sourceChannels;
	int16_t pattern, soloChannel;
	uint16_t initialBPM, initialSpeed;
	uint32_t sampleRate;
	uint8_t bitDepth;
	char filename[TAPEHEAD_RENDER_FILENAME_CAPACITY];
} tapeheadRenderPlan_t;

const char *tapeheadRenderScopeName(tapeheadRenderScope_t scope);
const char *tapeheadRenderScopeLabel(tapeheadRenderScope_t scope);
bool tapeheadRenderPlanInit(tapeheadRenderPlan_t *plan,
	tapeheadRenderScope_t scope, uint16_t songLength, uint16_t songPosition,
	uint16_t pattern, uint16_t track, uint16_t sourceChannels,
	uint16_t initialBPM, uint16_t initialSpeed, uint32_t sampleRate,
	uint8_t bitDepth);
bool tapeheadRenderWriteMetadata(FILE *file,
	const tapeheadRenderPlan_t *plan, uint64_t renderedFrames);
