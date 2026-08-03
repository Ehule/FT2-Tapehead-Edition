#pragma once

#include <stdbool.h>
#include <stdint.h>

#define TAPEHEAD_JACK_DEVICE_NAME "Tapehead JACK Virtual Outputs"

typedef void (*tapeheadJackRenderCallback)(float **outputs,
	uint32_t sampleFrames, uint8_t outputBusCount, void *userdata);

bool tapeheadJackLibraryAvailable(void);
bool tapeheadJackDeviceSelected(const char *deviceName);
const char *tapeheadJackGetLastError(void);

bool tapeheadJackOpen(uint8_t outputBusCount,
	tapeheadJackRenderCallback renderCallback, void *userdata,
	uint32_t *sampleRate, uint32_t *bufferSize);
bool tapeheadJackActivate(void);
void tapeheadJackClose(void);
void tapeheadJackPause(bool pause);
void tapeheadJackLock(void);
void tapeheadJackUnlock(void);
bool tapeheadJackIsOpen(void);

