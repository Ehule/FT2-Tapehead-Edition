// this implements MIDI input only!

#ifdef HAS_MIDI

// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif
#include "ft2_header.h"
#include "ft2_edit.h"
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_midi.h"
#include "ft2_audio.h"
#include "ft2_mouse.h"
#include "ft2_pattern_ed.h"
#include "ft2_structs.h"
#include "rtmidi/rtmidi_c.h"

// hide POSIX warnings
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

midi_t midi; // globalized

static volatile bool midiDeviceOpened;
static bool recMIDIValidChn = true;
static volatile RtMidiPtr midiInDev;
static volatile RtMidiOutPtr midiOutDev;
static bool midiOutPortOpened;
static bool midiOutVirtualAvailable;
static bool midiOutUsingVirtual;
static uint8_t midiDubActiveNote[MAX_CHANNELS];

/* MIDI_DUB_DAW_GATE: tracker notes are attacks, not indefinitely held keys.
** End each emitted MIDI note on the following replay tick so DAWs record
** repeated pitches as separate piano-roll notes instead of one drone. */
static uint8_t midiDubGateTicks[MAX_CHANNELS];


static bool initMidiOut(void)
{
	midiOutDev = rtmidi_out_create_default();
	if (midiOutDev == NULL || !midiOutDev->ok)
	{
		if (midiOutDev != NULL) rtmidi_out_free(midiOutDev);
		midiOutDev = NULL;
		return false;
	}

	/* Linux/macOS can expose a native virtual source. Windows normally
	** cannot, but the same RtMidi object can still enumerate and open
	** loopMIDI, hardware, and other destination ports. */
	rtmidi_open_virtual_port(midiOutDev, "FT2 Tapehead MIDI Dub");
	if (midiOutDev->ok)
	{
		midiOutPortOpened = true;
		midiOutVirtualAvailable = true;
		midiOutUsingVirtual = true;
		midi.outputDevice = 0;
		midi.outputDeviceName = strdup("FT2 Tapehead MIDI Dub (virtual)");
	}
	else
	{
		/* Recreate after a failed virtual-port attempt so output enumeration
		** remains usable on WinMM. */
		rtmidi_out_free(midiOutDev);
		midiOutDev = rtmidi_out_create_default();
		midiOutPortOpened = false;
		midiOutVirtualAvailable = false;
		midiOutUsingVirtual = false;
	}

	midi.dubEnable = true;
	memset(midiDubActiveNote, 0, sizeof (midiDubActiveNote));
	return midiOutDev != NULL && midiOutDev->ok;
}

void freeMidiOut(void)
{
	if (midiOutDev != NULL)
	{
		rtmidi_close_port(midiOutDev);
		rtmidi_out_free(midiOutDev);
		midiOutDev = NULL;
	}

	midiOutPortOpened = false;
	midiOutVirtualAvailable = false;
	midiOutUsingVirtual = false;
}

static void midiDubSend3Raw(uint8_t status, uint8_t data1, uint8_t data2)
{
	if (!midiOutPortOpened || midiOutDev == NULL)
		return;

	const unsigned char message[3] = { status, data1, data2 };
	rtmidi_out_send_message(midiOutDev, message, 3);
}

static void midiDubSend3(uint8_t status, uint8_t data1, uint8_t data2)
{
	if (midi.dubEnable)
		midiDubSend3Raw(status, data1, data2);
}

static inline uint8_t midiDubChannelForTrack(uint8_t channelIndex)
{
	return tapeheadConfig.midiDubTrackChannels[channelIndex];
}

void midiDubNoteOff(uint8_t channelIndex)
{
	if (!midi.dubEnable || channelIndex >= MAX_CHANNELS)
		return;

	const uint8_t activeNote = midiDubActiveNote[channelIndex];
	if (activeNote == 0)
		return;

	const uint8_t midiChannel = midiDubChannelForTrack(channelIndex);
	midiDubSend3(0x80 | midiChannel, activeNote, 0);
	midiDubActiveNote[channelIndex] = 0;
	midiDubGateTicks[channelIndex] = 0;
}

void midiDubNoteOn(uint8_t channelIndex, uint8_t note, uint8_t velocity)
{
	if (!midi.dubEnable || !midiOutPortOpened || channelIndex >= MAX_CHANNELS || note == 0 || note > 96)
		return;

	/* One monophonic MIDI voice per tracker track mirrors FT2's channel
	** behavior and prevents stacked notes from surviving retriggers. */
	midiDubNoteOff(channelIndex);

	const uint8_t midiChannel = midiDubChannelForTrack(channelIndex);
	const uint8_t midiNote = note + 11; // FT2 note 1 (C-0) -> MIDI note 12
	if (velocity == 0)
		velocity = 100;

	midiDubSend3(0x90 | midiChannel, midiNote, velocity & 0x7F);
	midiDubActiveNote[channelIndex] = midiNote;
	midiDubGateTicks[channelIndex] = 1;
}

void midiDubTick(void)
{
	if (!midi.dubEnable)
		return;

	for (uint8_t i = 0; i < MAX_CHANNELS; i++)
	{
		if (midiDubGateTicks[i] == 0)
			continue;

		if (--midiDubGateTicks[i] == 0)
			midiDubNoteOff(i);
	}
}

void midiDubPanic(void)
{
	for (uint8_t i = 0; i < MAX_CHANNELS; i++)
	{
		const uint8_t activeNote = midiDubActiveNote[i];
		if (activeNote != 0)
			midiDubSend3Raw(0x80 | midiDubChannelForTrack(i), activeNote, 0);

		midiDubActiveNote[i] = 0;
	}

	/* Also send CC123 on all channels in case the receiver retained a note. */
	for (uint8_t i = 0; i < 16; i++)
		midiDubSend3Raw(0xB0 | i, 123, 0);
}

static inline void midiInSetChannel(uint8_t status)
{
	recMIDIValidChn = (config.recMIDIAllChn || (status & 0xF) == config.recMIDIChn-1);
}

static inline void midiInKeyAction(int8_t m, uint8_t mv)
{
	int16_t vol = (mv * 64 * config.recMIDIVolSens) / (127 * 100);
	if (vol > 64)
		vol = 64;

	// FT2 bugfix: If velocity>0, and sensitivity made vol=0, set vol to 1 (prevent key off)
	if (mv > 0 && vol == 0)
		vol = 1;

	if (mv > 0 && !config.recMIDIVelocity)
		vol = -1; // don't record volume (velocity)

	m -= 11;
	if (config.recMIDITransp)
		m += (int8_t)config.recMIDITranspVal;

	if ((mv == 0 || vol != 0) && m > 0 && m < 96 && recMIDIValidChn)
		recordNote(m, (int8_t)vol);
}

static inline void midiInControlChange(uint8_t data1, uint8_t data2)
{
	if (data1 != 1) // 1 = modulation wheel
		return;

	midi.currMIDIVibDepth = data2 << 6;

	if (recMIDIValidChn) // real FT2 forgot to check this here..
	{
		for (uint8_t i = 0; i < song.numChannels; i++)
		{
			if (channel[i].midiVibDepth != 0 || editor.keyOnTab[i] != 0)
				channel[i].midiVibDepth = midi.currMIDIVibDepth;
		}
	}

	const uint8_t vibDepth = (midi.currMIDIVibDepth >> 9) & 0x0F;
	if (vibDepth > 0 && recMIDIValidChn)
		recordMIDIEffect(0x04, 0xA0 | vibDepth);
}

static inline void midiInPitchBendChange(uint8_t data1, uint8_t data2)
{
	int16_t pitch = (int16_t)((data2 << 7) | data1) - 8192; // -8192..8191
	pitch >>= 6; // -128..127

	midi.currMIDIPitch = pitch;
	if (recMIDIValidChn)
	{
		channel_t *ch = channel;
		for (uint8_t i = 0; i < song.numChannels; i++, ch++)
		{
			if (ch->midiPitch != 0 || editor.keyOnTab[i] != 0)
				ch->midiPitch = midi.currMIDIPitch;
		}
	}
}

static void midiInCallback(double timeStamp, const unsigned char *message, size_t messageSize, void *userData)
{
	uint8_t byte[3];

	if (!midi.enable || messageSize < 2)
		return;

	midi.callbackBusy = true;

	byte[0] = message[0];
	if (byte[0] > 127 && byte[0] < 240)
	{
		byte[1] = message[1] & 0x7F;

		if (messageSize >= 3)
			byte[2] = message[2] & 0x7F;
		else
			byte[2] = 0;

		midiInSetChannel(byte[0]);

		     if (byte[0] >= 128 && byte[0] <= 128+15)       midiInKeyAction(byte[1], 0);
		else if (byte[0] >= 144 && byte[0] <= 144+15)       midiInKeyAction(byte[1], byte[2]);
		else if (byte[0] >= 176 && byte[0] <= 176+15)   midiInControlChange(byte[1], byte[2]);
		else if (byte[0] >= 224 && byte[0] <= 224+15) midiInPitchBendChange(byte[1], byte[2]);
	}

	midi.callbackBusy = false;

	(void)timeStamp;
	(void)userData;
}

static uint32_t getNumMidiInDevices(void)
{
	if (midiInDev == NULL)
		return 0;

	return rtmidi_get_port_count(midiInDev);
}

static char *getMidiInDeviceName(uint32_t deviceID)
{
	if (midiInDev == NULL)
		return NULL; // MIDI not initialized

	// get string length
	int32_t reqStrLen = 0;
	rtmidi_get_port_name(midiInDev, deviceID, NULL, &reqStrLen);
	if (!midiInDev->ok)
		return NULL;

	// allocate memory
	char *devStr = (char *)malloc(reqStrLen+1);
	if (devStr == NULL)
		return NULL;

	rtmidi_get_port_name(midiInDev, deviceID, devStr, &reqStrLen);
	if (!midiInDev->ok)
		return NULL;

	return devStr;
}

static bool isMidiDubInputDevice(uint32_t deviceID)
{
	char *deviceName = getMidiInDeviceName(deviceID);
	if (deviceName == NULL)
		return false;

	const bool isMidiDub = strstr(deviceName, "FT2 Tapehead MIDI Dub") != NULL;
	free(deviceName);
	return isMidiDub;
}

void closeMidiInDevice(void)
{
	if (midiDeviceOpened)
	{
		if (midiInDev != NULL)
		{
			rtmidi_in_cancel_callback(midiInDev);
			rtmidi_close_port(midiInDev);
		}

		midiDeviceOpened = false;
	}
}

void freeMidiIn(void)
{
	if (midiInDev != NULL)
	{
		rtmidi_in_free(midiInDev);
		midiInDev = NULL;
	}
}

bool initMidiIn(void)
{
	midiInDev = rtmidi_in_create_default();
	if (midiInDev == NULL)
		return false;

	if (!midiInDev->ok)
	{
		rtmidi_in_free(midiInDev);
		midiInDev = NULL;
		return false;
	}

	return true;
}

bool openMidiInDevice(uint32_t deviceID)
{
	if (midiDeviceOpened || midiInDev == NULL || midi.numInputDevices == 0)
		return false;

	/* Never allow Tapehead's virtual MIDI output to feed its own input.
	** Such a connection recursively echoes every note until FT2 hangs. */
	if (isMidiDubInputDevice(deviceID))
		return false;

	rtmidi_open_port(midiInDev, deviceID, "FT2 Clone MIDI Port");
	if (!midiInDev->ok)
		return false;

	rtmidi_in_set_callback(midiInDev, midiInCallback, NULL);
	if (!midiInDev->ok)
	{
		rtmidi_close_port(midiInDev);
		return false;
	}

	rtmidi_in_ignore_types(midiInDev, true, true, true);

	midiDeviceOpened = true;
	return true;
}

void recordMIDIEffect(uint8_t efx, uint8_t efxData)
{
	// only handle this in record mode
	if (!midi.enable || (playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT))
		return;

	if (config.multiRec)
	{
		note_t *p = &pattern[editor.editPattern][editor.row * MAX_CHANNELS];
		for (int32_t i = 0; i < song.numChannels; i++, p++)
		{
			if (config.multiRecChn[i] && !editor.channelMuted[i])
			{
				if (!allocatePattern(editor.editPattern))
					return;

				if (p->efx == 0)
				{
					p->efx = efx;
					p->efxData = efxData;
					setSongModifiedFlag();
				}
			}
		}
	}
	else
	{
		if (!allocatePattern(editor.editPattern))
			return;

		note_t *p = &pattern[editor.editPattern][(editor.row * MAX_CHANNELS) + cursor.ch];
		if (p->efx != efx || p->efxData != efxData)
			setSongModifiedFlag();

		p->efx = efx;
		p->efxData = efxData;
	}
}

bool saveMidiInputDeviceToConfig(void)
{
	if (!midi.initThreadDone || midiInDev == NULL || !midiDeviceOpened)
		return false;

	const uint32_t numDevices = getNumMidiInDevices();
	if (numDevices == 0)
		return false;

	char *midiInStr = getMidiInDeviceName(midi.inputDevice);
	if (midiInStr == NULL)
		return false;

	FILE *f = UNICHAR_FOPEN(editor.midiConfigFileLocationU, "w");
	if (f == NULL)
	{
		free(midiInStr);
		return false;
	}

	fputs(midiInStr, f);
	free(midiInStr);

	fclose(f);
	return true;
}

bool setMidiInputDeviceFromConfig(void)
{
	uint32_t i;

	if (midiInDev == NULL || editor.midiConfigFileLocationU == NULL)
		goto setDefMidiInputDev;

	const uint32_t numDevices = getNumMidiInDevices();
	if (numDevices == 0)
		goto setDefMidiInputDev;

	FILE *f = UNICHAR_FOPEN(editor.midiConfigFileLocationU, "r");
	if (f == NULL)
		goto setDefMidiInputDev;

	char *devString = (char *)malloc(1024+2);
	if (devString == NULL)
	{
		fclose(f);
		goto setDefMidiInputDev;
	}
	devString[0] = '\0';

	if (fgets(devString, 1024, f) == NULL)
	{
		fclose(f);
		free(devString);
		goto setDefMidiInputDev;
	}

	fclose(f);

	// scan for device in list
	char *midiInStr = NULL;
	for (i = 0; i < numDevices; i++)
	{
		midiInStr = getMidiInDeviceName(i);
		if (midiInStr == NULL)
			continue;

		if (!_stricmp(devString, midiInStr))
			break; // device matched

		free(midiInStr);
		midiInStr = NULL;
	}

	free(devString);

	// device not found in list, set default
	if (i == numDevices)
		goto setDefMidiInputDev;

	if (midi.inputDeviceName != NULL)
	{
		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	midi.inputDevice = i;
	midi.inputDeviceName = midiInStr;
	midi.numInputDevices = numDevices;

	return true;

	// couldn't load device, set default
setDefMidiInputDev:
	if (midi.inputDeviceName != NULL)
	{
		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	midi.inputDevice = 0;
	midi.inputDeviceName = strdup("Error configuring MIDI...");
	midi.numInputDevices = 1;

	return false;
}


static uint32_t getNumMidiOutDevices(void)
{
	return midiOutDev == NULL ? 0 : rtmidi_get_port_count(midiOutDev);
}

static char *getMidiOutDeviceName(uint32_t deviceID)
{
	if (midiOutDev == NULL) return NULL;
	int32_t len = 0;
	rtmidi_get_port_name(midiOutDev, deviceID, NULL, &len);
	if (!midiOutDev->ok || len <= 0) return NULL;
	char *name = (char *)malloc(len + 1);
	if (name == NULL) return NULL;
	rtmidi_get_port_name(midiOutDev, deviceID, name, &len);
	if (!midiOutDev->ok) { free(name); return NULL; }
	return name;
}

void freeMidiOutputDeviceList(void)
{
	for (int32_t i = 0; i < MAX_MIDI_DEVICES; i++)
	{
		free(midi.outputDeviceNames[i]);
		midi.outputDeviceNames[i] = NULL;
	}
	midi.numOutputDevices = 0;
}

void rescanMidiOutputDevices(void)
{
	freeMidiOutputDeviceList();

	/* A virtual MIDI source is not returned by RtMidi's destination-port
	** enumeration, so expose it as an explicit synthetic first item. */
	if (midiOutVirtualAvailable && midi.numOutputDevices < MAX_MIDI_DEVICES)
		midi.outputDeviceNames[midi.numOutputDevices++] = strdup("FT2 Tapehead MIDI Dub (virtual)");

	const uint32_t room = MAX_MIDI_DEVICES - midi.numOutputDevices;
	const uint32_t count = MIN(getNumMidiOutDevices(), room);
	for (uint32_t i = 0; i < count; i++)
	{
		char *name = getMidiOutDeviceName(i);
		if (name != NULL) midi.outputDeviceNames[midi.numOutputDevices++] = name;
	}
	setScrollBarEnd(SB_MIDI_OUTPUT_SCROLL, midi.numOutputDevices);
	setScrollBarPos(SB_MIDI_OUTPUT_SCROLL, 0, DONT_TRIGGER_CALLBACK);
}

void drawMidiOutputList(void)
{
	clearRect(114, 105, 365, 62);
	if (!midi.initThreadDone || midiOutDev == NULL || midi.numOutputDevices == 0)
	{
		textOut(114, 105, PAL_FORGRND, "No MIDI output devices found.");
		textOut(114, 116, PAL_FORGRND, "On Windows, create a loopMIDI port first.");
		return;
	}
	for (uint16_t i = 0; i < 6; i++)
	{
		uint32_t n = getScrollBarPos(SB_MIDI_OUTPUT_SCROLL) + i;
		if (n >= midi.numOutputDevices || midi.outputDeviceNames[n] == NULL) continue;
		const uint16_t y = 105 + i * 11;
		if (midi.outputDeviceName != NULL && !_stricmp(midi.outputDeviceName, midi.outputDeviceNames[n]))
			fillRect(114, y, 365, 10, PAL_BOXSLCT);
		char *tmp = utf8ToCp850(midi.outputDeviceNames[n], true);
		if (tmp != NULL) { textOutClipX(114, y, PAL_FORGRND, tmp, 479); free(tmp); }
	}
}

void scrollMidiOutputDevListUp(void) { scrollBarScrollUp(SB_MIDI_OUTPUT_SCROLL, 1); }
void scrollMidiOutputDevListDown(void) { scrollBarScrollDown(SB_MIDI_OUTPUT_SCROLL, 1); }
void sbMidiOutputSetPos(uint32_t pos)
{
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_MIDI_INPUT) drawMidiOutputList();
	(void)pos;
}

bool testMidiOutputDeviceListMouseDown(void)
{
	if (!ui.configScreenShown || editor.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT) return false;
	if (mouse.x < 114 || mouse.x > 479 || mouse.y < 105 || mouse.y > 166) return false;
	if (!midi.initThreadDone || midiOutDev == NULL) return true;
	uint32_t n = getScrollBarPos(SB_MIDI_OUTPUT_SCROLL) + ((mouse.y - 105) / 11);
	if (n >= midi.numOutputDevices || midi.outputDeviceNames[n] == NULL) return true;

	midiDubPanic();
	if (midiOutPortOpened) rtmidi_close_port(midiOutDev);
	midiOutPortOpened = false;
	midiOutUsingVirtual = false;

	const bool selectVirtual = midiOutVirtualAvailable && n == 0;
	if (selectVirtual)
		rtmidi_open_virtual_port(midiOutDev, "FT2 Tapehead MIDI Dub");
	else
	{
		const uint32_t realPort = n - (midiOutVirtualAvailable ? 1 : 0);
		rtmidi_open_port(midiOutDev, realPort, "FT2 Tapehead MIDI Dub");
	}

	if (midiOutDev->ok)
	{
		midiOutPortOpened = true;
		midiOutUsingVirtual = selectVirtual;
		free(midi.outputDeviceName);
		midi.outputDeviceName = strdup(midi.outputDeviceNames[n]);
		midi.outputDevice = n;
	}
	drawMidiOutputList();
	return true;
}

void freeMidiInputDeviceList(void)
{
	for (int32_t i = 0; i < MAX_MIDI_DEVICES; i++)
	{
		if (midi.inputDeviceNames[i] != NULL)
		{
			free(midi.inputDeviceNames[i]);
			midi.inputDeviceNames[i] = NULL;
		}
	}

	midi.numInputDevices = 0;
}

void rescanMidiInputDevices(void)
{
	freeMidiInputDeviceList();

	const uint32_t numPorts = MIN(getNumMidiInDevices(), MAX_MIDI_DEVICES);
	midi.numInputDevices = 0;

	for (uint32_t port = 0; port < numPorts; port++)
	{
		char *deviceName = getMidiInDeviceName(port);
		if (deviceName == NULL)
			continue;

		/* Never expose our own virtual output as an FT2 input. Besides being
		** confusing, connecting it creates an exponentially growing MIDI loop. */
		if (strstr(deviceName, "FT2 Tapehead MIDI Dub") != NULL)
		{
			free(deviceName);
			continue;
		}

		midi.inputDeviceNames[midi.numInputDevices++] = deviceName;
	}

	setScrollBarEnd(SB_MIDI_INPUT_SCROLL, midi.numInputDevices);
	setScrollBarPos(SB_MIDI_INPUT_SCROLL, 0, DONT_TRIGGER_CALLBACK);
}

void drawMidiInputList(void)
{
	clearRect(114, 17, 365, 66);

	if (!midi.initThreadDone || midiInDev == NULL || midi.numInputDevices == 0)
	{
		textOut(114, 17 + (0 * 11), PAL_FORGRND, "No MIDI input devices found!");
		textOut(114, 17 + (1 * 11), PAL_FORGRND, "Either wait a few seconds for MIDI to initialize, or restart the");
		textOut(114, 17 + (2 * 11), PAL_FORGRND, "tracker if you recently plugged in a MIDI device.");
		return;
	}

	for (uint16_t i = 0; i < 6; i++)
	{
		uint32_t deviceEntry = getScrollBarPos(SB_MIDI_INPUT_SCROLL) + i;
		if (deviceEntry > MAX_MIDI_DEVICES)
			deviceEntry = MAX_MIDI_DEVICES;

		if (deviceEntry < midi.numInputDevices)
		{
			if (midi.inputDeviceNames[deviceEntry] == NULL)
				continue;

			const uint16_t y = 17 + (i * 11);

			if (midi.inputDeviceName != NULL)
			{
				if (_stricmp(midi.inputDeviceName, midi.inputDeviceNames[deviceEntry]) == 0)
					fillRect(114, y, 365, 10, PAL_BOXSLCT); // selection background color
			}

			char *tmpString = utf8ToCp850(midi.inputDeviceNames[deviceEntry], true);
			if (tmpString != NULL)
			{
				textOutClipX(114, y, PAL_FORGRND, tmpString, 479);
				free(tmpString);
			}
		}
	}
}

void scrollMidiInputDevListUp(void)
{
	scrollBarScrollUp(SB_MIDI_INPUT_SCROLL, 1);
}

void scrollMidiInputDevListDown(void)
{
	scrollBarScrollDown(SB_MIDI_INPUT_SCROLL, 1);
}

void sbMidiInputSetPos(uint32_t pos)
{
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_MIDI_INPUT)
		drawMidiInputList();

	(void)pos;
}

bool testMidiInputDeviceListMouseDown(void)
{
	if (!ui.configScreenShown || editor.currConfigScreen != CONFIG_SCREEN_MIDI_INPUT)
		return false;

	if (mouse.x < 114 || mouse.x > 479 || mouse.y < 17 || mouse.y > 82)
		return false; // we didn't click inside the list area

	if (!midi.initThreadDone)
		return true;

	uint32_t deviceNum = (uint32_t)scrollBars[SB_MIDI_INPUT_SCROLL].pos + ((mouse.y - 17) / 11);
	if (deviceNum > MAX_MIDI_DEVICES)
		deviceNum = MAX_MIDI_DEVICES;

	if (midi.numInputDevices == 0 || deviceNum >= midi.numInputDevices)
		return true;

	/* Keep the MIDI Dub source visible for diagnosis, but never let FT2
	** select it as its own input. */
	if (midi.inputDeviceNames[deviceNum] != NULL &&
	    strstr(midi.inputDeviceNames[deviceNum], "FT2 Tapehead MIDI Dub") != NULL)
		return true;

	if (midi.inputDeviceName != NULL)
	{
		if (!_stricmp(midi.inputDeviceName, midi.inputDeviceNames[deviceNum]))
			return true; // we clicked the currently selected device, do nothing

		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	midi.inputDeviceName = strdup(midi.inputDeviceNames[deviceNum]);
	midi.inputDevice = deviceNum;

	closeMidiInDevice();
	freeMidiIn();
	initMidiIn();
	openMidiInDevice(midi.inputDevice);

	drawMidiInputList();
	return true;
}

int32_t initMidiFunc(void *ptr)
{
	initMidiIn();
	initMidiOut();
	rescanMidiOutputDevices();
	setMidiInputDeviceFromConfig();
	openMidiInDevice(midi.inputDevice);
	midi.rescanDevicesFlag = true;
	midi.initThreadDone = true;

	return true;
	(void)ptr;
}

#else
typedef int prevent_compiler_warning; // kludge: prevent warning about empty .c file if HAS_MIDI is not defined
#endif
