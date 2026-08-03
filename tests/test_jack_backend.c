#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/ft2_jack.h"

typedef uint8_t (*fakePortCountFunc)(void);
typedef const char *(*fakePortNameFunc)(uint8_t);
typedef float (*fakeSampleFunc)(uint8_t, uint32_t);
typedef uint32_t (*fakeCounterFunc)(void);
typedef int (*fakeRunProcessFunc)(void);

static uint32_t renderedFrames;
static uint8_t renderedBuses;

static void renderTestAudio(float **outputs, uint32_t sampleFrames,
	uint8_t outputBusCount, void *userdata)
{
	(void)userdata;
	renderedFrames = sampleFrames;
	renderedBuses = outputBusCount;

	for (uint8_t channel = 0; channel < outputBusCount * 2; channel++)
	{
		for (uint32_t sample = 0; sample < sampleFrames; sample++)
			outputs[channel][sample] = (float)(channel + 1) / 10.0f;
	}
}

int main(void)
{
	assert(tapeheadJackDeviceSelected(TAPEHEAD_JACK_DEVICE_NAME));
	assert(!tapeheadJackDeviceSelected("(Default Audio Device)"));
	assert(tapeheadJackLibraryAvailable());

	uint32_t sampleRate = 0;
	uint32_t bufferSize = 0;
	assert(tapeheadJackOpen(2, renderTestAudio, NULL, &sampleRate, &bufferSize));
	assert(tapeheadJackIsOpen());
	assert(sampleRate == 48000);
	assert(bufferSize == 64);

	void *fakeLibrary = dlopen("libjack.so.0", RTLD_NOW | RTLD_LOCAL);
	assert(fakeLibrary != NULL);
	fakePortCountFunc portCount = (fakePortCountFunc)dlsym(fakeLibrary,
		"fake_jack_port_count");
	fakePortNameFunc portName = (fakePortNameFunc)dlsym(fakeLibrary,
		"fake_jack_port_name");
	fakeSampleFunc sample = (fakeSampleFunc)dlsym(fakeLibrary,
		"fake_jack_sample");
	fakeCounterFunc activateCount = (fakeCounterFunc)dlsym(fakeLibrary,
		"fake_jack_activate_count");
	fakeCounterFunc deactivateCount = (fakeCounterFunc)dlsym(fakeLibrary,
		"fake_jack_deactivate_count");
	fakeRunProcessFunc runProcess = (fakeRunProcessFunc)dlsym(fakeLibrary,
		"fake_jack_run_process");
	assert(portCount != NULL && portName != NULL && sample != NULL);
	assert(activateCount != NULL && deactivateCount != NULL && runProcess != NULL);

	/*
	** Reproduce FT2's real device-selection lifecycle: setupAudio() opens the
	** client first, allocates the mixer buffers, and resumeAudio() then calls
	** tapeheadJackPause(false). Pass 6's test activated the fake client directly
	** and therefore missed that the production client was never activated.
	*/
	assert(activateCount() == 0);
	tapeheadJackPause(false);
	assert(activateCount() == 1);
	assert(renderedFrames == 64);
	assert(renderedBuses == 2);

	assert(portCount() == 4);
	assert(strcmp(portName(0), "bus_A_L") == 0);
	assert(strcmp(portName(1), "bus_A_R") == 0);
	assert(strcmp(portName(2), "bus_B_L") == 0);
	assert(strcmp(portName(3), "bus_B_R") == 0);
	assert(sample(0, 0) == 0.1f);
	assert(sample(1, 0) == 0.2f);
	assert(sample(2, 0) == 0.3f);
	assert(sample(3, 0) == 0.4f);
	assert(activateCount() == 1);
	assert(deactivateCount() == 0);

	/* Pausing must preserve the active client/ports and publish silence. */
	tapeheadJackPause(true);
	assert(activateCount() == 1);
	assert(deactivateCount() == 0);
	assert(runProcess() == 0);
	assert(sample(0, 0) == 0.0f);
	assert(sample(1, 0) == 0.0f);
	assert(sample(2, 0) == 0.0f);
	assert(sample(3, 0) == 0.0f);

	tapeheadJackPause(false);
	assert(activateCount() == 1);
	assert(deactivateCount() == 0);
	assert(runProcess() == 0);
	assert(sample(0, 0) == 0.1f);
	assert(sample(1, 0) == 0.2f);
	assert(sample(2, 0) == 0.3f);
	assert(sample(3, 0) == 0.4f);

	tapeheadJackLock();
	tapeheadJackUnlock();
	tapeheadJackClose();
	assert(!tapeheadJackIsOpen());
	assert(deactivateCount() == 1);
	dlclose(fakeLibrary);

	printf("Native JACK backend tests passed.\n");
	return 0;
}
