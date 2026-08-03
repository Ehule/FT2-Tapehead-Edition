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
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_unicode.h"
#include "ft2_audio.h"
#include "ft2_sample_ed.h"
#include "ft2_mouse.h"
#include "ft2_diskop.h"
#include "ft2_sample_loader.h"
#include "ft2_sample_launcher.h"
#include "ft2_structs.h"
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

static void freeTmpSample(sample_t *s);

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

	if (undoStarted)
	{
		if (loadAsInstrFlag || adoptInstrumentName)
			undoInstrumentCommit();
		else
			undoSampleCommit();
	}

	setSongModifiedFlag();

	// when caught in main/video thread, it disables busy mouse and sets sampleIsLoading to true
	editor.updateCurSmp = true;

	return true;

loadError:
	if (undoStarted)
		undoCancelTransaction();
	setMouseBusy(false);
	freeTmpSample(&tmpSmp);
	sampleIsLoading = false;
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
	smpFilenameSet = false;

	memset(&tmpSmp, 0, sizeof (tmpSmp));
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



typedef struct sampleFolderFile_t
{
	UNICHAR *pathU;
	char *sortName;
	uint8_t destinationInstrument;
} sampleFolderFile_t;

typedef struct sampleFolderImportJob_t
{
	uint8_t mode, instrument;
	bool autoMap;
	uint32_t fileCount;
	sampleFolderFile_t *files;
} sampleFolderImportJob_t;

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

static bool decodeFolderSample(const UNICHAR *filenameU, sample_t *sample)
{
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	smpFilenameSet = false;

	FILE *f = UNICHAR_FOPEN(filenameU, "rb");
	if (f == NULL)
		return false;

	const int8_t format = detectSample(f);
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

static int32_t loadSampleFolderThread(void *ptr)
{
	sampleFolderImportJob_t *job = (sampleFolderImportJob_t *)ptr;
	uint32_t decodedCount = 0;
	sample_t *decodedSamples = (sample_t *)calloc(job->fileCount, sizeof (sample_t));
	if (decodedSamples == NULL)
	{
		loaderMsgBox("Not enough memory!");
		goto folderLoadError;
	}

	for (uint32_t i = 0; i < job->fileCount; i++)
	{
		if (!decodeFolderSample(job->files[i].pathU, &decodedSamples[decodedCount]))
		{
			loaderMsgBox("Couldn't load one of the folder samples. Nothing was changed.");
			goto folderLoadError;
		}
		decodedCount++;
	}

	if (job->mode == SAMPLE_FOLDER_IMPORT_LAUNCHER)
	{
		for (uint32_t i = 0; i < decodedCount; i++)
		{
			sanitizeSample(&decodedSamples[i]);
			fixSample(&decodedSamples[i]);
		}

		sampleLauncherAdoptDecodedFolder(decodedSamples, decodedCount);
		editor.updateCurSmp = true; /* clears the shared loader-busy flag */
		freeDecodedFolderSamples(decodedSamples, decodedCount);
		freeSampleFolderJob(job);
		return true;
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

		for (uint32_t i = 0; i < job->fileCount; i++)
		{
			const uint8_t destination = job->files[i].destinationInstrument;
			const bool undoStarted = undoInstrumentBegin(destination, "Import sample as instrument");

			lockMixerCallback();
			freeInstr(destination);
			instr[destination] = newInstruments[i];
			newInstruments[i] = NULL;
			memset(song.instrName[destination], 0, sizeof (song.instrName[destination]));
			memcpy(song.instrName[destination], instr[destination]->smp[0].name, 22);
			fixInstrAndSampleNames(destination);
			unlockMixerCallback();

			if (undoStarted)
				undoInstrumentCommit();
		}

		free(newInstruments);
		editor.curInstr = job->files[0].destinationInstrument;
		editor.curSmp = 0;
	}

	setSongModifiedFlag();
	editor.updateCurSmp = true;
	freeDecodedFolderSamples(decodedSamples, decodedCount);
	freeSampleFolderJob(job);
	return true;

folderLoadError:
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
	if (mode == SAMPLE_FOLDER_IMPORT_LAUNCHER &&
		job->fileCount > SAMPLE_LAUNCHER_MAX_TILES)
	{
		for (uint32_t i = SAMPLE_LAUNCHER_MAX_TILES; i < job->fileCount; i++)
		{
			free(job->files[i].pathU);
			free(job->files[i].sortName);
		}
		job->fileCount = SAMPLE_LAUNCHER_MAX_TILES;
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
