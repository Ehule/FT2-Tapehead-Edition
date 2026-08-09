#ifdef HAS_MIDI

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "ft2_midi_map.h"
#include "ft2_midi_surface.h"
#include "rtmidi/rtmidi_c.h"

#define MIDI_SURFACE_MAX_PORTS 99

static volatile bool acceptInput, callbackBusy;
static bool inputPortOpen, outputPortOpen;
static RtMidiInPtr surfaceIn;
static RtMidiOutPtr surfaceOut;

static bool textEqualsIgnoreCase(const char *a, const char *b)
{
	while (*a != '\0' && *b != '\0')
	{
		if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++))
			return false;
	}

	return *a == '\0' && *b == '\0';
}

static bool textContainsIgnoreCase(const char *text, const char *needle)
{
	if (*needle == '\0')
		return false;

	for (; *text != '\0'; text++)
	{
		const char *a = text;
		const char *b = needle;
		while (*a != '\0' && *b != '\0' &&
			tolower((unsigned char)*a) == tolower((unsigned char)*b))
		{
			a++;
			b++;
		}

		if (*b == '\0')
			return true;
	}

	return false;
}

int32_t tapeheadMidiSurfaceChoosePort(const char *requestedName,
	const char *const *availableNames, uint32_t availableCount)
{
	if (requestedName == NULL || *requestedName == '\0' ||
		availableNames == NULL)
	{
		return -1;
	}

	/* An exact saved name always wins, even if another port happens to contain
	** the same text. Comparisons are case-insensitive across MIDI backends. */
	for (uint32_t i = 0; i < availableCount; i++)
	{
		if (availableNames[i] != NULL &&
			textEqualsIgnoreCase(requestedName, availableNames[i]))
		{
			return (int32_t)i;
		}
	}

	/* ALSA can decorate a stable hardware name with a changing client/port
	** suffix. Accept a unique partial name, but refuse ambiguous matches. */
	int32_t match = -1;
	for (uint32_t i = 0; i < availableCount; i++)
	{
		if (availableNames[i] == NULL ||
			!textContainsIgnoreCase(availableNames[i], requestedName))
		{
			continue;
		}

		if (match >= 0)
			return -1;

		match = (int32_t)i;
	}

	return match;
}

static char *getPortName(RtMidiPtr device, uint32_t port)
{
	int32_t length = 0;
	rtmidi_get_port_name(device, port, NULL, &length);
	if (!device->ok || length <= 0)
		return NULL;

	char *name = (char *)malloc((size_t)length + 1);
	if (name == NULL)
		return NULL;

	rtmidi_get_port_name(device, port, name, &length);
	if (!device->ok)
	{
		free(name);
		return NULL;
	}

	return name;
}

static int32_t findRequestedPort(RtMidiPtr device, const char *requestedName)
{
	const uint32_t portCount = rtmidi_get_port_count(device);
	const uint32_t count = portCount > MIDI_SURFACE_MAX_PORTS
		? MIDI_SURFACE_MAX_PORTS : portCount;
	if (!device->ok || count == 0)
		return -1;

	char *names[MIDI_SURFACE_MAX_PORTS] = { NULL };
	for (uint32_t i = 0; i < count; i++)
		names[i] = getPortName(device, i);

	const int32_t selected = tapeheadMidiSurfaceChoosePort(requestedName,
		(const char *const *)names, count);

	for (uint32_t i = 0; i < count; i++)
		free(names[i]);

	return selected;
}

static void surfaceInputCallback(double timeStamp,
	const unsigned char *message, size_t messageSize, void *userData)
{
	if (!acceptInput || message == NULL || messageSize < 2)
		return;

	callbackBusy = true;
	if (acceptInput)
	{
		const uint8_t status = message[0];
		if (status >= 0x80 && status < 0xF0)
		{
			const uint8_t data1 = message[1] & 0x7F;
			const uint8_t data2 = messageSize >= 3 ? message[2] & 0x7F : 0;
			(void)tapeheadMidiMapHandleMessage(status, data1, data2);
		}
	}
	callbackBusy = false;

	(void)timeStamp;
	(void)userData;
}

static void openInput(const char *requestedName)
{
	if (requestedName == NULL || *requestedName == '\0')
		return;

	surfaceIn = rtmidi_in_create_default();
	if (surfaceIn == NULL || !surfaceIn->ok)
		goto fail;

	const int32_t port = findRequestedPort(surfaceIn, requestedName);
	if (port < 0)
		goto fail;

	rtmidi_open_port(surfaceIn, (uint32_t)port,
		"FT2 Tapehead Control Surface Input");
	if (!surfaceIn->ok)
		goto fail;

	rtmidi_in_set_callback(surfaceIn, surfaceInputCallback, NULL);
	if (!surfaceIn->ok)
	{
		rtmidi_close_port(surfaceIn);
		goto fail;
	}

	/* SysEx output is needed later for APC setup, but incoming SysEx, clock and
	** active sensing do not represent Tapehead actions. */
	rtmidi_in_ignore_types(surfaceIn, true, true, true);
	if (!surfaceIn->ok)
	{
		rtmidi_in_cancel_callback(surfaceIn);
		rtmidi_close_port(surfaceIn);
		goto fail;
	}

	inputPortOpen = true;
	acceptInput = true;
	return;

fail:
	if (surfaceIn != NULL)
	{
		rtmidi_in_free(surfaceIn);
		surfaceIn = NULL;
	}
}

static void openOutput(const char *requestedName)
{
	if (requestedName == NULL || *requestedName == '\0')
		return;

	surfaceOut = rtmidi_out_create_default();
	if (surfaceOut == NULL || !surfaceOut->ok)
		goto fail;

	const int32_t port = findRequestedPort(surfaceOut, requestedName);
	if (port < 0)
		goto fail;

	rtmidi_open_port(surfaceOut, (uint32_t)port,
		"FT2 Tapehead Control Surface Output");
	if (!surfaceOut->ok)
		goto fail;

	outputPortOpen = true;
	return;

fail:
	if (surfaceOut != NULL)
	{
		rtmidi_out_free(surfaceOut);
		surfaceOut = NULL;
	}
}

bool tapeheadMidiSurfaceOpen(const char *inputDeviceName,
	const char *outputDeviceName)
{
	tapeheadMidiSurfaceClose();
	openInput(inputDeviceName);
	openOutput(outputDeviceName);
	return inputPortOpen || outputPortOpen;
}

void tapeheadMidiSurfaceClose(void)
{
	acceptInput = false;
	if (surfaceIn != NULL)
	{
		if (inputPortOpen)
		{
			rtmidi_in_cancel_callback(surfaceIn);
			while (callbackBusy)
				SDL_Delay(1);
			rtmidi_close_port(surfaceIn);
		}

		rtmidi_in_free(surfaceIn);
		surfaceIn = NULL;
	}

	if (surfaceOut != NULL)
	{
		if (outputPortOpen)
			rtmidi_close_port(surfaceOut);
		rtmidi_out_free(surfaceOut);
		surfaceOut = NULL;
	}

	inputPortOpen = false;
	outputPortOpen = false;
	callbackBusy = false;
}

bool tapeheadMidiSurfaceInputIsOpen(void)
{
	return inputPortOpen;
}

bool tapeheadMidiSurfaceOutputIsOpen(void)
{
	return outputPortOpen;
}

bool tapeheadMidiSurfaceSend(const uint8_t *message, size_t messageSize)
{
	if (!outputPortOpen || surfaceOut == NULL || message == NULL ||
		messageSize == 0 || messageSize > INT_MAX)
	{
		return false;
	}

	if (rtmidi_out_send_message(surfaceOut, message, (int)messageSize) != 0 ||
		!surfaceOut->ok)
	{
		outputPortOpen = false;
		return false;
	}

	return true;
}

#else
typedef int prevent_compiler_warning;
#endif
