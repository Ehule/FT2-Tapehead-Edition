#include <string.h>
#include "ft2_sample_launcher_state.h"

static void appendAction(sampleLauncherAction_t *actions, uint8_t *count,
	sampleLauncherActionType_t type, int8_t voice, int16_t tile)
{
	if (*count >= SAMPLE_LAUNCHER_MAX_ACTIONS)
		return;

	actions[*count].type = type;
	actions[*count].voice = voice;
	actions[*count].tile = tile;
	(*count)++;
}

void sampleLauncherStateInit(sampleLauncherState_t *state)
{
	memset(state, 0, sizeof (*state));
	state->qCurrent = -1;
	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_QUEUE_MAX; i++)
		state->qQueue[i] = -1;
	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_MAX_POLY; i++)
	{
		state->polyTile[i] = -1;
		state->polyStartQueue[i] = -1;
	}
}

int8_t sampleLauncherStateGetQQueuePos(const sampleLauncherState_t *state,
	uint16_t tile)
{
	for (uint8_t i = 0; i < state->qQueueCount; i++)
	{
		if (state->qQueue[i] == tile)
			return (int8_t)i;
	}

	return -1;
}

int8_t sampleLauncherStateGetPolySlot(const sampleLauncherState_t *state,
	uint16_t tile)
{
	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_MAX_POLY; i++)
	{
		if (state->polyTile[i] == tile)
			return (int8_t)i;
	}

	return -1;
}

bool sampleLauncherStatePolyStopPending(const sampleLauncherState_t *state,
	uint16_t tile)
{
	const int8_t slot = sampleLauncherStateGetPolySlot(state, tile);
	return slot >= 0 && state->polyStopPending[slot];
}

bool sampleLauncherStatePolyStartPending(const sampleLauncherState_t *state,
	uint16_t tile)
{
	for (uint8_t i = 0; i < state->polyStartCount; i++)
	{
		if (state->polyStartQueue[i] == tile)
			return true;
	}

	return false;
}

bool sampleLauncherStateRequestQ(sampleLauncherState_t *state, uint16_t tile)
{
	if (tile >= SAMPLE_LAUNCHER_MAX_TILES)
		return false;

	if (state->qQueueCount > 0 &&
		state->qQueue[state->qQueueCount-1] == tile)
	{
		state->qQueue[--state->qQueueCount] = -1;
		return true;
	}

	if (state->qCurrent == tile && state->qQueueCount == 0)
	{
		state->qStopPending ^= 1;
		return true;
	}

	state->qStopPending = false;
	if (state->qQueueCount >= SAMPLE_LAUNCHER_QUEUE_MAX)
		return false;

	state->qQueue[state->qQueueCount++] = tile;
	return true;
}

static void removePendingPolyStart(sampleLauncherState_t *state, uint8_t index)
{
	for (uint8_t i = index + 1; i < state->polyStartCount; i++)
		state->polyStartQueue[i-1] = state->polyStartQueue[i];

	state->polyStartQueue[--state->polyStartCount] = -1;
}

static void clearQQueue(sampleLauncherState_t *state)
{
	state->qQueueCount = 0;
	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_QUEUE_MAX; i++)
		state->qQueue[i] = -1;
}

uint8_t sampleLauncherStateHardStop(sampleLauncherState_t *state, uint16_t tile,
	sampleLauncherAction_t actions[SAMPLE_LAUNCHER_MAX_ACTIONS])
{
	uint8_t actionCount = 0;
	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_MAX_ACTIONS; i++)
	{
		actions[i].type = SAMPLE_LAUNCHER_ACTION_NONE;
		actions[i].voice = -1;
		actions[i].tile = -1;
	}

	if (tile >= SAMPLE_LAUNCHER_MAX_TILES)
		return 0;

	if (state->qCurrent == tile)
	{
		appendAction(actions, &actionCount, SAMPLE_LAUNCHER_ACTION_STOP_Q,
			0, state->qCurrent);
		state->qCurrent = -1;
		state->qStopPending = false;
		clearQQueue(state);
	}
	else
	{
		uint8_t writeIndex = 0;
		for (uint8_t readIndex = 0; readIndex < state->qQueueCount;
			readIndex++)
		{
			if (state->qQueue[readIndex] != tile)
				state->qQueue[writeIndex++] = state->qQueue[readIndex];
		}
		for (uint8_t i = writeIndex; i < state->qQueueCount; i++)
			state->qQueue[i] = -1;
		state->qQueueCount = writeIndex;
	}

	const int8_t activeSlot = sampleLauncherStateGetPolySlot(state, tile);
	if (activeSlot >= 0)
	{
		appendAction(actions, &actionCount, SAMPLE_LAUNCHER_ACTION_STOP_POLY,
			(int8_t)(activeSlot + 1), state->polyTile[activeSlot]);
		state->polyTile[activeSlot] = -1;
		state->polyStopPending[activeSlot] = false;
	}

	for (uint8_t i = 0; i < state->polyStartCount;)
	{
		if (state->polyStartQueue[i] == tile)
			removePendingPolyStart(state, i);
		else
			i++;
	}

	return actionCount;
}

bool sampleLauncherStateTogglePoly(sampleLauncherState_t *state, uint16_t tile)
{
	if (tile >= SAMPLE_LAUNCHER_MAX_TILES)
		return false;

	const int8_t activeSlot = sampleLauncherStateGetPolySlot(state, tile);
	if (activeSlot >= 0)
	{
		state->polyStopPending[activeSlot] ^= 1;
		return true;
	}

	for (uint8_t i = 0; i < state->polyStartCount; i++)
	{
		if (state->polyStartQueue[i] == tile)
		{
			removePendingPolyStart(state, i);
			return true;
		}
	}

	uint8_t available = state->polyStartCount;
	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_MAX_POLY; i++)
	{
		if (state->polyTile[i] >= 0 && !state->polyStopPending[i])
			available++;
	}
	if (available >= SAMPLE_LAUNCHER_MAX_POLY)
		return false;

	state->polyStartQueue[state->polyStartCount++] = tile;
	return true;
}

uint8_t sampleLauncherStateCommitBoundary(sampleLauncherState_t *state,
	sampleLauncherAction_t actions[SAMPLE_LAUNCHER_MAX_ACTIONS])
{
	uint8_t actionCount = 0;
	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_MAX_ACTIONS; i++)
	{
		actions[i].type = SAMPLE_LAUNCHER_ACTION_NONE;
		actions[i].voice = -1;
		actions[i].tile = -1;
	}

	if (state->qStopPending)
	{
		if (state->qCurrent >= 0)
			appendAction(actions, &actionCount,
				SAMPLE_LAUNCHER_ACTION_STOP_Q, 0, state->qCurrent);
		state->qCurrent = -1;
		state->qStopPending = false;
		state->qQueueCount = 0;
		for (uint8_t i = 0; i < SAMPLE_LAUNCHER_QUEUE_MAX; i++)
			state->qQueue[i] = -1;
	}
	else if (state->qQueueCount > 0)
	{
		if (state->qCurrent >= 0)
			appendAction(actions, &actionCount,
				SAMPLE_LAUNCHER_ACTION_STOP_Q, 0, state->qCurrent);

		state->qCurrent = state->qQueue[0];
		for (uint8_t i = 1; i < state->qQueueCount; i++)
			state->qQueue[i-1] = state->qQueue[i];
		state->qQueue[--state->qQueueCount] = -1;
		appendAction(actions, &actionCount,
			SAMPLE_LAUNCHER_ACTION_START_Q, 0, state->qCurrent);
	}

	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_MAX_POLY; i++)
	{
		if (state->polyTile[i] >= 0 && state->polyStopPending[i])
		{
			appendAction(actions, &actionCount,
				SAMPLE_LAUNCHER_ACTION_STOP_POLY, (int8_t)(i + 1),
				state->polyTile[i]);
			state->polyTile[i] = -1;
			state->polyStopPending[i] = false;
		}
	}

	for (uint8_t pending = 0; pending < state->polyStartCount; pending++)
	{
		for (uint8_t slot = 0; slot < SAMPLE_LAUNCHER_MAX_POLY; slot++)
		{
			if (state->polyTile[slot] < 0)
			{
				state->polyTile[slot] = state->polyStartQueue[pending];
				appendAction(actions, &actionCount,
					SAMPLE_LAUNCHER_ACTION_START_POLY,
					(int8_t)(slot + 1), state->polyTile[slot]);
				break;
			}
		}
		state->polyStartQueue[pending] = -1;
	}
	state->polyStartCount = 0;

	return actionCount;
}
