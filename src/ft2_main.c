// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <time.h>
#include <math.h> // modf()
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <shlobj.h>
#include <SDL2/SDL_syswm.h>
#else
#include <unistd.h> // chdir()
#endif
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_audio.h"
#include "ft2_mouse.h"
#include "ft2_keyboard.h"
#include "ft2_config.h"
#include "ft2_sample_ed.h"
#include "ft2_diskop.h"
#include "scopes/ft2_scopes.h"
#include "scopes/ft2_scopedraw.h"
#include "ft2_about.h"
#include "ft2_pattern_ed.h"
#include "ft2_module_loader.h"
#include "ft2_sampling.h"
#include "ft2_audioselector.h"
#include "ft2_help.h"
#include "ft2_midi.h"
#include "ft2_midi_surface.h"
#include "ft2_apc40_mk2.h"
#include "ft2_events.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"
#include "ft2_hpc.h"
#include "ft2_smpfx.h"
#include "ft2_sample_launcher.h"
#include "ft2_sample_loader.h"
#include "ft2_pattern_launcher_ui.h"
#include "ft2_splash.h"
#include "ft2_tapesister_exchange.h"
#include "ft2_capture.h"
#include "ft2_palette.h"

static void initializeVars(void);
static void cleanUpAndExit(void); // never call this inside the main loop
static bool selectedAudioOutputIsDefault(void);
static bool confirmDefaultAudioFallback(void);
#ifdef __APPLE__
static void osxSetDirToProgramDirFromArgs(char **argv);
#endif

#ifdef _WIN32
static uint8_t readWindowsAudioBackendBeforeSDL(void);
static bool configureWindowsAudioBackendBeforeSDL(uint8_t backend);
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32 // test for SSE/SSE2 presence very first, to make sure no SSE/SSE2 code is attempted to be ran
	if (!SDL_HasSSE())
	{
		MessageBoxA(NULL, "Your computer's processor doesn't have the SSE instruction set " \
			"which is needed for this program to run. Sorry!", "Error", MB_ICONEXCLAMATION);
		return 0;
	}

	if (!SDL_HasSSE2())
	{
		MessageBoxA(NULL, "Your computer's processor doesn't have the SSE2 instruction set " \
			"which is needed for this program to run. Sorry!", "Error", MB_ICONEXCLAMATION);
		return 0;
	}
#endif

#if defined _WIN32 || defined __APPLE__
	SDL_version sdlVer;
#endif

	// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

#if SDL_MAJOR_VERSION == 2 && SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL < 5
#pragma message("WARNING: The SDL2 dev lib is older than ver 2.0.5. You'll get fullscreen mode issues and no audio input sampling.")
#pragma message("At least version 2.0.7 is recommended.")
#endif

	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
	SDL_EnableScreenSaver(); // allow screensaver to activate

	initializeVars();
	setupCrashHandler();

	// on Windows and macOS, test what version SDL2.DLL is (against library version used in compilation)
#if defined _WIN32 || defined __APPLE__
	SDL_GetVersion(&sdlVer);
	if (sdlVer.major != SDL_MAJOR_VERSION || sdlVer.minor != SDL_MINOR_VERSION || sdlVer.patch != SDL_PATCHLEVEL)
	{
#ifdef _WIN32
		showErrorMsgBox("SDL2.dll is not the expected version, the program will terminate.\n\n" \
		                "Loaded dll version: %d.%d.%d\n" \
		                "Required (compiled with) version: %d.%d.%d\n\n",
		                sdlVer.major, sdlVer.minor, sdlVer.patch,
		                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
#else
		showErrorMsgBox("The loaded SDL2 library is not the expected version, the program will terminate.\n\n" \
		                "Loaded library version: %d.%d.%d\n" \
		                "Required (compiled with) version: %d.%d.%d",
		                sdlVer.major, sdlVer.minor, sdlVer.patch,
		                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
#endif
		return 0;
	}
#endif

	hpc_Init();

	// ALT+F4 is used in FT2, but is "close program" in some cases...
#if SDL_MINOR_VERSION >= 24 || (SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL >= 4)
	SDL_SetHint("SDL_WINDOWS_NO_CLOSE_ON_ALT_F4", "1");
#endif

#ifdef _WIN32
#ifndef _MSC_VER
	SetProcessDPIAware();
#endif
#endif

	/* SDL 2.0.9 for Windows has a serious bug where you need to initialize the joystick subsystem
	** (even if you don't use it) or else weird things happen like random stutters, keyboard (rarely) being
	** reinitialized in Windows and what not.
	** Ref.: https://bugzilla.libsdl.org/show_bug.cgi?id=4391
	*/
#if defined _WIN32
	const uint8_t startupAudioBackend = readWindowsAudioBackendBeforeSDL();
	if (!configureWindowsAudioBackendBeforeSDL(startupAudioBackend))
		return 1;

	uint32_t sdlInitFlags = SDL_INIT_AUDIO | SDL_INIT_VIDEO;
#if SDL_MAJOR_VERSION == 2 && SDL_MINOR_VERSION == 0 && SDL_PATCHLEVEL == 9
	sdlInitFlags |= SDL_INIT_JOYSTICK;
#endif
	if (SDL_Init(sdlInitFlags) != 0)
#else
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0)
#endif
	{
#ifdef _WIN32
		showErrorMsgBox("Couldn't initialize SDL with Windows audio backend %s:\n%s",
			tapeheadAudioBackendName(startupAudioBackend), SDL_GetError());
#else
		showErrorMsgBox("Couldn't initialize SDL:\n%s", SDL_GetError());
#endif
		return 1;
	}

	SDL_SetHint("SDL_MOUSE_FOCUS_CLICKTHROUGH", "1");
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

	/* Text input is started by default in SDL2, turn it off to remove ~2ms spikes per key press.
	** We manuallay start it again when a text edit box is activated, and stop it when done.
	** Ref.: https://bugzilla.libsdl.org/show_bug.cgi?id=4166
	*/
	SDL_StopTextInput();

	hpc_SetDurationInHz(&video.vblankHpc, VBLANK_HZ);

#ifdef __APPLE__
	osxSetDirToProgramDirFromArgs(argv);
#endif
	if (!setupExecutablePath() || !loadBMPs() || !setupWindowedSincTables())
	{
		cleanUpAndExit();
		return 1;
	}

	loadConfigOrSetDefaults(); // config must be loaded at this exact point
	loadTapeheadConfig();
#ifdef _WIN32
	if (tapeheadConfig.audioBackend != startupAudioBackend)
	{
		fprintf(stderr,
			"Tapehead audio: startup backend path disagreed with loaded configuration "
			"(%s vs %s); using the startup selection for this session.\n",
			tapeheadAudioBackendName(startupAudioBackend),
			tapeheadAudioBackendName(tapeheadConfig.audioBackend));
	}
	const char *activeDriver = SDL_GetCurrentAudioDriver();
	fprintf(stderr, "Tapehead audio: configured backend=%s, active SDL backend=%s.\n",
		tapeheadAudioBackendName(startupAudioBackend),
		activeDriver != NULL ? activeDriver : "unknown");
#endif
	loadTapeheadPaletteOnStartup();
	tapeSisterExchangeInit();

	if (!setupWindow() || !setupRenderer())
	{
		// error message was shown in the functions above
		cleanUpAndExit();
		return 1;
	}

#ifdef _WIN32
	// allow only one instance, and send arguments to it (what song to play)
	if (handleSingleInstancing(argc, argv))
	{
		cleanUpAndExit();
		return 0; // close current instance, the main instance got a message now
	}
#endif

	if (!setupDiskOp())
	{
		cleanUpAndExit();
		return 1;
	}

	audio.currOutputDevice = getAudioOutputDeviceFromConfig();
	audio.currInputDevice = getAudioInputDeviceFromConfig();

	if (!setupAudio(CONFIG_HIDE_ERRORS)) // can we open the configured audio device?
	{
		if (!selectedAudioOutputIsDefault())
		{
			if (!confirmDefaultAudioFallback())
			{
				cleanUpAndExit();
				return 1;
			}

			fprintf(stderr,
				"Tapehead audio: user approved fallback from \"%s\" to the system default.\n",
				audioGetLastFailedOutputDevice());
			audio.startupDefaultFallback = true;
			setToDefaultAudioOutputDevice();
		}

		if (!setupAudio(CONFIG_HIDE_ERRORS))
		{
			// Keep the same device, but retry with conservative stream settings.
			config.audioFreq = 44100;
			config.specialFlags &= ~(BITDEPTH_32 + BUFFSIZE_512 + BUFFSIZE_2048);
			config.specialFlags |=  (BITDEPTH_16 + BUFFSIZE_1024);

			if (!setupAudio(CONFIG_SHOW_ERRORS))
			{
				cleanUpAndExit();
				return 1;
			}
		}
	}

	if (!setupReplayer() || !setupGUI() || !initScopes())
	{
		cleanUpAndExit();
		return 1;
	}

	pauseAudio();
	resumeAudio();
	rescanAudioDevices();
	if (tapeheadConfig.startWindow == TAPEHEAD_START_DECK_MATRIX)
	{
		patternLauncherSetStandaloneShown(true);
	}
	else if (tapeheadConfig.startWindow == TAPEHEAD_START_USE_LEGACY)
	{
		if (tapeheadConfig.launcherStandalone)
			patternLauncherSetStandaloneShown(true);
		else if (tapeheadConfig.launcherMode)
			patternLauncherSetPanelShown(true);
	}

#ifdef _WIN32 // on Windows we show the window at this point
	SDL_ShowWindow(video.window);
#endif

	if (config.windowFlags & START_IN_FULLSCR)
	{
		video.fullscreen = true;
		enterFullscreen();
	}

#ifdef HAS_MIDI
	// MIDI init can take several seconds, use thread
	midi.initMidiThread = SDL_CreateThread(initMidiFunc, "MIDI init thread", NULL);
	if (midi.initMidiThread == NULL)
	{
		showErrorMsgBox("Couldn't create MIDI initialization thread!");
		cleanUpAndExit();
		return 1;
	}
#endif

	hpc_ResetCounters(&video.vblankHpc); // quirk: this is needed for potential okBox() calls in handleModuleLoadFromArg()
	handleModuleLoadFromArg(argc, argv);

	if (tapeheadConfig.showSplashScreen && !showTapeheadSplash())
	{
		cleanUpAndExit();
		return 0;
	}

	editor.mainLoopOngoing = true;
	hpc_ResetCounters(&video.vblankHpc); // this must be the last thing we do before entering the main loop

	while (editor.programRunning)
	{
		beginFPSCounter();
		handleThreadEvents();
		readInput();
		handleEvents();
		tapeSisterExchangePoll(false);
		tapeheadCapturePoll();
		handleRecPlusExhaustion();
		handlePolyMatrixQHandoff();
		handlePatternLauncherStop();
		handlePatternLauncherPanelRefresh();
		handleRedrawing();
		/*
		** Playback updates can repaint the tracker's position, pattern-length and
		** tempo fields. The standalone launcher owns the complete framebuffer, so
		** draw it last to prevent those live fields from bleeding through it.
		*/
		if (patternLauncherStandaloneIsShown())
			patternLauncherDrawStandalone();
		flipFrame();
		endFPSCounter();
	}

	if (config.cfg_AutoSave)
		saveConfig(CONFIG_HIDE_ERRORS);

	cleanUpAndExit();
	return 0;
}

static void initializeVars(void)
{
	srand((uint32_t)time(NULL));

	// clear common structs
#ifdef HAS_MIDI
	memset(&midi, 0, sizeof (midi));
#endif
	memset(&video, 0, sizeof (video));
	memset(&keyb, 0, sizeof (keyb));
	memset(&mouse, 0, sizeof (mouse));
	memset(&editor, 0, sizeof (editor));
	memset((void *)&pattMark, 0, sizeof (pattMark));
	memset(&pattSync, 0, sizeof (pattSync));
	memset(&chSync, 0, sizeof (chSync));
	memset(&song, 0, sizeof (song));

	calcMiscReplayerVars();

	// used for scopes and sampling position line (sampler screen)
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		lastChInstr[i].instrNum = 255;
		lastChInstr[i].smpNum = 255;
	}

	// now set data that must be initialized to non-zero values...

	audio.locked = true; // XXX: Why..?
	audio.rescanAudioDevicesSupported = true;

	// set non-zero values

	editor.moduleSaveMode = MOD_SAVE_MODE_XM;
	editor.sampleSaveMode = SMP_SAVE_MODE_WAV;

	ui.sampleDataOrLoopDrag = -1;

	mouse.lastUsedObjectID = OBJECT_ID_NONE;

	editor.editRowSkip = 1;
	editor.srcInstr = 1;
	editor.curInstr = 1;
	editor.curOctave = 4;
	editor.smpEd_NoteNr = 1+NOTE_C4;

	editor.ptnJumpPos[0] = 0x00;
	editor.ptnJumpPos[1] = 0x10;
	editor.ptnJumpPos[2] = 0x20;
	editor.ptnJumpPos[3] = 0x30;

	editor.copyMaskEnable = true;
	memset(editor.copyMask, 1, sizeof (editor.copyMask));
	memset(editor.pasteMask, 1, sizeof (editor.pasteMask));

	editor.diskOpReadOnOpen = true;

	audio.linearPeriodsFlag = true;

#ifdef HAS_MIDI
	midi.enable = true;
#endif

	editor.programRunning = true;
}

static void cleanUpAndExit(void) // never call this inside the main loop!
{
	tapeSisterExchangeShutdown();
#ifdef HAS_MIDI
	// we used a thread to init MIDI (as it could take several seconds)
	if (midi.initMidiThread != NULL)
	{
		SDL_WaitThread(midi.initMidiThread, NULL);
		midi.initMidiThread = NULL;
	}

	midi.enable = false; // stop MIDI callback from doing things
	while (midi.callbackBusy) SDL_Delay(10); // wait for MIDI callback to finish

	tapeheadAPC40Mk2Close();
	tapeheadMidiSurfaceClose();
	closeMidiInDevice();
	freeMidiIn();
	midiDubPanic();
	freeMidiOut();
	freeMidiInputDeviceList();

	if (midi.inputDeviceName != NULL)
	{
		free(midi.inputDeviceName);
		midi.inputDeviceName = NULL;
	}

	if (editor.midiConfigFileLocationU != NULL)
	{
		free(editor.midiConfigFileLocationU);
		editor.midiConfigFileLocationU = NULL;
	}
#endif

	shutdownSamplePreview();
	sampleLauncherFree();
	closeAudio();
	closeReplayer();
	closeVideo();
	freeSprites();
	freeDiskOp();
	clearCopyBuffer();
	clearSampleUndo();
	freeAudioDeviceSelectorBuffers();
	windUpFTHelp();
	freeTextBoxes();
	freeMouseCursors();
	freeBMPs();

	if (editor.audioDevConfigFileLocationU != NULL)
	{
		free(editor.audioDevConfigFileLocationU);
		editor.audioDevConfigFileLocationU = NULL;
	}

	if (editor.configFileLocationU != NULL)
	{
		free(editor.configFileLocationU);
		editor.configFileLocationU = NULL;
	}

	if (editor.binaryPathU != NULL)
	{
		free(editor.binaryPathU);
		editor.binaryPathU = NULL;
	}

#ifdef _WIN32
	closeSingleInstancing();
#endif

	SDL_Quit();
}

#ifdef __APPLE__
static void osxSetDirToProgramDirFromArgs(char **argv)
{
	/* OS X/macOS: hackish way of setting the current working directory to the place where we double clicked
	** on the icon (for FT2.CFG loading)
	*/

	// if we launched from the terminal, argv[0][0] would be '.'
	if (argv[0] != NULL && argv[0][0] == DIR_DELIMITER) // don't do the hack if we launched from the terminal
	{
		char *tmpPath = strdup(argv[0]);
		if (tmpPath != NULL)
		{
			// cut off program filename
			int32_t tmpPathLen = strlen(tmpPath);
			for (int32_t i = tmpPathLen - 1; i >= 0; i--)
			{
				if (tmpPath[i] == DIR_DELIMITER)
				{
					tmpPath[i] = '\0';
					break;
				}
			}

			chdir(tmpPath); // path to binary
			chdir("../../../"); // we should now be in the directory where the config can be

			free(tmpPath);
		}
	}
}
#endif

static bool selectedAudioOutputIsDefault(void)
{
	return audio.currOutputDevice == NULL ||
		strcmp(audio.currOutputDevice, DEFAULT_AUDIO_DEV_STR) == 0;
}

static bool confirmDefaultAudioFallback(void)
{
	char message[768];
	snprintf(message, sizeof (message),
		"Tapehead could not open the selected output device:\n\n%.240s\n\n"
		"Reason: %.360s\n\n"
		"Use the system default output instead? Tapehead will not change devices "
		"unless you approve it.",
		audioGetLastFailedOutputDevice(), audioGetLastOpenError());

	const SDL_MessageBoxButtonData buttons[] =
	{
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Exit Tapehead" },
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Use System Default" }
	};
	const SDL_MessageBoxData data =
	{
		SDL_MESSAGEBOX_WARNING,
		video.window,
		"Tapehead audio device unavailable",
		message,
		2,
		buttons,
		NULL
	};

	int buttonID = 0;
	return SDL_ShowMessageBox(&data, &buttonID) == 0 && buttonID == 1;
}

#ifdef _WIN32
static char *trimStartupConfigText(char *text)
{
	while (isspace((unsigned char)*text)) text++;
	char *end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1])) end--;
	*end = '\0';
	return text;
}

static bool windowsFileExists(const wchar_t *path)
{
	FILE *file = _wfopen(path, L"rb");
	if (file == NULL)
		return false;
	fclose(file);
	return true;
}

static bool joinWindowsPath(wchar_t *destination, size_t capacity,
	const wchar_t *directory, const wchar_t *filename)
{
	const size_t length = wcslen(directory);
	const bool hasSeparator = length > 0 &&
		(directory[length-1] == L'\\' || directory[length-1] == L'/');
	const int written = _snwprintf(destination, capacity, hasSeparator
		? L"%ls%ls" : L"%ls\\%ls", directory, filename);
	if (written < 0 || (size_t)written >= capacity)
	{
		destination[0] = L'\0';
		return false;
	}
	return true;
}

static bool findWindowsTapeheadConfig(wchar_t *destination, size_t capacity)
{
	wchar_t directory[PATH_MAX+1], candidate[PATH_MAX+1];
	const DWORD moduleLength = GetModuleFileNameW(NULL, directory,
		(DWORD)(sizeof (directory) / sizeof (directory[0])));
	if (moduleLength > 0 && moduleLength < sizeof (directory) / sizeof (directory[0]))
	{
		wchar_t *separator = wcsrchr(directory, L'\\');
		if (separator != NULL)
		{
			*separator = L'\0';
			if (joinWindowsPath(candidate, sizeof (candidate) / sizeof (candidate[0]),
				directory, L"FT2.CFG") && windowsFileExists(candidate))
			{
				return joinWindowsPath(destination, capacity, directory,
					L"tapehead.ini");
			}
		}
	}

	const DWORD directoryCapacity =
		(DWORD)(sizeof (directory) / sizeof (directory[0]));
	const DWORD currentDirectoryLength = GetCurrentDirectoryW(directoryCapacity,
		directory);
	if (currentDirectoryLength > 0 && currentDirectoryLength < directoryCapacity &&
		joinWindowsPath(candidate, sizeof (candidate) / sizeof (candidate[0]),
			directory, L"FT2.CFG") && windowsFileExists(candidate))
	{
		return joinWindowsPath(destination, capacity, directory, L"tapehead.ini");
	}

	if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
		directory) == S_OK &&
		joinWindowsPath(candidate, sizeof (candidate) / sizeof (candidate[0]),
			directory, L"FT2 clone") &&
		joinWindowsPath(destination, capacity, candidate, L"tapehead.ini"))
	{
		return true;
	}

	return false;
}

static uint8_t readWindowsAudioBackendBeforeSDL(void)
{
	wchar_t configPath[PATH_MAX+1];
	if (!findWindowsTapeheadConfig(configPath,
		sizeof (configPath) / sizeof (configPath[0])))
	{
		return TAPEHEAD_AUDIO_BACKEND_AUTO;
	}

	FILE *file = _wfopen(configPath, L"r");
	if (file == NULL)
		return TAPEHEAD_AUDIO_BACKEND_AUTO;

	bool audioSection = false;
	uint8_t backend = TAPEHEAD_AUDIO_BACKEND_AUTO;
	char line[512];
	while (fgets(line, sizeof (line), file) != NULL)
	{
		char *text = trimStartupConfigText(line);
		if (*text == '\0' || *text == ';' || *text == '#')
			continue;

		if (*text == '[')
		{
			char *close = strchr(text, ']');
			if (close != NULL) *close = '\0';
			audioSection = !_stricmp(text + 1, "Audio");
			continue;
		}

		if (!audioSection)
			continue;

		char *equals = strchr(text, '=');
		if (equals == NULL)
			continue;
		*equals = '\0';
		char *key = trimStartupConfigText(text);
		char *value = trimStartupConfigText(equals + 1);
		if (_stricmp(key, "Backend") != 0)
			continue;

		backend = tapeheadParseAudioBackend(value);
		break;
	}

	fclose(file);
	return backend;
}

static bool configureWindowsAudioBackendBeforeSDL(uint8_t backend)
{
	const char *requestedDriver = NULL;
	if (backend == TAPEHEAD_AUDIO_BACKEND_WASAPI)
		requestedDriver = "wasapi";
	else if (backend == TAPEHEAD_AUDIO_BACKEND_DIRECTSOUND)
		requestedDriver = "directsound";

	if (requestedDriver != NULL &&
		!SDL_SetHintWithPriority(SDL_HINT_AUDIODRIVER, requestedDriver,
			SDL_HINT_OVERRIDE))
	{
		showErrorMsgBox("Couldn't select the configured Windows audio backend: %s",
			tapeheadAudioBackendName(backend));
		return false;
	}
	return true;
}
#endif
