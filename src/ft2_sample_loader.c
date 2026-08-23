// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_unicode.h"
#include "ft2_audio.h"
#include "ft2_sample_ed.h"
#include "ft2_mouse.h"
#include "ft2_diskop.h"
#include "ft2_diskop_preview.h"
#include "ft2_sample_loader.h"
#include "ft2_sample_launcher.h"
#include "ft2_structs.h"
#include "ft2_tapesister_ack.h"
#include "ft2_undo.h"

bool detectFLAC(FILE *f);
bool loadFLAC(FILE *f, uint32_t filesize);

bool detectOGG(FILE *f);
bool loadOGG(FILE *f, uint32_t filesize);

bool detectMP3(FILE *f);
bool loadMP3(FILE *f, uint32_t filesize);

bool detectBRR(FILE *f);
bool loadBRR(FILE *f, uint32_t filesize);

bool loadAIFF(FILE *f, uint32_t filesize);
bool loadIFF(FILE *f, uint32_t filesize);
bool loadRAW(FILE *f, uint32_t filesize);
bool loadWAV(FILE *f, uint32_t filesize);

enum
{
	FORMAT_UNKNOWN = 0,
	FORMAT_IFF = 1,
	FORMAT_WAV = 2,
	FORMAT_AIFF = 3,
	FORMAT_FLAC = 4,
	FORMAT_OGG = 5,
	FORMAT_MP3 = 6,
	FORMAT_BRR = 7
};

// file extensions accepted by Disk Op. in sample mode
char *supportedSmpExtensions[] =
{
	"iff", "raw", "wav", "snd", "smp", "sam", "aif", "pat",
	"aiff", "flac", "ogg", "mp3", "brr", // IMPORTANT: Remember comma after last entry!

	"END_OF_LIST" // do NOT move, remove or edit this line!
};

// globals for sample loaders
bool loadAsInstrFlag, smpFilenameSet;
char *smpFilename;
uint8_t sampleSlot;
sample_t tmpSmp;
// --------------------------

static volatile bool sampleIsLoading;
static SDL_Thread *thread;

/* Sample decoders historically write through tmpSmp and global dialog
** callbacks. Serialize real imports and Disk Op previews around that legacy
** interface, while the preview request serial makes rapid browsing
** newest-selection-wins. */
static SDL_mutex *sampleDecodeMutex, *previewRequestMutex;
static SDL_cond *previewRequestCond;
static SDL_Thread *previewThread;
static bool previewThreadRunning, previewCancelRequested;
static bool previewShutdownRequested;
static volatile bool previewDecodeActive;
static bool previewAsInstrument;
static uint8_t previewTargetInstrument, previewTargetSample;
static diskOpPreviewRoute_t previewRoute;
static uint32_t previewRequestSerial;
static UNICHAR previewFilenameU[PATH_MAX + 1];

static void freeTmpSample(sample_t *s);
static void setImportedSampleName(sample_t *sample, const UNICHAR *filenameU);

static bool ensureSampleLoaderMutexes(void)
{
	if (sampleDecodeMutex == NULL)
		sampleDecodeMutex = SDL_CreateMutex();
	if (previewRequestMutex == NULL)
		previewRequestMutex = SDL_CreateMutex();
	if (previewRequestCond == NULL)
		previewRequestCond = SDL_CreateCond();

	return sampleDecodeMutex != NULL && previewRequestMutex != NULL &&
		previewRequestCond != NULL;
}

void sampleLoaderShowError(const char *fmt, ...)
{
	if (previewDecodeActive)
		return;

	char message[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(message, sizeof (message), fmt, args);
	va_end(args);
	loaderMsgBox("%s", message);
}

int16_t sampleLoaderAskStereo(int16_t type, const char *headline,
	const char *text, void (*checkBoxCallback)(void))
{
	if (previewDecodeActive)
		return STEREO_SAMPLE_MIX_TO_MONO;

	return loaderSysReq(type, headline, text, checkBoxCallback);
}

bool sampleLoaderIsPreviewDecode(void)
{
	return previewDecodeActive;
}

// Crude sample detection routine. These aren't always accurate detections!
static int8_t detectSample(FILE *f)
{
	uint8_t D[512];

	uint32_t oldPos = ftell(f);
	rewind(f);
	memset(D, 0, sizeof (D));
	fread(D, 1, sizeof (D), f);
	fseek(f, oldPos, SEEK_SET);

	if (detectFLAC(f))
		return FORMAT_FLAC;

	if (detectOGG(f))
		return FORMAT_OGG;

	if (detectMP3(f))
		return FORMAT_MP3;

	if (!memcmp("FORM", &D[0], 4) && (!memcmp("8SVX", &D[8], 4) || !memcmp("16SV", &D[8], 4)))
		return FORMAT_IFF;

	if (!memcmp("RIFF", &D[0], 4) && !memcmp("WAVE", &D[8], 4))
		return FORMAT_WAV;

	if (!memcmp("FORM", &D[0], 4) && (!memcmp("AIFF", &D[8], 4) || !memcmp("AIFC", &D[8], 4)))
		return FORMAT_AIFF;

	if (detectBRR(f))
		return FORMAT_BRR;

	return FORMAT_UNKNOWN;
}

static int32_t loadSampleThread(void *ptr)
{
	bool undoStarted = false;
	SDL_LockMutex(sampleDecodeMutex);
	loaderMsgBox = myLoaderMsgBoxThreadSafe;
	loaderSysReq = okBoxThreadSafe;
	/* The real import may have waited for an in-flight preview. Reinitialize
	** decoder globals here, after acquiring serialization, so preview metadata
	** cannot leak into the committed sample name. */
	smpFilenameSet = false;
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	if (editor.tmpFilenameU == NULL)
	{
		loaderMsgBox("General I/O error during loading!");
		goto loadError;
	}

	FILE *f = UNICHAR_FOPEN(editor.tmpFilenameU, "rb");
	if (f == NULL)
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		goto loadError;
	}

	int8_t format = detectSample(f);
	fseek(f, 0, SEEK_END);
	uint32_t filesize = ftell(f);

	if (filesize == 0)
	{
		fclose(f);
		loaderMsgBox("Error loading sample: The file is empty!");
		goto loadError;
	}

	bool sampleLoaded = false;

	rewind(f);
	switch (format)
	{
		case FORMAT_IFF: sampleLoaded = loadIFF(f, filesize); break;
		case FORMAT_WAV: sampleLoaded = loadWAV(f, filesize); break;
		case FORMAT_AIFF: sampleLoaded = loadAIFF(f, filesize); break;
		case FORMAT_FLAC: sampleLoaded = loadFLAC(f, filesize); break;
		case FORMAT_OGG: sampleLoaded = loadOGG(f, filesize); break;
		case FORMAT_MP3: sampleLoaded = loadMP3(f, filesize); break;
		case FORMAT_BRR: sampleLoaded = loadBRR(f, filesize); break;
		default: sampleLoaded = loadRAW(f, filesize); break;
	}
	fclose(f);

	if (!sampleLoaded)
		goto loadError;

	// sample loaded successfully!

	if (!smpFilenameSet) // if we didn't set a custom sample name in the loader, set it to its filename
	{
		char *tmpFilename = unicharToCp850(editor.tmpFilenameU, true);
		if (tmpFilename != NULL)
		{
			int32_t i = (int32_t)strlen(tmpFilename);
			while (i--)
			{
				if (tmpFilename[i] == DIR_DELIMITER)
					break;
			}

			char *tmpPtr = tmpFilename;
			if (i > 0)
				tmpPtr += i+1;

			sanitizeFilename(tmpPtr);

			int32_t filenameLen = (int32_t)strlen(tmpPtr);
			for (i = 0; i < 22; i++)
			{
				if (i < filenameLen)
					tmpSmp.name[i] = tmpPtr[i];
				else
					tmpSmp.name[i] = '\0';
			}

			free(tmpFilename);
		}
	}

	fixString(tmpSmp.name, 21); // remove leading spaces from sample filename

	const bool adoptInstrumentName = song.instrName[editor.curInstr][0] == '\0';
	if (loadAsInstrFlag || adoptInstrumentName)
	{
		const char *undoDescription = loadAsInstrFlag ? "Load sample as instrument" : "Load sample";
		undoStarted = undoInstrumentBegin(editor.curInstr, undoDescription);
	}
	else
	{
		undoStarted = undoSampleBegin(editor.curInstr, sampleSlot, "Load sample");
	}

	lockMixerCallback();
	if (loadAsInstrFlag) // if loaded in instrument mode
	{
		freeInstr(editor.curInstr);
		memset(song.instrName[editor.curInstr], 0, 23);
	}

	if (instr[editor.curInstr] == NULL)
		allocateInstr(editor.curInstr);

	if (instr[editor.curInstr] == NULL)
	{
		loaderMsgBox("Not enough memory!");
		goto loadError;
	}

	sample_t *s = &instr[editor.curInstr]->smp[sampleSlot];

	freeSample(editor.curInstr, sampleSlot);
	memcpy(s, &tmpSmp, sizeof (sample_t));

	if (adoptInstrumentName)
	{
		memcpy(song.instrName[editor.curInstr], tmpSmp.name, 22);
		song.instrName[editor.curInstr][22] = '\0';
	}

	sanitizeSample(s);

	fixSample(s); // prepares sample for branchless resampling interpolation
	fixInstrAndSampleNames(editor.curInstr);

	unlockMixerCallback();

	setSongModifiedFlag();
	if (undoStarted)
	{
		if (loadAsInstrFlag || adoptInstrumentName)
			undoInstrumentCommit();
		else
			undoSampleCommit();
	}

	// when caught in main/video thread, it disables busy mouse and sets sampleIsLoading to true
	editor.updateCurSmp = true;
	SDL_UnlockMutex(sampleDecodeMutex);

	return true;

loadError:
	if (undoStarted)
		undoCancelTransaction();
	setMouseBusy(false);
	freeTmpSample(&tmpSmp);
	sampleIsLoading = false;
	SDL_UnlockMutex(sampleDecodeMutex);
	return false;

	(void)ptr;
}

static void freeTmpSample(sample_t *s)
{
	freeSmpData(s);
}

void removeSampleIsLoadingFlag(void)
{
	sampleIsLoading = false;
}

bool loadSample(UNICHAR *filenameU, uint8_t smpNr, bool instrFlag)
{
	if (sampleIsLoading || filenameU == NULL)
		return false;
	if (!ensureSampleLoaderMutexes())
		return false;

	cancelSamplePreview();

	// setup message box functions
	loaderMsgBox = myLoaderMsgBoxThreadSafe;
	loaderSysReq = okBoxThreadSafe;

	if (editor.curInstr == 0)
	{
		loaderMsgBox("The zero-instrument cannot hold instrument data!");
		return false;
	}

	sampleSlot = smpNr;
	loadAsInstrFlag = instrFlag;
	sampleIsLoading = true;
	UNICHAR_STRCPY(editor.tmpFilenameU, filenameU);

	mouseAnimOn();
	thread = SDL_CreateThread(loadSampleThread, "sample load thread", NULL);
	if (thread == NULL)
	{
		sampleIsLoading = false;
		loaderMsgBox("Couldn't create thread!");
		return false;
	}

	SDL_DetachThread(thread);
	return true;
}

static bool decodePreviewSample(const UNICHAR *filenameU, sample_t *sample)
{
	bool sampleLoaded = false;
	SDL_LockMutex(sampleDecodeMutex);

	previewDecodeActive = true;
	smpFilenameSet = false;
	memset(&tmpSmp, 0, sizeof (tmpSmp));

	FILE *f = UNICHAR_FOPEN(filenameU, "rb");
	if (f != NULL)
	{
		const int8_t format = detectSample(f);
		fseek(f, 0, SEEK_END);
		const long fileSizeLong = ftell(f);
		if (format != FORMAT_UNKNOWN && fileSizeLong > 0 &&
			(uint64_t)fileSizeLong <= UINT32_MAX)
		{
			const uint32_t filesize = (uint32_t)fileSizeLong;
			rewind(f);
			switch (format)
			{
				case FORMAT_IFF: sampleLoaded = loadIFF(f, filesize); break;
				case FORMAT_WAV: sampleLoaded = loadWAV(f, filesize); break;
				case FORMAT_AIFF: sampleLoaded = loadAIFF(f, filesize); break;
				case FORMAT_FLAC: sampleLoaded = loadFLAC(f, filesize); break;
				case FORMAT_OGG: sampleLoaded = loadOGG(f, filesize); break;
				case FORMAT_MP3: sampleLoaded = loadMP3(f, filesize); break;
				case FORMAT_BRR: sampleLoaded = loadBRR(f, filesize); break;
				default: break;
			}
		}
		fclose(f);
	}

	if (sampleLoaded)
	{
		setImportedSampleName(&tmpSmp, filenameU);
		memcpy(sample, &tmpSmp, sizeof (sample_t));
		memset(&tmpSmp, 0, sizeof (tmpSmp));
	}
	else
	{
		freeTmpSample(&tmpSmp);
		memset(&tmpSmp, 0, sizeof (tmpSmp));
	}

	previewDecodeActive = false;
	SDL_UnlockMutex(sampleDecodeMutex);
	return sampleLoaded;
}

static int32_t previewSampleThread(void *ptr)
{
	(void)ptr;
	uint32_t handledSerial = 0;

	while (true)
	{
		UNICHAR filenameU[PATH_MAX + 1];
		SDL_LockMutex(previewRequestMutex);
		while (!previewShutdownRequested &&
			(previewCancelRequested || previewRequestSerial == handledSerial))
		{
			SDL_CondWait(previewRequestCond, previewRequestMutex);
		}
		if (previewShutdownRequested)
		{
			previewThreadRunning = false;
			SDL_UnlockMutex(previewRequestMutex);
			break;
		}

		const uint32_t requestSerial = previewRequestSerial;
		const uint8_t targetInstrument = previewTargetInstrument;
		const uint8_t targetSample = previewTargetSample;
		const bool asInstrument = previewAsInstrument;
		const diskOpPreviewRoute_t route = previewRoute;
		UNICHAR_STRNCPY(filenameU, previewFilenameU, PATH_MAX);
		filenameU[PATH_MAX] = 0;
		SDL_UnlockMutex(previewRequestMutex);

		sample_t decodedSample;
		memset(&decodedSample, 0, sizeof (decodedSample));
		const bool decoded = decodePreviewSample(filenameU, &decodedSample);

		SDL_LockMutex(previewRequestMutex);
		const bool requestIsCurrent = diskOpPreviewRequestIsCurrent(
			requestSerial, previewRequestSerial, previewCancelRequested,
			previewShutdownRequested);
		if (decoded && requestIsCurrent)
		{
			if (route == DISKOP_PREVIEW_ROUTE_PRIVATE)
				installDiskOpSamplePreview(&decodedSample);
			else if (route == DISKOP_PREVIEW_ROUTE_LIVE_LOAD &&
				targetInstrument > 0 && targetInstrument <= MAX_INST &&
				targetInstrument == editor.curInstr &&
				(asInstrument || targetSample == editor.curSmp))
			{
				const uint8_t destinationSample = asInstrument ? 0 : targetSample;
				const bool adoptInstrumentName = !asInstrument &&
					song.instrName[targetInstrument][0] == '\0';
				const bool undoStarted = asInstrument || adoptInstrumentName
					? undoInstrumentBegin(targetInstrument, "Live-load sample")
					: undoSampleBegin(targetInstrument, destinationSample,
						"Live-load sample");

				if (undoStarted)
				{
					lockMixerCallback();
					if (sampleLauncherInstrumentIsMapped(targetInstrument))
						sampleLauncherReset();
					if (asInstrument)
					{
						freeInstr(targetInstrument);
						memset(song.instrName[targetInstrument], 0, 23);
					}
					if (instr[targetInstrument] == NULL)
						allocateInstr(targetInstrument);

					if (instr[targetInstrument] != NULL)
					{
						freeSample(targetInstrument, destinationSample);
						diskOpPreviewMoveSample(
							&instr[targetInstrument]->smp[destinationSample],
							&decodedSample);
						sample_t *destination =
							&instr[targetInstrument]->smp[destinationSample];
						if (adoptInstrumentName)
						{
							memcpy(song.instrName[targetInstrument],
								destination->name, 22);
							song.instrName[targetInstrument][22] = '\0';
						}
						sanitizeSample(destination);
						fixSample(destination);
						fixInstrAndSampleNames(targetInstrument);
						unlockMixerCallback();
						setSongModifiedFlag();
						if (asInstrument || adoptInstrumentName)
							undoInstrumentCommit();
						else
							undoSampleCommit();
						editor.updateCurSmp = true;
					}
					else
					{
						unlockMixerCallback();
						undoCancelTransaction();
					}
				}
			}
		}
		SDL_UnlockMutex(previewRequestMutex);

		freeTmpSample(&decodedSample);
		handledSerial = requestSerial;
	}

	return 0;
}

static bool queueDiskOpSample(UNICHAR *filenameU,
	diskOpPreviewRoute_t route, uint8_t targetInstrument,
	uint8_t targetSample, bool asInstrument)
{
	if (filenameU == NULL || sampleIsLoading || !ensureSampleLoaderMutexes())
		return false;

	SDL_LockMutex(previewRequestMutex);
	if (previewShutdownRequested)
	{
		SDL_UnlockMutex(previewRequestMutex);
		return false;
	}
	if (route == DISKOP_PREVIEW_ROUTE_PRIVATE)
		stopDiskOpSamplePreview();
	previewCancelRequested = false;
	previewRoute = route;
	previewTargetInstrument = targetInstrument;
	previewTargetSample = targetSample;
	previewAsInstrument = asInstrument;
	UNICHAR_STRNCPY(previewFilenameU, filenameU, PATH_MAX);
	previewFilenameU[PATH_MAX] = 0;
	previewRequestSerial++;
	if (previewRequestSerial == 0)
		previewRequestSerial = 1;

	bool started = true;
	if (!previewThreadRunning)
	{
		previewThreadRunning = true;
		previewThread = SDL_CreateThread(previewSampleThread,
			"sample preview thread", NULL);
		if (previewThread == NULL)
		{
			previewThreadRunning = false;
			started = false;
		}
		else
			SDL_CondSignal(previewRequestCond);
	}
	else
		SDL_CondSignal(previewRequestCond);
	SDL_UnlockMutex(previewRequestMutex);
	return started;
}

bool previewSample(UNICHAR *filenameU)
{
	return queueDiskOpSample(filenameU, DISKOP_PREVIEW_ROUTE_PRIVATE,
		0, 0, false);
}

bool liveLoadSample(UNICHAR *filenameU, uint8_t targetInstrument,
	uint8_t targetSample, bool asInstrument)
{
	return queueDiskOpSample(filenameU, DISKOP_PREVIEW_ROUTE_LIVE_LOAD,
		targetInstrument, targetSample, asInstrument);
}

void cancelSamplePreview(void)
{
	if (previewRequestMutex != NULL)
	{
		SDL_LockMutex(previewRequestMutex);
		previewCancelRequested = true;
		previewRequestSerial++;
		SDL_CondSignal(previewRequestCond);
		SDL_UnlockMutex(previewRequestMutex);
	}

	stopDiskOpSamplePreview();
}

void shutdownSamplePreview(void)
{
	if (previewRequestMutex == NULL)
		return;

	SDL_LockMutex(previewRequestMutex);
	previewShutdownRequested = true;
	previewCancelRequested = true;
	previewRequestSerial++;
	if (previewRequestCond != NULL)
		SDL_CondSignal(previewRequestCond);
	SDL_Thread *threadToJoin = previewThread;
	SDL_UnlockMutex(previewRequestMutex);

	if (threadToJoin != NULL)
		SDL_WaitThread(threadToJoin, NULL);

	previewThread = NULL;
	previewThreadRunning = false;
	stopDiskOpSamplePreview();
}



typedef struct sampleFolderFile_t
{
	UNICHAR *pathU;
	char *sortName;
	uint8_t destinationInstrument, destinationSample;
} sampleFolderFile_t;

typedef struct sampleFolderImportJob_t
{
	uint8_t mode, instrument, launcherBank, launcherBankCount;
	uint8_t launcherInstrument[SAMPLE_LAUNCHER_BANK_COUNT][2];
	bool autoMap, exchangePreserveUnlistedSamples;
	uint32_t fileCount;
	sampleFolderFile_t *files;
	char launcherName[23];
	UNICHAR *exchangeFolderU, *exchangeAcknowledgementU;
	uint16_t matrixTiles[SAMPLE_LAUNCHER_MAX_TILES];
	uint32_t matrixRequested, matrixOmitted;
} sampleFolderImportJob_t;

static volatile bool matrixImportResultReady;
static volatile uint32_t matrixImportAdded, matrixImportRequested,
	matrixImportOmitted;

static void freeSampleFolderJob(sampleFolderImportJob_t *job)
{
	if (job == NULL)
		return;

	if (job->files != NULL)
	{
		for (uint32_t i = 0; i < job->fileCount; i++)
		{
			free(job->files[i].pathU);
			free(job->files[i].sortName);
		}
		free(job->files);
	}
	free(job->exchangeFolderU);
	free(job->exchangeAcknowledgementU);

	free(job);
}

static char *getFolderImportSortName(const UNICHAR *pathU)
{
	char *path = unicharToCp850((UNICHAR *)pathU, true);
	if (path == NULL)
		return NULL;

	char *name = strrchr(path, DIR_DELIMITER);
	if (name != NULL)
		name++;
	else
		name = path;

	const size_t nameLen = strlen(name);
	char *copy = (char *)malloc(nameLen + 1);
	if (copy != NULL)
		memcpy(copy, name, nameLen + 1);

	free(path);
	return copy;
}

static void getFolderImportName(const UNICHAR *folderPathU, char name[23])
{
	name[0] = '\0';
	char *path = unicharToCp850((UNICHAR *)folderPathU, true);
	if (path == NULL)
		return;

	size_t length = strlen(path);
	while (length > 0 && (path[length-1] == '/' || path[length-1] == '\\'))
		path[--length] = '\0';

	char *base = path;
	for (char *p = path; *p != '\0'; p++)
	{
		if (*p == '/' || *p == '\\')
			base = p + 1;
	}
	sanitizeFilename(base);
	strncpy(name, base, 22);
	name[22] = '\0';
	fixString(name, 21);
	free(path);
}

static int naturalSampleNameCompare(const void *a, const void *b)
{
	const unsigned char *s1 = (const unsigned char *)((const sampleFolderFile_t *)a)->sortName;
	const unsigned char *s2 = (const unsigned char *)((const sampleFolderFile_t *)b)->sortName;

	while (*s1 != '\0' && *s2 != '\0')
	{
		if (isdigit(*s1) && isdigit(*s2))
		{
			const unsigned char *run1 = s1;
			const unsigned char *run2 = s2;
			while (*s1 == '0') s1++;
			while (*s2 == '0') s2++;

			const unsigned char *digits1 = s1;
			const unsigned char *digits2 = s2;
			while (isdigit(*s1)) s1++;
			while (isdigit(*s2)) s2++;

			const size_t digitsLen1 = (size_t)(s1 - digits1);
			const size_t digitsLen2 = (size_t)(s2 - digits2);
			if (digitsLen1 != digitsLen2)
				return digitsLen1 < digitsLen2 ? -1 : 1;

			const int digitCompare = memcmp(digits1, digits2, digitsLen1);
			if (digitCompare != 0)
				return digitCompare;

			const size_t runLen1 = (size_t)(s1 - run1);
			const size_t runLen2 = (size_t)(s2 - run2);
			if (runLen1 != runLen2)
				return runLen1 < runLen2 ? -1 : 1;

			continue;
		}

		const int c1 = tolower(*s1++);
		const int c2 = tolower(*s2++);
		if (c1 != c2)
			return c1 < c2 ? -1 : 1;
	}

	if (*s1 == *s2)
		return 0;
	return *s1 == '\0' ? -1 : 1;
}

static UNICHAR *joinFolderSamplePath(const UNICHAR *folderPathU, const UNICHAR *fileNameU)
{
	const size_t folderLen = UNICHAR_STRLEN(folderPathU);
	const size_t fileLen = UNICHAR_STRLEN(fileNameU);
	const bool needsDelimiter = folderLen > 0 && folderPathU[folderLen-1] != DIR_DELIMITER;
	if (folderLen + (needsDelimiter ? 1 : 0) + fileLen > PATH_MAX)
		return NULL;

	UNICHAR *pathU = (UNICHAR *)malloc((folderLen + (needsDelimiter ? 1 : 0) + fileLen + 1) * sizeof (UNICHAR));
	if (pathU == NULL)
		return NULL;

	UNICHAR_STRCPY(pathU, folderPathU);
	if (needsDelimiter)
	{
#ifdef _WIN32
		UNICHAR_STRCAT(pathU, L"\\");
#else
		UNICHAR_STRCAT(pathU, "/");
#endif
	}
	UNICHAR_STRCAT(pathU, fileNameU);
	return pathU;
}

static void setImportedSampleName(sample_t *sample, const UNICHAR *filenameU)
{
	char *filename = unicharToCp850((UNICHAR *)filenameU, true);
	if (filename == NULL)
		return;

	char *name = strrchr(filename, DIR_DELIMITER);
	if (name != NULL)
		name++;
	else
		name = filename;

	sanitizeFilename(name);
	strncpy(sample->name, name, 22);
	sample->name[22] = '\0';
	fixString(sample->name, 21);
	free(filename);
}

static bool decodeFolderSample(const UNICHAR *filenameU, sample_t *sample,
	bool requireWav)
{
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	smpFilenameSet = false;

	FILE *f = UNICHAR_FOPEN(filenameU, "rb");
	if (f == NULL)
		return false;

	const int8_t format = detectSample(f);
	if (requireWav && format != FORMAT_WAV)
	{
		fclose(f);
		return false;
	}
	fseek(f, 0, SEEK_END);
	const long fileSizeLong = ftell(f);
	if (fileSizeLong <= 0 || (uint64_t)fileSizeLong > UINT32_MAX)
	{
		fclose(f);
		return false;
	}

	const uint32_t filesize = (uint32_t)fileSizeLong;
	bool sampleLoaded = false;
	rewind(f);
	switch (format)
	{
		case FORMAT_IFF: sampleLoaded = loadIFF(f, filesize); break;
		case FORMAT_WAV: sampleLoaded = loadWAV(f, filesize); break;
		case FORMAT_AIFF: sampleLoaded = loadAIFF(f, filesize); break;
		case FORMAT_FLAC: sampleLoaded = loadFLAC(f, filesize); break;
		case FORMAT_OGG: sampleLoaded = loadOGG(f, filesize); break;
		case FORMAT_MP3: sampleLoaded = loadMP3(f, filesize); break;
		case FORMAT_BRR: sampleLoaded = loadBRR(f, filesize); break;
		default: sampleLoaded = loadRAW(f, filesize); break;
	}
	fclose(f);

	if (!sampleLoaded)
	{
		freeTmpSample(&tmpSmp);
		memset(&tmpSmp, 0, sizeof (tmpSmp));
		return false;
	}

	setImportedSampleName(&tmpSmp, filenameU);
	memcpy(sample, &tmpSmp, sizeof (sample_t));
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	return true;
}

static void freeDecodedFolderSamples(sample_t *samples, uint32_t count)
{
	if (samples == NULL)
		return;

	for (uint32_t i = 0; i < count; i++)
		freeTmpSample(&samples[i]);
	free(samples);
}

static void initFolderInstrument(instr_t *instrument)
{
	memset(instrument, 0, sizeof (instr_t));
	for (uint32_t i = 0; i < MAX_SMP_PER_INST; i++)
	{
		instrument->smp[i].panning = 128;
		instrument->smp[i].volume = 64;
	}
	setStdEnvelope(instrument, 0, 3);
}

static void makeDefaultInstrumentName(uint8_t instrument, char *name)
{
	snprintf(name, 23, "Instrument %02u", (unsigned int)instrument);
	fixString(name, 21);
}

static instr_t *makeCurrentFolderInstrument(uint8_t instrument, sample_t *samples,
	uint32_t sampleCount, bool autoMap)
{
	instr_t *newInstrument = (instr_t *)calloc(1, sizeof (instr_t));
	if (newInstrument == NULL)
		return NULL;

	if (instr[instrument] != NULL)
	{
		memcpy(newInstrument, instr[instrument], sizeof (instr_t));
		memset(newInstrument->smp, 0, sizeof (newInstrument->smp));
		for (uint32_t i = 0; i < MAX_SMP_PER_INST; i++)
		{
			newInstrument->smp[i].panning = 128;
			newInstrument->smp[i].volume = 64;
		}
	}
	else
	{
		initFolderInstrument(newInstrument);
	}

	if (autoMap)
		memset(newInstrument->note2SampleLUT, 0, sizeof (newInstrument->note2SampleLUT));

	for (uint32_t i = 0; i < sampleCount; i++)
	{
		if (autoMap)
		{
			const int16_t compensatedNote = (int16_t)samples[i].relativeNote - (int16_t)i;
			samples[i].relativeNote = (int8_t)CLAMP(compensatedNote, INT8_MIN, INT8_MAX);
			newInstrument->note2SampleLUT[NOTE_C4 + i] = (uint8_t)i;
		}

		memcpy(&newInstrument->smp[i], &samples[i], sizeof (sample_t));
		memset(&samples[i], 0, sizeof (sample_t));
		sanitizeSample(&newInstrument->smp[i]);
		fixSample(&newInstrument->smp[i]);
	}

	return newInstrument;
}

static instr_t *makeSingleSampleInstrument(sample_t *sample)
{
	instr_t *newInstrument = (instr_t *)calloc(1, sizeof (instr_t));
	if (newInstrument == NULL)
		return NULL;

	initFolderInstrument(newInstrument);
	memcpy(&newInstrument->smp[0], sample, sizeof (sample_t));
	memset(sample, 0, sizeof (sample_t));
	sanitizeSample(&newInstrument->smp[0]);
	fixSample(&newInstrument->smp[0]);
	return newInstrument;
}

static void freeFolderInstrument(instr_t *instrument);

static instr_t *cloneExchangeDestinationInstrument(uint8_t destination,
	bool preserveUnlistedSamples)
{
	instr_t *instrument = calloc(1, sizeof (*instrument));
	if (instrument == NULL)
		return NULL;

	if (!preserveUnlistedSamples || instr[destination] == NULL)
	{
		initFolderInstrument(instrument);
		return instrument;
	}

	/* A page_instruments offer is a sparse patch: preserve the complete FT2
	** instrument and deep-copy every existing sample before replacing only the
	** explicitly listed slots. Nothing in the live song is touched here. */
	memcpy(instrument, instr[destination], sizeof (*instrument));
	memset(instrument->smp, 0, sizeof (instrument->smp));
	for (uint8_t sample = 0; sample < MAX_SMP_PER_INST; sample++)
	{
		if (!cloneSample(&instr[destination]->smp[sample],
			&instrument->smp[sample]))
		{
			freeFolderInstrument(instrument);
			return NULL;
		}
	}
	return instrument;
}

static instr_t *findOrCreateExchangeInstrument(instr_t **newInstruments,
	uint8_t *destinations, uint16_t *instrumentCount, uint8_t destination,
	bool preserveUnlistedSamples)
{
	for (uint16_t i = 0; i < *instrumentCount; i++)
		if (destinations[i] == destination)
			return newInstruments[i];

	if (*instrumentCount >= MAX_INST)
		return NULL;
	instr_t *instrument = cloneExchangeDestinationInstrument(destination,
		preserveUnlistedSamples);
	if (instrument == NULL)
		return NULL;
	destinations[*instrumentCount] = destination;
	newInstruments[*instrumentCount] = instrument;
	(*instrumentCount)++;
	return instrument;
}

static bool commitTapeSisterExchange(sampleFolderImportJob_t *job,
	sample_t *decodedSamples, uint32_t decodedCount)
{
	instr_t *newInstruments[MAX_INST] = { NULL };
	uint8_t destinations[MAX_INST] = { 0 };
	bool destinationWasPopulated[MAX_INST] = { false };
	char destinationNames[MAX_INST][23] = { { 0 } };
	uint16_t instrumentCount = 0;

	for (uint32_t i = 0; i < decodedCount; i++)
	{
		const uint8_t destination = job->files[i].destinationInstrument;
		const uint8_t sample = job->files[i].destinationSample;
		if (destination == 0 || sample >= MAX_SMP_PER_INST)
			goto allocationError;

		instr_t *instrument = findOrCreateExchangeInstrument(newInstruments,
			destinations, &instrumentCount, destination,
			job->exchangePreserveUnlistedSamples);
		if (instrument == NULL)
			goto allocationError;
		freeTmpSample(&instrument->smp[sample]);
		memcpy(&instrument->smp[sample], &decodedSamples[i], sizeof (sample_t));
		memset(&decodedSamples[i], 0, sizeof (sample_t));
		sanitizeSample(&instrument->smp[sample]);
		fixSample(&instrument->smp[sample]);
	}

	for (uint16_t i = 0; i < instrumentCount; i++)
	{
		const uint8_t destination = destinations[i];
		destinationWasPopulated[i] = instr[destination] != NULL ||
			song.instrName[destination][0] != '\0';
		if (job->exchangePreserveUnlistedSamples &&
			destinationWasPopulated[i])
		{
			memcpy(destinationNames[i], song.instrName[destination],
				sizeof (destinationNames[i]));
		}
		else
		{
			for (uint8_t sample = 0; sample < MAX_SMP_PER_INST; sample++)
			{
				if (newInstruments[i]->smp[sample].dataPtr != NULL)
				{
					memcpy(destinationNames[i],
						newInstruments[i]->smp[sample].name, 22);
					break;
				}
			}
		}
		fixString(destinationNames[i], 21);
		for (uint8_t sample = 0; sample < MAX_SMP_PER_INST; sample++)
			fixString(newInstruments[i]->smp[sample].name, 21);
	}

	bool undoReady = undoTransactionBegin("Import TapeSister Transfer");
	for (uint16_t i = 0; i < instrumentCount && undoReady; i++)
		undoReady = undoTransactionAddInstrument(destinations[i]);
	for (uint16_t i = 0; i < instrumentCount && undoReady; i++)
	{
		undoReady = undoTransactionPrepareInstrumentAfter(destinations[i],
			destinationNames[i], newInstruments[i]);
	}
	if (!undoReady)
	{
		undoCancelTransaction();
		loaderMsgBox("Not enough memory to create exchange Undo data. Nothing was changed.");
		goto allocationError;
	}
	if (!undoTransactionPreparedInstrumentsFitMemoryLimit())
	{
		undoCancelTransaction();
		loaderMsgBox("The exchange exceeds the configured Undo memory limit. Nothing was changed.");
		goto allocationError;
	}

	lockMixerCallback();
	for (uint16_t i = 0; i < instrumentCount; i++)
	{
		const uint8_t destination = destinations[i];
		freeInstr(destination);
		instr[destination] = newInstruments[i];
		newInstruments[i] = NULL;
		memcpy(song.instrName[destination], destinationNames[i],
			sizeof (song.instrName[destination]));
	}
	unlockMixerCallback();

	setSongModifiedFlag();
	undoTransactionCommit();
	editor.curInstr = job->files[0].destinationInstrument;
	editor.curSmp = job->files[0].destinationSample;
	editor.updateCurSmp = true;
	if (!tapeheadExchangeWriteAcknowledgement(job->exchangeAcknowledgementU))
	{
		loaderMsgBox("TapeSister samples were imported, but tapehead.received could not be written.");
	}
	return true;

allocationError:
	for (uint16_t i = 0; i < instrumentCount; i++)
		freeFolderInstrument(newInstruments[i]);
	return false;
}

static instr_t *makeLauncherBankInstrument(sample_t *samples,
	uint32_t sampleCount, uint32_t first)
{
	instr_t *newInstrument = (instr_t *)calloc(1, sizeof (instr_t));
	if (newInstrument == NULL)
		return NULL;

	initFolderInstrument(newInstrument);
	for (uint32_t i = 0; i < MAX_SMP_PER_INST && first + i < sampleCount; i++)
	{
		const int16_t compensatedNote =
			(int16_t)samples[first+i].relativeNote - (int16_t)i;
		samples[first+i].relativeNote =
			(int8_t)CLAMP(compensatedNote, INT8_MIN, INT8_MAX);
		newInstrument->note2SampleLUT[NOTE_C4 + i] = (uint8_t)i;
		memcpy(&newInstrument->smp[i], &samples[first+i], sizeof (sample_t));
		memset(&samples[first+i], 0, sizeof (sample_t));
		sanitizeSample(&newInstrument->smp[i]);
		fixSample(&newInstrument->smp[i]);
	}
	return newInstrument;
}

static void freeFolderInstrument(instr_t *instrument)
{
	if (instrument == NULL)
		return;
	for (uint32_t i = 0; i < MAX_SMP_PER_INST; i++)
		freeTmpSample(&instrument->smp[i]);
	free(instrument);
}

static int32_t loadSampleFolderThread(void *ptr)
{
	sampleFolderImportJob_t *job = (sampleFolderImportJob_t *)ptr;
	bool decodeMutexLocked = false;
	uint32_t decodedCount = 0;
	sample_t *decodedSamples = (sample_t *)calloc(job->fileCount, sizeof (sample_t));
	if (decodedSamples == NULL)
	{
		loaderMsgBox("Not enough memory!");
		goto folderLoadError;
	}

	SDL_LockMutex(sampleDecodeMutex);
	decodeMutexLocked = true;
	loaderMsgBox = myLoaderMsgBoxThreadSafe;
	loaderSysReq = okBoxThreadSafe;
	for (uint32_t i = 0; i < job->fileCount; i++)
	{
		if (!decodeFolderSample(job->files[i].pathU,
			&decodedSamples[decodedCount],
			job->mode == SAMPLE_FOLDER_IMPORT_TAPESISTER))
		{
			loaderMsgBox("Couldn't load one of the folder samples. Nothing was changed.");
			goto folderLoadError;
		}
		decodedCount++;
	}
	SDL_UnlockMutex(sampleDecodeMutex);
	decodeMutexLocked = false;

	if (job->mode == SAMPLE_FOLDER_IMPORT_TAPESISTER)
	{
		const bool committed = commitTapeSisterExchange(job, decodedSamples,
			decodedCount);
		freeDecodedFolderSamples(decodedSamples, decodedCount);
		freeSampleFolderJob(job);
		if (!committed)
		{
			setMouseBusy(false);
			sampleIsLoading = false;
		}
		return committed;
	}

	if (job->mode == SAMPLE_FOLDER_IMPORT_LAUNCHER)
	{
		instr_t *newInstrument[SAMPLE_LAUNCHER_BANK_COUNT][2] = { { NULL } };
		for (uint8_t bankOffset = 0; bankOffset < job->launcherBankCount;
			bankOffset++)
		{
			for (uint8_t half = 0; half < 2; half++)
			{
				if (job->launcherInstrument[bankOffset][half] == 0)
					continue;

				const uint32_t first =
					((uint32_t)bankOffset * SAMPLE_LAUNCHER_TILES_PER_BANK) +
					((uint32_t)half * MAX_SMP_PER_INST);
				newInstrument[bankOffset][half] = makeLauncherBankInstrument(
					decodedSamples, decodedCount, first);
				if (newInstrument[bankOffset][half] == NULL)
				{
					for (uint8_t freeBank = 0; freeBank <= bankOffset; freeBank++)
					{
						for (uint8_t freeHalf = 0; freeHalf < 2; freeHalf++)
							freeFolderInstrument(newInstrument[freeBank][freeHalf]);
					}
					loaderMsgBox("Not enough memory!");
					goto folderLoadError;
				}
			}
		}

		if (!undoTransactionBegin("Import Sample Banks") ||
			!undoTransactionAddSampleLauncher())
		{
			undoCancelTransaction();
			for (uint8_t freeBank = 0; freeBank < job->launcherBankCount; freeBank++)
				for (uint8_t freeHalf = 0; freeHalf < 2; freeHalf++)
					freeFolderInstrument(newInstrument[freeBank][freeHalf]);
			loaderMsgBox("Not enough memory to create undo data. Nothing was changed.");
			goto folderLoadError;
		}

		bool undoReady = true;
		for (uint8_t bankOffset = 0; bankOffset < job->launcherBankCount && undoReady; bankOffset++)
		{
			const uint8_t bank = job->launcherBank + bankOffset;
			uint8_t oldA = 0, oldB = 0;
			sampleLauncherGetBankInstruments(bank, &oldA, &oldB);
			if (oldA > 0) undoReady &= undoTransactionAddInstrument(oldA);
			if (oldB > 0) undoReady &= undoTransactionAddInstrument(oldB);
			for (uint8_t half = 0; half < 2; half++)
			{
				const uint8_t destination = job->launcherInstrument[bankOffset][half];
				if (destination > 0) undoReady &= undoTransactionAddInstrument(destination);
			}
		}
		if (!undoReady)
		{
			undoCancelTransaction();
			for (uint8_t freeBank = 0; freeBank < job->launcherBankCount; freeBank++)
				for (uint8_t freeHalf = 0; freeHalf < 2; freeHalf++)
					freeFolderInstrument(newInstrument[freeBank][freeHalf]);
			loaderMsgBox("Not enough memory to create undo data. Nothing was changed.");
			goto folderLoadError;
		}

		lockMixerCallback();
		sampleLauncherReset();
		for (uint8_t bankOffset = 0; bankOffset < job->launcherBankCount;
			bankOffset++)
		{
			const uint8_t bank = job->launcherBank + bankOffset;
			sampleLauncherClearBank(bank);
			for (uint8_t half = 0; half < 2; half++)
			{
				const uint8_t destination =
					job->launcherInstrument[bankOffset][half];
				if (destination == 0)
					continue;

				char instrumentName[23];
				sampleLauncherMakeInstrumentName(bank, half,
					job->launcherName, instrumentName);
				freeInstr(destination);
				instr[destination] = newInstrument[bankOffset][half];
				newInstrument[bankOffset][half] = NULL;
				memset(song.instrName[destination], 0,
					sizeof (song.instrName[destination]));
				memcpy(song.instrName[destination], instrumentName, 22);
				fixInstrAndSampleNames(destination);
			}
			sampleLauncherAttachBank(bank,
				job->launcherInstrument[bankOffset][0],
				job->launcherInstrument[bankOffset][1]);
		}
		unlockMixerCallback();

		editor.curInstr = job->launcherInstrument[0][0];
		editor.curSmp = 0;
		setSongModifiedFlag();
		undoTransactionCommit();
		editor.updateCurSmp = true;
		freeDecodedFolderSamples(decodedSamples, decodedCount);
		freeSampleFolderJob(job);
		return true;
	}

	if (job->mode == SAMPLE_FOLDER_IMPORT_MATRIX_OPEN)
	{
		if (!undoTransactionBegin("Import Samples to Matrix") ||
			!undoTransactionAddSampleLauncher())
		{
			undoCancelTransaction();
			loaderMsgBox("Not enough memory to create undo data. Nothing was changed.");
			goto folderLoadError;
		}

		uint32_t added = 0;
		for (uint32_t i = 0; i < decodedCount; i++)
		{
			const sampleLauncherPlaceResult_t result =
				sampleLauncherMoveDecodedSampleToTile(job->matrixTiles[i],
					&decodedSamples[i]);
			if (result != SAMPLE_LAUNCHER_PLACE_OK)
				break;
			added++;
		}
		if (added > 0)
		{
			sampleLauncherSelectTileInEditor(job->matrixTiles[0]);
			editor.updateCurSmp = true;
			undoTransactionCommit();
		}
		else
		{
			undoCancelTransaction();
		}
		matrixImportAdded = added;
		matrixImportRequested = job->matrixRequested;
		matrixImportOmitted = job->matrixOmitted + (decodedCount - added);
		matrixImportResultReady = true;
		freeDecodedFolderSamples(decodedSamples, decodedCount);
		freeSampleFolderJob(job);
		if (added == 0)
		{
			setMouseBusy(false);
			sampleIsLoading = false;
		}
		return added > 0;
	}

	if (job->mode == SAMPLE_FOLDER_IMPORT_CURRENT_INSTRUMENT)
	{
		instr_t *newInstrument = makeCurrentFolderInstrument(job->instrument,
			decodedSamples, decodedCount, job->autoMap);
		if (newInstrument == NULL)
		{
			loaderMsgBox("Not enough memory!");
			goto folderLoadError;
		}

		if (!undoInstrumentBegin(job->instrument, "Import sample folder"))
		{
			for (uint32_t i = 0; i < MAX_SMP_PER_INST; i++)
				freeTmpSample(&newInstrument->smp[i]);
			free(newInstrument);
			loaderMsgBox("Not enough memory to create undo data. Nothing was changed.");
			goto folderLoadError;
		}

		lockMixerCallback();
		freeInstr(job->instrument);
		instr[job->instrument] = newInstrument;
		if (song.instrName[job->instrument][0] == '\0')
			makeDefaultInstrumentName(job->instrument, song.instrName[job->instrument]);
		fixInstrAndSampleNames(job->instrument);
		unlockMixerCallback();

		setSongModifiedFlag();
		undoInstrumentCommit();
		editor.curSmp = 0;
	}
	else
	{
		instr_t **newInstruments = (instr_t **)calloc(job->fileCount, sizeof (instr_t *));
		if (newInstruments == NULL)
		{
			loaderMsgBox("Not enough memory!");
			goto folderLoadError;
		}

		for (uint32_t i = 0; i < job->fileCount; i++)
		{
			newInstruments[i] = makeSingleSampleInstrument(&decodedSamples[i]);
			if (newInstruments[i] == NULL)
			{
				for (uint32_t j = 0; j < i; j++)
				{
					freeTmpSample(&newInstruments[j]->smp[0]);
					free(newInstruments[j]);
				}
				free(newInstruments);
				loaderMsgBox("Not enough memory!");
				goto folderLoadError;
			}
		}

		bool undoReady = undoTransactionBegin("Import Samples");
		for (uint32_t i = 0; i < job->fileCount && undoReady; i++)
			undoReady = undoTransactionAddInstrument(job->files[i].destinationInstrument);
		if (!undoReady)
		{
			undoCancelTransaction();
			for (uint32_t i = 0; i < job->fileCount; i++)
				freeFolderInstrument(newInstruments[i]);
			free(newInstruments);
			loaderMsgBox("Not enough memory to create undo data. Nothing was changed.");
			goto folderLoadError;
		}

		for (uint32_t i = 0; i < job->fileCount; i++)
		{
			const uint8_t destination = job->files[i].destinationInstrument;
			lockMixerCallback();
			freeInstr(destination);
			instr[destination] = newInstruments[i];
			newInstruments[i] = NULL;
			memset(song.instrName[destination], 0, sizeof (song.instrName[destination]));
			memcpy(song.instrName[destination], instr[destination]->smp[0].name, 22);
			fixInstrAndSampleNames(destination);
			unlockMixerCallback();
		}

		setSongModifiedFlag();
		undoTransactionCommit();
		free(newInstruments);
		editor.curInstr = job->files[0].destinationInstrument;
		editor.curSmp = 0;
	}

	editor.updateCurSmp = true;
	freeDecodedFolderSamples(decodedSamples, decodedCount);
	freeSampleFolderJob(job);
	return true;

folderLoadError:
	if (decodeMutexLocked)
		SDL_UnlockMutex(sampleDecodeMutex);
	freeDecodedFolderSamples(decodedSamples, decodedCount);
	freeSampleFolderJob(job);
	setMouseBusy(false);
	sampleIsLoading = false;
	return false;
}

static uint32_t assignFolderInstrumentDestinations(sampleFolderImportJob_t *job)
{
	uint32_t assigned = 0;
	uint16_t start = job->instrument;
	if (start == 0)
		start = 1;

	for (uint16_t pass = 0; pass < 2 && assigned < job->fileCount; pass++)
	{
		const uint16_t first = pass == 0 ? start : 1;
		const uint16_t last = pass == 0 ? MAX_INST : (uint16_t)(start - 1);
		if (first > last)
			continue;

		for (uint16_t instrument = first; instrument <= last && assigned < job->fileCount; instrument++)
		{
			if (instr[instrument] == NULL && song.instrName[instrument][0] == '\0')
				job->files[assigned++].destinationInstrument = (uint8_t)instrument;
		}
	}

	return assigned;
}

bool loadSampleFolder(const UNICHAR *folderPathU, const UNICHAR *const *fileNamesU,
	uint32_t fileCount, uint8_t mode, bool autoMap)
{
	if (sampleIsLoading || folderPathU == NULL || fileNamesU == NULL || fileCount == 0)
		return false;
	if (!ensureSampleLoaderMutexes())
		return false;

	cancelSamplePreview();

	loaderMsgBox = myLoaderMsgBoxThreadSafe;
	loaderSysReq = okBoxThreadSafe;

	if (editor.curInstr == 0 && mode == SAMPLE_FOLDER_IMPORT_CURRENT_INSTRUMENT)
	{
		loaderMsgBox("The zero-instrument cannot hold instrument data!");
		return false;
	}

	sampleFolderImportJob_t *job = (sampleFolderImportJob_t *)calloc(1, sizeof (sampleFolderImportJob_t));
	if (job == NULL)
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}

	job->mode = mode;
	job->autoMap = autoMap;
	job->instrument = editor.curInstr;
	if (mode == SAMPLE_FOLDER_IMPORT_LAUNCHER)
	{
		job->launcherBank = sampleLauncherGetBank();
		getFolderImportName(folderPathU, job->launcherName);
	}
	job->fileCount = fileCount;
	job->files = (sampleFolderFile_t *)calloc(fileCount, sizeof (sampleFolderFile_t));
	if (job->files == NULL)
	{
		freeSampleFolderJob(job);
		loaderMsgBox("Not enough memory!");
		return false;
	}

	for (uint32_t i = 0; i < fileCount; i++)
	{
		job->files[i].pathU = joinFolderSamplePath(folderPathU, fileNamesU[i]);
		if (job->files[i].pathU != NULL)
			job->files[i].sortName = getFolderImportSortName(job->files[i].pathU);

		if (job->files[i].pathU == NULL || job->files[i].sortName == NULL)
		{
			freeSampleFolderJob(job);
			loaderMsgBox("Not enough memory or sample path too long!");
			return false;
		}
	}

	qsort(job->files, job->fileCount, sizeof (sampleFolderFile_t), naturalSampleNameCompare);
	if (mode == SAMPLE_FOLDER_IMPORT_LAUNCHER)
	{
		const uint32_t capacity = (SAMPLE_LAUNCHER_BANK_COUNT - job->launcherBank) *
			SAMPLE_LAUNCHER_TILES_PER_BANK;
		if (job->fileCount > capacity)
		{
			for (uint32_t i = capacity; i < job->fileCount; i++)
			{
				free(job->files[i].pathU);
				free(job->files[i].sortName);
			}
			job->fileCount = capacity;
		}

		if (!sampleLauncherPrepareRangeImport(job->launcherBank,
			job->fileCount, job->launcherInstrument, &job->launcherBankCount))
		{
			freeSampleFolderJob(job);
			loaderMsgBox("Not enough empty instrument slots for this Sample Bank range!");
			return false;
		}
	}
	else if (mode == SAMPLE_FOLDER_IMPORT_CURRENT_INSTRUMENT && job->fileCount > MAX_SMP_PER_INST)
	{
		for (uint32_t i = MAX_SMP_PER_INST; i < job->fileCount; i++)
		{
			free(job->files[i].pathU);
			free(job->files[i].sortName);
		}
		job->fileCount = MAX_SMP_PER_INST;
	}
	else if (mode == SAMPLE_FOLDER_IMPORT_INSTRUMENTS)
	{
		const uint32_t assigned = assignFolderInstrumentDestinations(job);
		if (assigned == 0)
		{
			freeSampleFolderJob(job);
			loaderMsgBox("There are no empty instrument slots!");
			return false;
		}

		for (uint32_t i = assigned; i < job->fileCount; i++)
		{
			free(job->files[i].pathU);
			free(job->files[i].sortName);
		}
		job->fileCount = assigned;
	}

	UNICHAR_STRNCPY(editor.tmpFilenameU, job->files[0].pathU, PATH_MAX);
	editor.tmpFilenameU[PATH_MAX] = 0;
	sampleIsLoading = true;
	mouseAnimOn();
	thread = SDL_CreateThread(loadSampleFolderThread, "sample folder load thread", job);
	if (thread == NULL)
	{
		sampleIsLoading = false;
		setMouseBusy(false);
		freeSampleFolderJob(job);
		loaderMsgBox("Couldn't create thread!");
		return false;
	}

	SDL_DetachThread(thread);
	return true;
}

bool loadSamplesToMatrix(const UNICHAR *folderPathU,
	const UNICHAR *const *fileNamesU, uint32_t fileCount, uint16_t startTile,
	bool replaceExactTile)
{
	if (sampleIsLoading || folderPathU == NULL || fileNamesU == NULL ||
		fileCount == 0 || startTile >= SAMPLE_LAUNCHER_MAX_TILES)
	{
		return false;
	}
	if (!ensureSampleLoaderMutexes())
		return false;

	cancelSamplePreview();

	loaderMsgBox = myLoaderMsgBoxThreadSafe;
	loaderSysReq = okBoxThreadSafe;
	sampleFolderImportJob_t *job = calloc(1, sizeof (*job));
	if (job == NULL)
		return false;
	job->mode = SAMPLE_FOLDER_IMPORT_MATRIX_OPEN;
	job->matrixRequested = fileCount;
	job->files = calloc(fileCount, sizeof (*job->files));
	if (job->files == NULL)
	{
		freeSampleFolderJob(job);
		return false;
	}

	uint32_t destinationCount = 0;
	if (replaceExactTile)
	{
		job->matrixTiles[destinationCount++] = startTile;
	}
	else
	{
		for (uint16_t tile = startTile;
			tile < SAMPLE_LAUNCHER_MAX_TILES && destinationCount < fileCount;
			tile++)
		{
			if (!sampleLauncherTileIsLoaded(tile) &&
				!sampleLauncherTileNaturalStorageIsLoaded(tile))
			{
				job->matrixTiles[destinationCount++] = tile;
			}
		}
	}
	if (destinationCount == 0)
	{
		freeSampleFolderJob(job);
		return false;
	}
	if (destinationCount > fileCount)
		destinationCount = fileCount;
	if (!sampleLauncherCanImportToTiles(job->matrixTiles, destinationCount))
	{
		freeSampleFolderJob(job);
		loaderMsgBox("Not enough free instrument slots for these Matrix tiles!");
		return false;
	}
	job->matrixOmitted = fileCount - destinationCount;
	job->fileCount = destinationCount;
	for (uint32_t i = 0; i < destinationCount; i++)
	{
		job->files[i].pathU = joinFolderSamplePath(folderPathU, fileNamesU[i]);
		if (job->files[i].pathU != NULL)
			job->files[i].sortName = getFolderImportSortName(job->files[i].pathU);
		if (job->files[i].pathU == NULL || job->files[i].sortName == NULL)
		{
			freeSampleFolderJob(job);
			return false;
		}
	}

	/* Browser order is already natural and user selection order is visual order.
	** Keep it intact so highlighted files land predictably on successive tiles. */
	UNICHAR_STRNCPY(editor.tmpFilenameU, job->files[0].pathU, PATH_MAX);
	editor.tmpFilenameU[PATH_MAX] = 0;
	matrixImportResultReady = false;
	sampleIsLoading = true;
	mouseAnimOn();
	thread = SDL_CreateThread(loadSampleFolderThread,
		"sample matrix import thread", job);
	if (thread == NULL)
	{
		sampleIsLoading = false;
		setMouseBusy(false);
		freeSampleFolderJob(job);
		return false;
	}
	SDL_DetachThread(thread);
	return true;
}

bool sampleLoaderIsBusy(void)
{
	return sampleIsLoading;
}

bool loadTapeSisterExchange(const UNICHAR *folderPathU,
	const tapeheadExchangeOffer_t *offer,
	const tapeheadExchangeDestination_t *destinations)
{
	if (sampleIsLoading || folderPathU == NULL || offer == NULL ||
		destinations == NULL || offer->count == 0 ||
		offer->count > TAPEHEAD_EXCHANGE_MAX_ITEMS)
	{
		return false;
	}
	if (!ensureSampleLoaderMutexes())
		return false;

	cancelSamplePreview();

	loaderMsgBox = myLoaderMsgBoxThreadSafe;
	loaderSysReq = okBoxThreadSafe;
	sampleFolderImportJob_t *job = calloc(1, sizeof (*job));
	if (job == NULL)
		return false;
	job->mode = SAMPLE_FOLDER_IMPORT_TAPESISTER;
	job->exchangePreserveUnlistedSamples = offer->layout ==
		TAPEHEAD_EXCHANGE_LAYOUT_PAGE_INSTRUMENTS;
	job->fileCount = offer->count;
	job->exchangeFolderU = UNICHAR_STRDUP(folderPathU);
	job->files = calloc(job->fileCount, sizeof (*job->files));
	if (job->exchangeFolderU == NULL || job->files == NULL)
	{
		freeSampleFolderJob(job);
		return false;
	}

#ifdef _WIN32
	static const UNICHAR acknowledgementName[] = L"tapehead.received";
#else
	static const UNICHAR acknowledgementName[] = "tapehead.received";
#endif
	job->exchangeAcknowledgementU = joinFolderSamplePath(folderPathU,
		acknowledgementName);
	if (job->exchangeAcknowledgementU == NULL)
	{
		freeSampleFolderJob(job);
		return false;
	}

	for (uint16_t i = 0; i < offer->count; i++)
	{
#ifdef _WIN32
		const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			offer->items[i].filename, -1, NULL, 0);
		UNICHAR *filenameU = needed > 0 ? malloc((size_t)needed * sizeof (UNICHAR)) : NULL;
		if (filenameU != NULL)
			MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
				offer->items[i].filename, -1, filenameU, needed);
#else
		UNICHAR *filenameU = strdup(offer->items[i].filename);
#endif
		if (filenameU == NULL)
		{
			freeSampleFolderJob(job);
			return false;
		}
		job->files[i].pathU = joinFolderSamplePath(folderPathU, filenameU);
		free(filenameU);
		job->files[i].sortName = strdup(offer->items[i].filename);
		job->files[i].destinationInstrument = destinations[i].instrument;
		job->files[i].destinationSample = destinations[i].sample - 1;
		if (job->files[i].pathU == NULL || job->files[i].sortName == NULL)
		{
			freeSampleFolderJob(job);
			return false;
		}
	}

	UNICHAR_STRNCPY(editor.tmpFilenameU, job->files[0].pathU, PATH_MAX);
	editor.tmpFilenameU[PATH_MAX] = 0;
	sampleIsLoading = true;
	mouseAnimOn();
	thread = SDL_CreateThread(loadSampleFolderThread,
		"TapeSister exchange import", job);
	if (thread == NULL)
	{
		sampleIsLoading = false;
		setMouseBusy(false);
		freeSampleFolderJob(job);
		return false;
	}
	SDL_DetachThread(thread);
	return true;
}

bool sampleMatrixImportTakeResult(uint32_t *added, uint32_t *requested,
	uint32_t *omitted)
{
	if (!matrixImportResultReady)
		return false;
	if (added != NULL) *added = matrixImportAdded;
	if (requested != NULL) *requested = matrixImportRequested;
	if (omitted != NULL) *omitted = matrixImportOmitted;
	matrixImportResultReady = false;
	return true;
}


void normalizeSigned32Bit(int32_t *sampleData, uint32_t sampleLength)
{
	uint32_t i;

	uint32_t sampleVolPeak = 0;
	for (i = 0; i < sampleLength; i++)
	{
		const uint32_t sample = ABS(sampleData[i]);
		if (sampleVolPeak < sample)
			sampleVolPeak = sample;
	}

	if (sampleVolPeak <= 0)
		return;

	const double dGain = (double)INT32_MAX / sampleVolPeak;
	for (i = 0; i < sampleLength; i++)
		sampleData[i] = (int32_t)(sampleData[i] * dGain);
}

void normalize32BitFloatToSigned16Bit(float *fSampleData, uint32_t sampleLength)
{
	uint32_t i;

	float fSampleVolPeak = 0.0f;
	for (i = 0; i < sampleLength; i++)
	{
		const float fSample = fabsf(fSampleData[i]);
		if (fSampleVolPeak < fSample)
			fSampleVolPeak = fSample;
	}

	if (fSampleVolPeak <= 0.0f)
		return;

	const float fGain = (float)INT16_MAX / fSampleVolPeak;
	for (i = 0; i < sampleLength; i++)
		fSampleData[i] *= fGain;
}

void normalize64BitFloatToSigned16Bit(double *dSampleData, uint32_t sampleLength)
{
	uint32_t i;

	double dSampleVolPeak = 0.0;
	for (i = 0; i < sampleLength; i++)
	{
		const double dSample = fabs(dSampleData[i]);
		if (dSampleVolPeak < dSample)
			dSampleVolPeak = dSample;
	}

	if (dSampleVolPeak <= 0.0)
		return;

	const double dGain = (double)INT16_MAX / dSampleVolPeak;
	for (i = 0; i < sampleLength; i++)
		dSampleData[i] *= dGain;
}
