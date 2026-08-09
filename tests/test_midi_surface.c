#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "ft2_midi_surface.h"
#include "rtmidi/rtmidi_c.h"

static struct RtMidiWrapper inputWrapper, outputWrapper;
static int inputToken, outputToken;
static const char *const inputPorts[] =
{
	"Studio Keyboard 20:0",
	"APC40 mkII 24:0"
};
static const char *const outputPorts[] =
{
	"External Synth 21:0",
	"APC40 mkII 24:1"
};

static RtMidiCCallback capturedCallback;
static void *capturedUserData;
static uint32_t openedInputPort, openedOutputPort;
static bool inputCallbackCancelled;
static uint8_t sentMessage[32];
static int sentMessageLength;
static uint32_t mappedMessageCount;
static uint8_t mappedStatus, mappedData1, mappedData2;

static bool isInput(RtMidiPtr device)
{
	return device != NULL && device->ptr == &inputToken;
}

static const char *const *portNames(RtMidiPtr device, uint32_t *count)
{
	if (isInput(device))
	{
		*count = sizeof (inputPorts) / sizeof (inputPorts[0]);
		return inputPorts;
	}

	*count = sizeof (outputPorts) / sizeof (outputPorts[0]);
	return outputPorts;
}

RtMidiInPtr rtmidi_in_create_default(void)
{
	memset(&inputWrapper, 0, sizeof (inputWrapper));
	inputWrapper.ptr = &inputToken;
	inputWrapper.ok = true;
	return &inputWrapper;
}

RtMidiOutPtr rtmidi_out_create_default(void)
{
	memset(&outputWrapper, 0, sizeof (outputWrapper));
	outputWrapper.ptr = &outputToken;
	outputWrapper.ok = true;
	return &outputWrapper;
}

void rtmidi_in_free(RtMidiInPtr device)
{
	(void)device;
}

void rtmidi_out_free(RtMidiOutPtr device)
{
	(void)device;
}

unsigned int rtmidi_get_port_count(RtMidiPtr device)
{
	uint32_t count;
	(void)portNames(device, &count);
	return count;
}

int rtmidi_get_port_name(RtMidiPtr device, unsigned int portNumber,
	char *buffer, int *bufferLength)
{
	uint32_t count;
	const char *const *names = portNames(device, &count);
	if (portNumber >= count || bufferLength == NULL)
	{
		device->ok = false;
		return -1;
	}

	const int required = (int)strlen(names[portNumber]) + 1;
	if (buffer == NULL)
	{
		*bufferLength = required;
		return 0;
	}

	if (*bufferLength < required)
	{
		device->ok = false;
		return -1;
	}

	memcpy(buffer, names[portNumber], (size_t)required);
	*bufferLength = required;
	return 0;
}

void rtmidi_open_port(RtMidiPtr device, unsigned int portNumber,
	const char *portName)
{
	uint32_t count;
	(void)portNames(device, &count);
	device->ok = portNumber < count;
	if (isInput(device))
		openedInputPort = portNumber;
	else
		openedOutputPort = portNumber;
	(void)portName;
}

void rtmidi_close_port(RtMidiPtr device)
{
	(void)device;
}

void rtmidi_in_set_callback(RtMidiInPtr device, RtMidiCCallback callback,
	void *userData)
{
	capturedCallback = callback;
	capturedUserData = userData;
	device->ok = true;
}

void rtmidi_in_cancel_callback(RtMidiInPtr device)
{
	inputCallbackCancelled = true;
	device->ok = true;
}

void rtmidi_in_ignore_types(RtMidiInPtr device, bool midiSysex,
	bool midiTime, bool midiSense)
{
	assert(midiSysex && midiTime && midiSense);
	device->ok = true;
}

int rtmidi_out_send_message(RtMidiOutPtr device,
	const unsigned char *message, int length)
{
	assert(length >= 0 && (size_t)length <= sizeof (sentMessage));
	memcpy(sentMessage, message, (size_t)length);
	sentMessageLength = length;
	device->ok = true;
	return 0;
}

void SDLCALL SDL_Delay(Uint32 milliseconds)
{
	(void)milliseconds;
}

bool tapeheadMidiMapHandleMessage(uint8_t status, uint8_t data1,
	uint8_t data2)
{
	mappedMessageCount++;
	mappedStatus = status;
	mappedData1 = data1;
	mappedData2 = data2;
	return true;
}

static void resetFixture(void)
{
	capturedCallback = NULL;
	capturedUserData = NULL;
	openedInputPort = UINT32_MAX;
	openedOutputPort = UINT32_MAX;
	inputCallbackCancelled = false;
	sentMessageLength = 0;
	mappedMessageCount = 0;
}

static void testNameSelection(void)
{
	const char *names[] =
	{
		"APC40 mkII secondary 31:0",
		"APC40 mkII",
		"Studio Keyboard"
	};
	assert(tapeheadMidiSurfaceChoosePort("apc40 MKII", names, 3) == 1);
	assert(tapeheadMidiSurfaceChoosePort("Keyboard", names, 3) == 2);
	assert(tapeheadMidiSurfaceChoosePort("secondary", names, 3) == 0);
	assert(tapeheadMidiSurfaceChoosePort("APC40", names, 3) == -1);
	assert(tapeheadMidiSurfaceChoosePort("Missing", names, 3) == -1);
	assert(tapeheadMidiSurfaceChoosePort("", names, 3) == -1);
}

static void testIndependentPortsAndControllerOnlyCallback(void)
{
	resetFixture();
	assert(tapeheadMidiSurfaceOpen("APC40 mkII 24:0", "APC40 mkII"));
	assert(tapeheadMidiSurfaceInputIsOpen());
	assert(tapeheadMidiSurfaceOutputIsOpen());
	assert(openedInputPort == 1);
	assert(openedOutputPort == 1);
	assert(capturedCallback != NULL);

	const unsigned char note[] = { 0x90, 48, 100 };
	capturedCallback(0.0, note, sizeof (note), capturedUserData);
	assert(mappedMessageCount == 1);
	assert(mappedStatus == 0x90 && mappedData1 == 48 && mappedData2 == 100);

	/* System messages do not reach the performance map. This dedicated module
	** has no dependency on FT2's recordNote()/musical-input path. */
	const unsigned char systemMessage[] = { 0xF8, 0 };
	capturedCallback(0.0, systemMessage, sizeof (systemMessage), capturedUserData);
	assert(mappedMessageCount == 1);

	const uint8_t feedback[] = { 0x90, 48, 5 };
	assert(tapeheadMidiSurfaceSend(feedback, sizeof (feedback)));
	assert(sentMessageLength == 3);
	assert(!memcmp(sentMessage, feedback, sizeof (feedback)));

	tapeheadMidiSurfaceClose();
	assert(inputCallbackCancelled);
	assert(!tapeheadMidiSurfaceInputIsOpen());
	assert(!tapeheadMidiSurfaceOutputIsOpen());
}

static void testMissingDirectionDoesNotDisableOther(void)
{
	resetFixture();
	assert(tapeheadMidiSurfaceOpen("Not connected", "APC40 mkII"));
	assert(!tapeheadMidiSurfaceInputIsOpen());
	assert(tapeheadMidiSurfaceOutputIsOpen());
	tapeheadMidiSurfaceClose();

	assert(tapeheadMidiSurfaceOpen("APC40 mkII", "Not connected"));
	assert(tapeheadMidiSurfaceInputIsOpen());
	assert(!tapeheadMidiSurfaceOutputIsOpen());
	tapeheadMidiSurfaceClose();

	assert(!tapeheadMidiSurfaceOpen("Not connected", "Also missing"));
	assert(!tapeheadMidiSurfaceInputIsOpen());
	assert(!tapeheadMidiSurfaceOutputIsOpen());
	tapeheadMidiSurfaceClose();
}

int main(void)
{
	testNameSelection();
	testIndependentPortsAndControllerOnlyCallback();
	testMissingDirectionDoesNotDisableOther();
	puts("Tapehead dual MIDI-device surface tests passed.");
	return 0;
}
