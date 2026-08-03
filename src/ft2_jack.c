#include <stdio.h>
#include <string.h>
#include "ft2_jack.h"

#if defined(__linux__)

#include <dlfcn.h>
#include <SDL2/SDL.h>

/*
** JACK is loaded dynamically so ordinary Tapehead builds retain no mandatory
** JACK development-package or runtime dependency. These declarations mirror
** the small, stable subset of the JACK ABI used by this backend.
*/
typedef uint32_t jack_nframes_t;
typedef uint32_t jack_options_t;
typedef uint32_t jack_status_t;
typedef struct _jack_client jack_client_t;
typedef struct _jack_port jack_port_t;

enum
{
	JackNullOption = 0,
	JackNoStartServer = 1,
	JackPortIsOutput = 2
};

#define JACK_DEFAULT_AUDIO_TYPE "32 bit float mono audio"

typedef jack_client_t *(*jackClientOpenFunc)(const char *, jack_options_t,
	jack_status_t *, ...);
typedef int (*jackClientCloseFunc)(jack_client_t *);
typedef int (*jackActivateFunc)(jack_client_t *);
typedef int (*jackDeactivateFunc)(jack_client_t *);
typedef int (*jackSetProcessCallbackFunc)(jack_client_t *,
	int (*)(jack_nframes_t, void *), void *);
typedef jack_port_t *(*jackPortRegisterFunc)(jack_client_t *, const char *,
	const char *, unsigned long, unsigned long);
typedef void *(*jackPortGetBufferFunc)(jack_port_t *, jack_nframes_t);
typedef jack_nframes_t (*jackGetSampleRateFunc)(jack_client_t *);
typedef jack_nframes_t (*jackGetBufferSizeFunc)(jack_client_t *);

typedef struct jackApi_t
{
	void *library;
	jackClientOpenFunc clientOpen;
	jackClientCloseFunc clientClose;
	jackActivateFunc activate;
	jackDeactivateFunc deactivate;
	jackSetProcessCallbackFunc setProcessCallback;
	jackPortRegisterFunc portRegister;
	jackPortGetBufferFunc portGetBuffer;
	jackGetSampleRateFunc getSampleRate;
	jackGetBufferSizeFunc getBufferSize;
} jackApi_t;

static jackApi_t jackApi;
static jack_client_t *jackClient;
static jack_port_t *jackPorts[32];
static SDL_mutex *jackMutex;
static tapeheadJackRenderCallback jackRenderCallback;
static void *jackRenderUserdata;
static uint8_t jackOutputBusCount;
static bool jackClientActive;
static SDL_atomic_t jackPauseRequested;
static char jackLastError[256];

static void setJackError(const char *message)
{
	if (message == NULL)
		message = "Unknown JACK error.";

	snprintf(jackLastError, sizeof (jackLastError), "%s", message);
}

static bool loadJackSymbol(void **destination, const char *name)
{
	*destination = dlsym(jackApi.library, name);
	if (*destination != NULL)
		return true;

	snprintf(jackLastError, sizeof (jackLastError),
		"JACK library is missing the required symbol '%s'.", name);
	return false;
}

static bool loadJackLibrary(void)
{
	if (jackApi.library != NULL)
		return true;

	jackApi.library = dlopen("libjack.so.0", RTLD_NOW | RTLD_LOCAL);
	if (jackApi.library == NULL)
	{
		setJackError("libjack.so.0 is not installed. Install PipeWire-JACK or JACK2.");
		return false;
	}

#define LOAD_JACK_SYMBOL(field, symbol) \
	if (!loadJackSymbol((void **)&jackApi.field, symbol)) goto loadFailure

	LOAD_JACK_SYMBOL(clientOpen, "jack_client_open");
	LOAD_JACK_SYMBOL(clientClose, "jack_client_close");
	LOAD_JACK_SYMBOL(activate, "jack_activate");
	LOAD_JACK_SYMBOL(deactivate, "jack_deactivate");
	LOAD_JACK_SYMBOL(setProcessCallback, "jack_set_process_callback");
	LOAD_JACK_SYMBOL(portRegister, "jack_port_register");
	LOAD_JACK_SYMBOL(portGetBuffer, "jack_port_get_buffer");
	LOAD_JACK_SYMBOL(getSampleRate, "jack_get_sample_rate");
	LOAD_JACK_SYMBOL(getBufferSize, "jack_get_buffer_size");

#undef LOAD_JACK_SYMBOL
	return true;

loadFailure:
	dlclose(jackApi.library);
	memset(&jackApi, 0, sizeof (jackApi));
	return false;
}

static void clearJackOutputs(jack_nframes_t sampleFrames)
{
	for (uint8_t channel = 0; channel < jackOutputBusCount * 2; channel++)
	{
		float *output = (float *)jackApi.portGetBuffer(jackPorts[channel], sampleFrames);
		if (output != NULL)
			memset(output, 0, sampleFrames * sizeof (float));
	}
}

static int jackProcessCallback(jack_nframes_t sampleFrames, void *userdata)
{
	(void)userdata;

	/*
	** Keep the JACK client active while FT2 performs editor operations. JACK
	** forgets live graph connections when a client is deactivated, so a paused
	** client must remain registered and publish silence instead.
	*/
	if (SDL_AtomicGet(&jackPauseRequested))
	{
		clearJackOutputs(sampleFrames);
		return 0;
	}

	/* Never make JACK's realtime thread wait for a UI/editor operation. */
	if (jackMutex == NULL || SDL_TryLockMutex(jackMutex) != 0)
	{
		clearJackOutputs(sampleFrames);
		return 0;
	}

	/*
	** The pause request can arrive after the callback's first check but before
	** it acquires the mixer lock. Check again while owning the lock so an
	** editor operation that has completed the pause handshake cannot race with
	** one last render using sample or voice pointers it is about to change.
	*/
	if (SDL_AtomicGet(&jackPauseRequested))
	{
		clearJackOutputs(sampleFrames);
		SDL_UnlockMutex(jackMutex);
		return 0;
	}

	float *outputs[32];
	for (uint8_t channel = 0; channel < jackOutputBusCount * 2; channel++)
		outputs[channel] = (float *)jackApi.portGetBuffer(jackPorts[channel], sampleFrames);

	if (jackRenderCallback != NULL)
		jackRenderCallback(outputs, sampleFrames, jackOutputBusCount, jackRenderUserdata);
	else
		clearJackOutputs(sampleFrames);

	SDL_UnlockMutex(jackMutex);
	return 0;
}

bool tapeheadJackLibraryAvailable(void)
{
	return loadJackLibrary();
}

bool tapeheadJackDeviceSelected(const char *deviceName)
{
	return deviceName != NULL && strcmp(deviceName, TAPEHEAD_JACK_DEVICE_NAME) == 0;
}

const char *tapeheadJackGetLastError(void)
{
	return jackLastError[0] != '\0' ? jackLastError : "Unknown JACK error.";
}

bool tapeheadJackOpen(uint8_t outputBusCount,
	tapeheadJackRenderCallback renderCallback, void *userdata,
	uint32_t *sampleRate, uint32_t *bufferSize)
{
	tapeheadJackClose();

	if (!loadJackLibrary())
		return false;

	if (outputBusCount < 1 || outputBusCount > 16)
	{
		setJackError("Invalid Tapehead JACK output-bus count.");
		return false;
	}

	jack_status_t status = 0;
	jackClient = jackApi.clientOpen("ft2_tapehead", JackNoStartServer, &status);
	if (jackClient == NULL)
	{
		setJackError("Could not connect to a running JACK or PipeWire-JACK server.");
		return false;
	}

	jackMutex = SDL_CreateMutex();
	if (jackMutex == NULL)
	{
		setJackError("Could not create the JACK audio lock.");
		goto openFailure;
	}

	jackOutputBusCount = outputBusCount;
	jackRenderCallback = renderCallback;
	jackRenderUserdata = userdata;
	SDL_AtomicSet(&jackPauseRequested, false);

	if (jackApi.setProcessCallback(jackClient, jackProcessCallback, NULL) != 0)
	{
		setJackError("Could not install Tapehead's JACK process callback.");
		goto openFailure;
	}

	for (uint8_t bus = 0; bus < outputBusCount; bus++)
	{
		char portName[32];
		const char busLetter = (char)('A' + bus);

		snprintf(portName, sizeof (portName), "bus_%c_L", busLetter);
		jackPorts[bus * 2] = jackApi.portRegister(jackClient, portName,
			JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

		snprintf(portName, sizeof (portName), "bus_%c_R", busLetter);
		jackPorts[(bus * 2) + 1] = jackApi.portRegister(jackClient, portName,
			JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

		if (jackPorts[bus * 2] == NULL || jackPorts[(bus * 2) + 1] == NULL)
		{
			setJackError("Could not register all requested Tapehead JACK output ports.");
			goto openFailure;
		}
	}

	if (sampleRate != NULL)
		*sampleRate = jackApi.getSampleRate(jackClient);
	if (bufferSize != NULL)
		*bufferSize = jackApi.getBufferSize(jackClient);

	jackLastError[0] = '\0';
	return true;

openFailure:
	tapeheadJackClose();
	return false;
}

bool tapeheadJackActivate(void)
{
	if (jackClient == NULL)
		return false;

	if (jackClientActive)
		return true;

	if (jackApi.activate(jackClient) != 0)
	{
		setJackError("Could not activate the Tapehead JACK client.");
		return false;
	}

	jackClientActive = true;
	return true;
}

void tapeheadJackClose(void)
{
	SDL_AtomicSet(&jackPauseRequested, true);

	if (jackClient != NULL)
	{
		if (jackClientActive)
			jackApi.deactivate(jackClient);

		jackApi.clientClose(jackClient);
		jackClient = NULL;
	}

	if (jackMutex != NULL)
	{
		SDL_DestroyMutex(jackMutex);
		jackMutex = NULL;
	}

	memset(jackPorts, 0, sizeof (jackPorts));
	jackRenderCallback = NULL;
	jackRenderUserdata = NULL;
	jackOutputBusCount = 0;
	jackClientActive = false;
	SDL_AtomicSet(&jackPauseRequested, false);
}

void tapeheadJackPause(bool pause)
{
	if (jackClient == NULL)
		return;

	if (pause)
	{
		/*
		** Stop future renders, then briefly take the mixer lock to wait for a
		** callback that may already be using FT2's voices/sample pointers.
		*/
		SDL_AtomicSet(&jackPauseRequested, true);
		if (jackMutex != NULL)
		{
			SDL_LockMutex(jackMutex);
			SDL_UnlockMutex(jackMutex);
		}
	}
	else
	{
		/*
		** A newly opened JACK client is intentionally activated only after FT2
		** has allocated and initialized all mixer buffers. setNewAudioSettings()
		** reaches this path through resumeAudio(). Later editor resumes see an
		** already-active client and therefore preserve every graph connection.
		*/
		if (!jackClientActive)
			(void)tapeheadJackActivate();

		SDL_AtomicSet(&jackPauseRequested, false);
	}
}

void tapeheadJackLock(void)
{
	if (jackMutex != NULL)
		SDL_LockMutex(jackMutex);
}

void tapeheadJackUnlock(void)
{
	if (jackMutex != NULL)
		SDL_UnlockMutex(jackMutex);
}

bool tapeheadJackIsOpen(void)
{
	return jackClient != NULL;
}

#else

bool tapeheadJackLibraryAvailable(void) { return false; }
bool tapeheadJackDeviceSelected(const char *deviceName)
{
	(void)deviceName;
	return false;
}
const char *tapeheadJackGetLastError(void)
{
	return "Native JACK virtual outputs are available on Linux only.";
}
bool tapeheadJackOpen(uint8_t outputBusCount,
	tapeheadJackRenderCallback renderCallback, void *userdata,
	uint32_t *sampleRate, uint32_t *bufferSize)
{
	(void)outputBusCount;
	(void)renderCallback;
	(void)userdata;
	(void)sampleRate;
	(void)bufferSize;
	return false;
}
bool tapeheadJackActivate(void) { return false; }
void tapeheadJackClose(void) { }
void tapeheadJackPause(bool pause) { (void)pause; }
void tapeheadJackLock(void) { }
void tapeheadJackUnlock(void) { }
bool tapeheadJackIsOpen(void) { return false; }

#endif
