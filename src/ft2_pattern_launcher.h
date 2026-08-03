#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PATTERN_LAUNCHER_QUEUE_MAX 4

typedef enum patternLauncherExitMode_t
{
	PATTERN_LAUNCHER_EXIT_NONE = 0,
	PATTERN_LAUNCHER_EXIT_RETURN,
	PATTERN_LAUNCHER_EXIT_STOP,
	PATTERN_LAUNCHER_EXIT_CONTINUE
} patternLauncherExitMode_t;

typedef enum patternLauncherBoundaryResult_t
{
	PATTERN_LAUNCHER_BOUNDARY_INACTIVE = 0,
	PATTERN_LAUNCHER_BOUNDARY_HANDLED,
	PATTERN_LAUNCHER_BOUNDARY_STOPPED
} patternLauncherBoundaryResult_t;

bool patternLauncherIsEnabled(void);
int16_t patternLauncherGetCurrent(void);
uint8_t patternLauncherGetQueueCount(void);
int16_t patternLauncherGetQueueItem(uint8_t index);
bool patternLauncherStopIsPending(void);
bool patternLauncherPolyHandoffIsPending(uint8_t patternNum);
uint8_t patternLauncherGetExitMode(void);
bool patternLauncherHasRouting(void);
bool patternLauncherOwnsDestination(int32_t destinationChannel);
int32_t patternLauncherGetSourceForDestination(int32_t destinationChannel);
bool patternLauncherConsumeDestinationRelease(int32_t destinationChannel);
void patternLauncherSetEnabled(bool enabled);
void patternLauncherRequest(uint8_t patternNum, bool ctrlPressed, bool shiftPressed);
bool patternLauncherRequestPolyHandoff(uint8_t patternNum);
patternLauncherBoundaryResult_t patternLauncherHandleBoundary(void);
void handlePatternLauncherStop(void);
void handlePolyMatrixQHandoff(void);
