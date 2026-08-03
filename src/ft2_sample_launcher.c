#include <stdio.h>
#include <string.h>
#include "ft2_audio.h"
#include "ft2_header.h"
#include "ft2_sample_ed.h"
#include "ft2_sample_launcher.h"
#include "ft2_structs.h"

typedef struct sampleLauncherTile_t
{
	sample_t sample;
	uint8_t outputBus;
} sampleLauncherTile_t;

static sampleLauncherTile_t tiles[SAMPLE_LAUNCHER_MAX_TILES];
static sampleLauncherState_t launcherState;
static uint8_t tileCount;
static bool initialized;

static void ensureInitialized(void)
{
	if (!initialized)
	{
		sampleLauncherStateInit(&launcherState);
		initialized = true;
	}
}

static void executeBoundaryActions(void)
{
	sampleLauncherAction_t actions[SAMPLE_LAUNCHER_MAX_ACTIONS];
	const uint8_t count = sampleLauncherStateCommitBoundary(&launcherState,
		actions);
	for (uint8_t i = 0; i < count; i++)
	{
		const sampleLauncherAction_t *action = &actions[i];
		if (action->type == SAMPLE_LAUNCHER_ACTION_STOP_Q ||
			action->type == SAMPLE_LAUNCHER_ACTION_STOP_POLY)
		{
			audioSampleLauncherStop((uint8_t)action->voice);
		}
		else if ((action->type == SAMPLE_LAUNCHER_ACTION_START_Q ||
			action->type == SAMPLE_LAUNCHER_ACTION_START_POLY) &&
			action->tile >= 0 && action->tile < tileCount)
		{
			const sampleLauncherTile_t *tile = &tiles[action->tile];
			audioSampleLauncherTrigger((uint8_t)action->voice,
				&tile->sample, tile->outputBus);
		}
	}
}

uint8_t sampleLauncherGetTileCount(void)
{
	return tileCount;
}

const char *sampleLauncherGetTileName(uint8_t tile)
{
	return tile < tileCount ? tiles[tile].sample.name : "";
}

uint8_t sampleLauncherGetTileBus(uint8_t tile)
{
	return tile < tileCount ? tiles[tile].outputBus : 0;
}

void sampleLauncherCycleTileBus(uint8_t tile, uint8_t busCount)
{
	if (tile >= tileCount)
		return;
	if (busCount < 1)
		busCount = 1;
	if (busCount > TAPEHEAD_MAX_OUTPUT_BUSES)
		busCount = TAPEHEAD_MAX_OUTPUT_BUSES;
	tiles[tile].outputBus = (uint8_t)((tiles[tile].outputBus + 1) % busCount);
	if (launcherState.qCurrent == tile)
		audioSampleLauncherSetOutputBus(0, tiles[tile].outputBus);
	const int8_t polySlot = sampleLauncherStateGetPolySlot(&launcherState, tile);
	if (polySlot >= 0)
		audioSampleLauncherSetOutputBus((uint8_t)(polySlot + 1),
			tiles[tile].outputBus);
}

int16_t sampleLauncherGetQCurrent(void)
{
	ensureInitialized();
	return launcherState.qCurrent;
}

bool sampleLauncherQStopPending(void)
{
	ensureInitialized();
	return launcherState.qStopPending;
}

int8_t sampleLauncherGetQQueuePos(uint8_t tile)
{
	ensureInitialized();
	return sampleLauncherStateGetQQueuePos(&launcherState, tile);
}

int8_t sampleLauncherGetPolySlot(uint8_t tile)
{
	ensureInitialized();
	return sampleLauncherStateGetPolySlot(&launcherState, tile);
}

bool sampleLauncherPolyStopPending(uint8_t tile)
{
	ensureInitialized();
	return sampleLauncherStatePolyStopPending(&launcherState, tile);
}

bool sampleLauncherPolyStartPending(uint8_t tile)
{
	ensureInitialized();
	return sampleLauncherStatePolyStartPending(&launcherState, tile);
}

bool sampleLauncherRequestQ(uint8_t tile)
{
	ensureInitialized();
	if (tile >= tileCount || !sampleLauncherStateRequestQ(&launcherState, tile))
		return false;

	if (!songPlaying)
		executeBoundaryActions();
	return true;
}

bool sampleLauncherTogglePoly(uint8_t tile)
{
	ensureInitialized();
	if (tile >= tileCount || !sampleLauncherStateTogglePoly(&launcherState, tile))
		return false;

	if (!songPlaying)
		executeBoundaryActions();
	return true;
}

void sampleLauncherHandleBoundary(void)
{
	ensureInitialized();
	executeBoundaryActions();
}

void sampleLauncherReset(void)
{
	ensureInitialized();
	audioSampleLauncherStopAll();
	sampleLauncherStateInit(&launcherState);
}

void sampleLauncherAdoptDecodedFolder(sample_t *samples, uint32_t count)
{
	if (samples == NULL)
		return;
	if (count > SAMPLE_LAUNCHER_MAX_TILES)
		count = SAMPLE_LAUNCHER_MAX_TILES;

	ensureInitialized();
	lockMixerCallback();
	sampleLauncherReset();
	for (uint8_t i = 0; i < tileCount; i++)
		freeSmpData(&tiles[i].sample);
	memset(tiles, 0, sizeof (tiles));

	for (uint32_t i = 0; i < count; i++)
	{
		memcpy(&tiles[i].sample, &samples[i], sizeof (sample_t));
		memset(&samples[i], 0, sizeof (sample_t));
		tiles[i].outputBus = 0;
	}
	tileCount = (uint8_t)count;
	unlockMixerCallback();
}

void sampleLauncherFree(void)
{
	ensureInitialized();
	lockMixerCallback();
	sampleLauncherReset();
	for (uint8_t i = 0; i < tileCount; i++)
		freeSmpData(&tiles[i].sample);
	memset(tiles, 0, sizeof (tiles));
	tileCount = 0;
	unlockMixerCallback();
}
