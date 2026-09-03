#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ft2_audio.h"

typedef struct openTestContext_t
{
	const char *expectedDevice;
	uint8_t successfulChannels;
	uint8_t attemptedChannels[4];
	uint8_t attemptCount;
	bool wrongDevice;
} openTestContext_t;

static bool fakeOpen(const char *device, uint8_t channels, void *context)
{
	openTestContext_t *test = (openTestContext_t *)context;
	if (test->attemptCount < sizeof (test->attemptedChannels))
		test->attemptedChannels[test->attemptCount] = channels;
	test->attemptCount++;

	if ((device == NULL) != (test->expectedDevice == NULL) ||
		(device != NULL && strcmp(device, test->expectedDevice) != 0))
	{
		test->wrongDevice = true;
	}

	return channels == test->successfulChannels;
}

static bool testExplicitDeviceNeverFallsBackToDefault(void)
{
	bool stereoFallback;
	openTestContext_t test = { "CABLE Input", 0, { 0 }, 0, false };
	const bool opened = tapeheadTestTryOpenSelectedOutput("CABLE Input", 8,
		fakeOpen, &test, &stereoFallback);
	return !opened && !stereoFallback && !test.wrongDevice &&
		test.attemptCount == 2 && test.attemptedChannels[0] == 8 &&
		test.attemptedChannels[1] == 2;
}

static bool testStereoFoldKeepsSelectedDevice(void)
{
	bool stereoFallback;
	openTestContext_t test = { "Studio Interface", 2, { 0 }, 0, false };
	const bool opened = tapeheadTestTryOpenSelectedOutput("Studio Interface", 6,
		fakeOpen, &test, &stereoFallback);
	return opened && stereoFallback && !test.wrongDevice &&
		test.attemptCount == 2 && test.attemptedChannels[0] == 6 &&
		test.attemptedChannels[1] == 2;
}

static bool testDefaultDeviceRemainsDefault(void)
{
	bool stereoFallback;
	openTestContext_t test = { NULL, 2, { 0 }, 0, false };
	const bool opened = tapeheadTestTryOpenSelectedOutput(NULL, 2, fakeOpen,
		&test, &stereoFallback);
	return opened && !stereoFallback && !test.wrongDevice &&
		test.attemptCount == 1 && test.attemptedChannels[0] == 2;
}

static bool testDeviceEventLifecycle(void)
{
	const uint8_t removed = tapeheadTestClassifyAudioDeviceEvent(
		SDL_AUDIODEVICEREMOVED, false, 41, 41, false);
	const uint8_t unrelated = tapeheadTestClassifyAudioDeviceEvent(
		SDL_AUDIODEVICEREMOVED, false, 42, 41, false);
	const uint8_t capture = tapeheadTestClassifyAudioDeviceEvent(
		SDL_AUDIODEVICEREMOVED, true, 41, 41, false);
	const uint8_t retry = tapeheadTestClassifyAudioDeviceEvent(
		SDL_AUDIODEVICEADDED, false, 0, 0, true);
	const uint8_t healthy = tapeheadTestClassifyAudioDeviceEvent(
		SDL_AUDIODEVICEADDED, false, 0, 41, false);

	return removed == (TAPEHEAD_AUDIO_EVENT_RESCAN |
			TAPEHEAD_AUDIO_EVENT_ACTIVE_OUTPUT_REMOVED) &&
		unrelated == TAPEHEAD_AUDIO_EVENT_RESCAN &&
		capture == TAPEHEAD_AUDIO_EVENT_RESCAN &&
		retry == (TAPEHEAD_AUDIO_EVENT_RESCAN |
			TAPEHEAD_AUDIO_EVENT_RETRY_OUTPUT) &&
		healthy == TAPEHEAD_AUDIO_EVENT_RESCAN;
}

int main(void)
{
	if (!testExplicitDeviceNeverFallsBackToDefault())
	{
		fprintf(stderr, "Explicit output silently fell back or retried incorrectly\n");
		return 1;
	}
	if (!testStereoFoldKeepsSelectedDevice())
	{
		fprintf(stderr, "Stereo folding changed the selected output device\n");
		return 1;
	}
	if (!testDefaultDeviceRemainsDefault())
	{
		fprintf(stderr, "Default-device open policy changed unexpectedly\n");
		return 1;
	}
	if (!testDeviceEventLifecycle())
	{
		fprintf(stderr, "Audio device lifecycle event classification failed\n");
		return 1;
	}

	return 0;
}
