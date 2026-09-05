#pragma once

#include <stdbool.h>
#include <stdint.h>

#define TAPEHEAD_LIVE_LINK_DEVICE_NAME "TapeSister Live Link"

typedef void (*tapeheadLiveLinkRenderCallback)(float *interleaved,
    uint32_t sampleFrames, void *userdata);

bool tapeheadLiveLinkDeviceSelected(const char *deviceName);
const char *tapeheadLiveLinkGetLastError(void);
bool tapeheadLiveLinkOpen(uint32_t sampleRate, uint32_t bufferFrames,
    tapeheadLiveLinkRenderCallback renderCallback, void *userdata);
void tapeheadLiveLinkClose(void);
void tapeheadLiveLinkPause(bool pause);
void tapeheadLiveLinkLock(void);
void tapeheadLiveLinkUnlock(void);
bool tapeheadLiveLinkIsOpen(void);
