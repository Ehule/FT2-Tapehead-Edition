#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ft2_replayer.h"
#include "ft2_sample_launcher_state.h"

uint8_t sampleLauncherGetTileCount(void);
const char *sampleLauncherGetTileName(uint8_t tile);
uint8_t sampleLauncherGetTileBus(uint8_t tile);
void sampleLauncherCycleTileBus(uint8_t tile, uint8_t busCount);
int16_t sampleLauncherGetQCurrent(void);
bool sampleLauncherQStopPending(void);
int8_t sampleLauncherGetQQueuePos(uint8_t tile);
int8_t sampleLauncherGetPolySlot(uint8_t tile);
bool sampleLauncherPolyStopPending(uint8_t tile);
bool sampleLauncherPolyStartPending(uint8_t tile);
bool sampleLauncherRequestQ(uint8_t tile);
bool sampleLauncherTogglePoly(uint8_t tile);
void sampleLauncherHandleBoundary(void);
void sampleLauncherAdoptDecodedFolder(sample_t *samples, uint32_t count);
void sampleLauncherReset(void);
void sampleLauncherFree(void);
