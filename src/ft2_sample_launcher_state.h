#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SAMPLE_LAUNCHER_BANK_COUNT 8
#define SAMPLE_LAUNCHER_TILES_PER_BANK 32
#define SAMPLE_LAUNCHER_MAX_TILES (SAMPLE_LAUNCHER_BANK_COUNT * SAMPLE_LAUNCHER_TILES_PER_BANK)
#define SAMPLE_LAUNCHER_MAX_POLY 4
#define SAMPLE_LAUNCHER_QUEUE_MAX 4
#define SAMPLE_LAUNCHER_MAX_ACTIONS 10

typedef enum sampleLauncherActionType_t
{
	SAMPLE_LAUNCHER_ACTION_NONE = 0,
	SAMPLE_LAUNCHER_ACTION_STOP_Q,
	SAMPLE_LAUNCHER_ACTION_START_Q,
	SAMPLE_LAUNCHER_ACTION_STOP_POLY,
	SAMPLE_LAUNCHER_ACTION_START_POLY
} sampleLauncherActionType_t;

typedef struct sampleLauncherAction_t
{
	sampleLauncherActionType_t type;
	int8_t voice;
	int16_t tile;
} sampleLauncherAction_t;

typedef struct sampleLauncherState_t
{
	int16_t qCurrent;
	int16_t qQueue[SAMPLE_LAUNCHER_QUEUE_MAX];
	uint8_t qQueueCount;
	bool qStopPending;
	int16_t polyTile[SAMPLE_LAUNCHER_MAX_POLY];
	bool polyStopPending[SAMPLE_LAUNCHER_MAX_POLY];
	int16_t polyStartQueue[SAMPLE_LAUNCHER_MAX_POLY];
	uint8_t polyStartCount;
} sampleLauncherState_t;

void sampleLauncherStateInit(sampleLauncherState_t *state);
bool sampleLauncherStateRequestQ(sampleLauncherState_t *state, uint16_t tile);
bool sampleLauncherStateTogglePoly(sampleLauncherState_t *state, uint16_t tile);
uint8_t sampleLauncherStateHardStop(sampleLauncherState_t *state, uint16_t tile,
	sampleLauncherAction_t actions[SAMPLE_LAUNCHER_MAX_ACTIONS]);
uint8_t sampleLauncherStateCommitBoundary(sampleLauncherState_t *state,
	sampleLauncherAction_t actions[SAMPLE_LAUNCHER_MAX_ACTIONS]);
int8_t sampleLauncherStateGetQQueuePos(const sampleLauncherState_t *state,
	uint16_t tile);
int8_t sampleLauncherStateGetPolySlot(const sampleLauncherState_t *state,
	uint16_t tile);
bool sampleLauncherStatePolyStopPending(const sampleLauncherState_t *state,
	uint16_t tile);
bool sampleLauncherStatePolyStartPending(const sampleLauncherState_t *state,
	uint16_t tile);
