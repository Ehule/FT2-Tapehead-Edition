#include "ft2_tapesister_exchange.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <wctype.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "ft2_config.h"
#include "ft2_diskop.h"
#include "ft2_header.h"
#include "ft2_sample_loader.h"
#include "ft2_sample_saver.h"
#include "ft2_structs.h"
#include "ft2_sysreqs.h"
#include "ft2_tapesister_protocol.h"
#include "ft2_unicode.h"

/* Leave room for a child name and Win32's extended-path prefix beyond the
** longest configured root/executable value. */
#define EXCHANGE_RUNTIME_PATH_CAPACITY (TAPEHEAD_CONFIG_PATH_CAPACITY + 512)
#define EXCHANGE_POLL_INTERVAL_MS 1000
#define EXCHANGE_PRESENCE_MAX_AGE_SECONDS 5

typedef struct exchangeRuntimeOffer_t
{
	tapeheadExchangeOffer_t manifest;
	UNICHAR folder[EXCHANGE_RUNTIME_PATH_CAPACITY];
	char folderName[256];
	uint64_t modified;
} exchangeRuntimeOffer_t;

typedef struct exchangeSource_t
{
	tapeheadExchangeOffer_t manifest;
	uint8_t instruments[TAPEHEAD_EXCHANGE_MAX_V1_ITEMS];
	uint8_t samples[TAPEHEAD_EXCHANGE_MAX_V1_ITEMS];
} exchangeSource_t;

typedef struct inboxScanJob_t
{
	UNICHAR *root;
	bool manual, found;
	SDL_atomic_t finished;
	exchangeRuntimeOffer_t offer;
	char diagnostic[192];
} inboxScanJob_t;

static uint32_t lastPollTick, lastPresenceTick;
static uint64_t *deferredFolders;
static size_t deferredFolderCount, deferredFolderCapacity;
static SDL_Thread *inboxScanThread;
static inboxScanJob_t *inboxScanJob;
static bool manualScanRequested;

static void showIncomingOffer(const exchangeRuntimeOffer_t *runtime);

static void exchangeRuntimeOfferInit(exchangeRuntimeOffer_t *runtime)
{
	memset(runtime, 0, sizeof (*runtime));
	tapeheadExchangeOfferInit(&runtime->manifest);
}

static void exchangeRuntimeOfferFree(exchangeRuntimeOffer_t *runtime)
{
	if (runtime == NULL)
		return;
	tapeheadExchangeOfferFree(&runtime->manifest);
	memset(runtime, 0, sizeof (*runtime));
}

static bool exchangeSourceInit(exchangeSource_t *source)
{
	memset(source, 0, sizeof (*source));
	tapeheadExchangeOfferInit(&source->manifest);
	source->manifest.version = 1;
	return tapeheadExchangeOfferReserve(&source->manifest,
		TAPEHEAD_EXCHANGE_MAX_V1_ITEMS);
}

static void exchangeSourceFree(exchangeSource_t *source)
{
	if (source != NULL)
		tapeheadExchangeOfferFree(&source->manifest);
}

#ifdef _WIN32
static const UNICHAR tapeheadPresenceName[] = L".tapehead.running";
static const UNICHAR tapeSisterPresenceName[] = L".tapesister.running";
#else
static const UNICHAR tapeheadPresenceName[] = ".tapehead.running";
static const UNICHAR tapeSisterPresenceName[] = ".tapesister.running";
#endif

static UNICHAR *pathFromUtf8(const char *path)
{
	if (path == NULL)
		return NULL;
#ifdef _WIN32
	int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
		NULL, 0);
	if (length <= 0)
		length = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
	if (length <= 0)
		return NULL;
	wchar_t *plain = malloc((size_t)(length + 8) * sizeof (wchar_t));
	if (plain == NULL)
		return NULL;
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, plain,
		length) <= 0 && MultiByteToWideChar(CP_ACP, 0, path, -1, plain, length) <= 0)
	{
		free(plain);
		return NULL;
	}

	/* Wide Win32 file APIs support long paths when an absolute path carries
	** the extended prefix. Keep ordinary short/relative paths unchanged. */
	const size_t plainLength = wcslen(plain);
	const bool driveAbsolute = plainLength >= 3 && iswalpha(plain[0]) &&
		plain[1] == L':' && (plain[2] == L'\\' || plain[2] == L'/');
	const bool uncAbsolute = plainLength >= 2 && plain[0] == L'\\' &&
		plain[1] == L'\\';
	if (plainLength >= MAX_PATH - 1 && (driveAbsolute || uncAbsolute) &&
		wcsncmp(plain, L"\\\\?\\", 4) != 0)
	{
		wchar_t *extended = malloc((plainLength + 9) * sizeof (wchar_t));
		if (extended == NULL)
		{
			free(plain);
			return NULL;
		}
		if (wcsncmp(plain, L"\\\\", 2) == 0)
			swprintf(extended, plainLength + 9, L"\\\\?\\UNC\\%ls", plain + 2);
		else
			swprintf(extended, plainLength + 9, L"\\\\?\\%ls", plain);
		free(plain);
		return extended;
	}
	return plain;
#else
	return strdup(path);
#endif
}

static bool joinPath(UNICHAR *destination, size_t capacity,
	const UNICHAR *left, const UNICHAR *right)
{
	if (destination == NULL || capacity == 0 || left == NULL || right == NULL)
		return false;
	const size_t leftLength = UNICHAR_STRLEN(left);
	const size_t rightLength = UNICHAR_STRLEN(right);
	const bool delimiter = leftLength > 0 && left[leftLength - 1] != '/' &&
		left[leftLength - 1] != '\\';
	if (leftLength + (delimiter ? 1 : 0) + rightLength >= capacity)
		return false;
	memcpy(destination, left, leftLength * sizeof (UNICHAR));
	size_t position = leftLength;
	if (delimiter)
		destination[position++] = DIR_DELIMITER;
	memcpy(&destination[position], right, (rightLength + 1) * sizeof (UNICHAR));
	return true;
}

static bool pathAttributes(const UNICHAR *path, bool *directory,
	uint64_t *modified)
{
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data))
		return false;
	if (directory != NULL)
		*directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	if (modified != NULL)
		*modified = ((uint64_t)data.ftLastWriteTime.dwHighDateTime << 32) |
			data.ftLastWriteTime.dwLowDateTime;
#else
	struct stat info;
	if (stat(path, &info) != 0)
		return false;
	if (directory != NULL)
		*directory = S_ISDIR(info.st_mode);
	if (modified != NULL)
	{
#if defined(__APPLE__)
		*modified = (uint64_t)info.st_mtimespec.tv_sec * UINT64_C(1000000000) +
			(uint64_t)info.st_mtimespec.tv_nsec;
#else
		*modified = (uint64_t)info.st_mtim.tv_sec * UINT64_C(1000000000) +
			(uint64_t)info.st_mtim.tv_nsec;
#endif
	}
#endif
	return true;
}

static bool pathExists(const UNICHAR *path)
{
	return pathAttributes(path, NULL, NULL);
}

static bool pathIsDirectory(const UNICHAR *path)
{
	bool directory = false;
	return pathAttributes(path, &directory, NULL) && directory;
}

static uint64_t currentModifiedTime(void)
{
#ifdef _WIN32
	FILETIME now;
	GetSystemTimeAsFileTime(&now);
	return ((uint64_t)now.dwHighDateTime << 32) | now.dwLowDateTime;
#else
	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now) != 0)
		return (uint64_t)time(NULL) * UINT64_C(1000000000);
	return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
		(uint64_t)now.tv_nsec;
#endif
}

static uint64_t presenceMaxAge(void)
{
#ifdef _WIN32
	return (uint64_t)EXCHANGE_PRESENCE_MAX_AGE_SECONDS * UINT64_C(10000000);
#else
	return (uint64_t)EXCHANGE_PRESENCE_MAX_AGE_SECONDS * UINT64_C(1000000000);
#endif
}

static bool presencePath(const UNICHAR *name,
	UNICHAR path[EXCHANGE_RUNTIME_PATH_CAPACITY])
{
	UNICHAR *root = pathFromUtf8(tapeheadConfig.tapeSisterExchangePath);
	const bool valid = root != NULL && pathIsDirectory(root) &&
		joinPath(path, EXCHANGE_RUNTIME_PATH_CAPACITY, root, name);
	free(root);
	return valid;
}

static void refreshTapeheadPresence(void)
{
	UNICHAR path[EXCHANGE_RUNTIME_PATH_CAPACITY];
	if (!presencePath(tapeheadPresenceName, path))
		return;
	FILE *file = UNICHAR_FOPEN(path, "wb");
	if (file == NULL)
		return;
#ifdef _WIN32
	fprintf(file, "pid=%lu\n", (unsigned long)GetCurrentProcessId());
#else
	fprintf(file, "pid=%ld\n", (long)getpid());
#endif
	fclose(file);
}

static bool tapeSisterIsRunning(void)
{
	UNICHAR path[EXCHANGE_RUNTIME_PATH_CAPACITY];
	uint64_t modified;
	if (!presencePath(tapeSisterPresenceName, path) ||
		!pathAttributes(path, NULL, &modified))
	{
		return false;
	}
	const uint64_t now = currentModifiedTime();
	const uint64_t maximumAge = presenceMaxAge();
	return modified > now ? modified - now <= maximumAge :
		now - modified <= maximumAge;
}

static bool makeDirectory(const UNICHAR *path)
{
#ifdef _WIN32
	return _wmkdir(path) == 0;
#else
	return mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
#endif
}

static void removeDirectory(const UNICHAR *path)
{
#ifdef _WIN32
	_wrmdir(path);
#else
	rmdir(path);
#endif
}

static uint64_t pathHash(const UNICHAR *path)
{
	uint64_t hash = UINT64_C(1469598103934665603);
	for (const UNICHAR *p = path; *p != 0; p++)
	{
		uint32_t value = (uint32_t)*p;
		for (uint8_t byte = 0; byte < sizeof (value); byte++)
		{
			hash ^= value & 0xFF;
			hash *= UINT64_C(1099511628211);
			value >>= 8;
		}
	}
	return hash;
}

static bool folderIsDeferred(const UNICHAR *folder)
{
	const uint64_t hash = pathHash(folder);
	for (size_t i = 0; i < deferredFolderCount; i++)
		if (deferredFolders[i] == hash)
			return true;
	return false;
}

static void deferFolder(const UNICHAR *folder)
{
	if (folderIsDeferred(folder))
		return;
	if (deferredFolderCount == deferredFolderCapacity)
	{
		const size_t newCapacity = deferredFolderCapacity == 0 ? 16 :
			deferredFolderCapacity * 2;
		uint64_t *grown = realloc(deferredFolders,
			newCapacity * sizeof (*deferredFolders));
		if (grown == NULL)
			return;
		deferredFolders = grown;
		deferredFolderCapacity = newCapacity;
	}
	deferredFolders[deferredFolderCount++] = pathHash(folder);
}

static bool unicodeEndsWithPartial(const UNICHAR *name)
{
#ifdef _WIN32
	static const UNICHAR suffix[] = L".partial";
#else
	static const UNICHAR suffix[] = ".partial";
#endif
	const size_t nameLength = UNICHAR_STRLEN(name);
	const size_t suffixLength = UNICHAR_STRLEN(suffix);
	return nameLength >= suffixLength &&
		UNICHAR_STRCMP(name + nameLength - suffixLength, suffix) == 0;
}

static void displayFolderName(const UNICHAR *name, char destination[256])
{
#ifdef _WIN32
	const int bytes = WideCharToMultiByte(CP_UTF8, 0, name, -1, destination,
		256, NULL, NULL);
	if (bytes <= 0)
		destination[0] = '\0';
#else
	strncpy(destination, name, 255);
	destination[255] = '\0';
#endif
}

static bool candidateIsComplete(const UNICHAR *folder, const UNICHAR *name,
	uint64_t modified, bool manual, exchangeRuntimeOffer_t *candidate,
	char *diagnostic, size_t diagnosticSize)
{
	if (unicodeEndsWithPartial(name) || (!manual && folderIsDeferred(folder)))
		return false;

#ifdef _WIN32
	static const UNICHAR manifestName[] = L"exchange.tsexchange";
	static const UNICHAR acknowledgementName[] = L"tapehead.received";
#else
	static const UNICHAR manifestName[] = "exchange.tsexchange";
	static const UNICHAR acknowledgementName[] = "tapehead.received";
#endif
	UNICHAR path[EXCHANGE_RUNTIME_PATH_CAPACITY];
	if (!joinPath(path, EXCHANGE_RUNTIME_PATH_CAPACITY, folder,
		acknowledgementName))
	{
		return false;
	}
	if (pathExists(path))
		return false;
	if (!joinPath(path, EXCHANGE_RUNTIME_PATH_CAPACITY, folder, manifestName))
		return false;
	FILE *file = UNICHAR_FOPEN(path, "rb");
	if (file == NULL)
		return false;

	char error[192];
	tapeheadExchangeOffer_t offer;
	tapeheadExchangeOfferInit(&offer);
	const bool parsed = tapeheadExchangeParseManifest(file, &offer, error,
		sizeof (error));
	fclose(file);
	if (!parsed || strcmp(offer.sender, "tapesister") != 0 ||
		strcmp(offer.recipient, "tapehead") != 0)
	{
		if (!parsed && diagnostic != NULL && diagnostic[0] == '\0')
		{
			snprintf(diagnostic, diagnosticSize,
				"Rejected TapeSister transfer %.80s: %.90s",
				candidate->folderName[0] != '\0' ? candidate->folderName :
				"(unnamed folder)", error);
		}
		tapeheadExchangeOfferFree(&offer);
		return false;
	}

	for (uint16_t i = 0; i < offer.count; i++)
	{
		UNICHAR *filenameU = pathFromUtf8(offer.items[i].filename);
		if (filenameU == NULL || !joinPath(path, EXCHANGE_RUNTIME_PATH_CAPACITY,
			folder, filenameU))
		{
			if (diagnostic != NULL && diagnostic[0] == '\0')
				snprintf(diagnostic, diagnosticSize,
					"Rejected TapeSister transfer: unsafe WAV path.");
			free(filenameU);
			tapeheadExchangeOfferFree(&offer);
			return false;
		}
		free(filenameU);
		bool directory = false;
		if (!pathAttributes(path, &directory, NULL) || directory)
		{
			if (diagnostic != NULL && diagnostic[0] == '\0')
				snprintf(diagnostic, diagnosticSize,
					"Rejected TapeSister transfer: referenced WAV %.80s is missing or unreadable.",
					offer.items[i].filename);
			tapeheadExchangeOfferFree(&offer);
			return false;
		}
	}

	candidate->manifest = offer;
	UNICHAR_STRNCPY(candidate->folder, folder,
		EXCHANGE_RUNTIME_PATH_CAPACITY - 1);
	candidate->folder[EXCHANGE_RUNTIME_PATH_CAPACITY - 1] = 0;
	displayFolderName(name, candidate->folderName);
	candidate->modified = modified;
	return true;
}

static bool considerCandidate(const UNICHAR *root, const UNICHAR *name,
	uint64_t modified, bool manual, exchangeRuntimeOffer_t *best, bool *found,
	char *diagnostic, size_t diagnosticSize)
{
	UNICHAR folder[EXCHANGE_RUNTIME_PATH_CAPACITY];
	if (!joinPath(folder, EXCHANGE_RUNTIME_PATH_CAPACITY, root, name) ||
		!pathIsDirectory(folder))
	{
		return true;
	}
	exchangeRuntimeOffer_t candidate;
	exchangeRuntimeOfferInit(&candidate);
	displayFolderName(name, candidate.folderName);
	if (candidateIsComplete(folder, name, modified, manual, &candidate,
		diagnostic, diagnosticSize) &&
		(!*found || candidate.modified > best->modified))
	{
		exchangeRuntimeOfferFree(best);
		*best = candidate;
		*found = true;
		return true;
	}
	exchangeRuntimeOfferFree(&candidate);
	return true;
}

static bool findPendingOffer(const UNICHAR *root, bool manual,
	exchangeRuntimeOffer_t *offer, char *diagnostic, size_t diagnosticSize)
{
	if (root == NULL || !pathIsDirectory(root))
		return false;
	bool found = false;
#ifdef _WIN32
	UNICHAR search[EXCHANGE_RUNTIME_PATH_CAPACITY];
	if (!joinPath(search, EXCHANGE_RUNTIME_PATH_CAPACITY, root, L"*"))
		return false;
	WIN32_FIND_DATAW data;
	HANDLE handle = FindFirstFileW(search, &data);
	if (handle != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (wcscmp(data.cFileName, L".") == 0 ||
				wcscmp(data.cFileName, L"..") == 0)
			{
				continue;
			}
			const uint64_t modified =
				((uint64_t)data.ftLastWriteTime.dwHighDateTime << 32) |
				data.ftLastWriteTime.dwLowDateTime;
			considerCandidate(root, data.cFileName, modified, manual, offer,
				&found, diagnostic, diagnosticSize);
		}
		while (FindNextFileW(handle, &data));
		FindClose(handle);
	}
#else
	DIR *directory = opendir(root);
	if (directory != NULL)
	{
		struct dirent *entry;
		while ((entry = readdir(directory)) != NULL)
		{
			if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0)
			{
				continue;
			}
			UNICHAR folder[EXCHANGE_RUNTIME_PATH_CAPACITY];
			uint64_t modified = 0;
			if (joinPath(folder, EXCHANGE_RUNTIME_PATH_CAPACITY, root,
				entry->d_name))
			{
				pathAttributes(folder, NULL, &modified);
			}
			considerCandidate(root, entry->d_name, modified, manual, offer,
				&found, diagnostic, diagnosticSize);
		}
		closedir(directory);
	}
#endif
	return found;
}

static int32_t scanInboxThread(void *ptr)
{
	inboxScanJob_t *job = (inboxScanJob_t *)ptr;
	job->found = findPendingOffer(job->root, job->manual, &job->offer,
		job->manual ? job->diagnostic : NULL, sizeof (job->diagnostic));
	SDL_AtomicSet(&job->finished, true);
	return 0;
}

static void freeInboxScanJob(inboxScanJob_t *job)
{
	if (job == NULL)
		return;
	free(job->root);
	exchangeRuntimeOfferFree(&job->offer);
	free(job);
}

static bool startInboxScan(bool manual)
{
	if (inboxScanThread != NULL || inboxScanJob != NULL)
		return false;

	inboxScanJob_t *job = calloc(1, sizeof (*job));
	if (job == NULL)
		return false;
	job->root = pathFromUtf8(tapeheadConfig.tapeSisterExchangePath);
	job->manual = manual;
	exchangeRuntimeOfferInit(&job->offer);
	SDL_AtomicSet(&job->finished, false);
	if (job->root == NULL)
	{
		freeInboxScanJob(job);
		return false;
	}

	inboxScanThread = SDL_CreateThread(scanInboxThread,
		"TapeSister inbox scan", job);
	if (inboxScanThread == NULL)
	{
		freeInboxScanJob(job);
		return false;
	}
	inboxScanJob = job;
	return true;
}

static bool inboxCanShowResult(void)
{
	return !ui.sysReqShown && !editor.editTextFlag &&
		!editor.samplingAudioFlag && !sampleLoaderIsBusy() &&
		!okBoxData.active;
}

static bool finishInboxScanIfReady(void)
{
	if (inboxScanThread == NULL || inboxScanJob == NULL ||
		!SDL_AtomicGet(&inboxScanJob->finished) || !inboxCanShowResult())
	{
		return false;
	}

	SDL_WaitThread(inboxScanThread, NULL);
	inboxScanThread = NULL;
	inboxScanJob_t *job = inboxScanJob;
	inboxScanJob = NULL;
	if (job->found)
		showIncomingOffer(&job->offer);
	else if (job->manual)
		okBox(0, "TapeSister Inbox", job->diagnostic[0] != '\0' ?
			job->diagnostic :
			"No complete unacknowledged TapeSister transfer was found.", NULL);
	freeInboxScanJob(job);
	return true;
}

static bool instrumentOccupied(uint8_t instrument)
{
	if (instrument == 0 || instrument > MAX_INST)
		return true;
	if (song.instrName[instrument][0] != '\0')
		return true;
	/* An allocated instrument can contain envelopes, maps, MIDI settings, or
	** timeline metadata even when every sample slot is empty. Treat it as
	** occupied so replacement is never presented as conflict-free. */
	return instr[instrument] != NULL;
}

static bool sampleOccupied(uint8_t instrument, uint8_t sample)
{
	if (instrument == 0 || instrument > MAX_INST || sample == 0 ||
		sample > MAX_SMP_PER_INST || instr[instrument] == NULL)
	{
		return false;
	}
	const sample_t *slot = &instr[instrument]->smp[sample - 1];
	return slot->dataPtr != NULL && slot->length > 0;
}

static bool destinationsAreEmpty(const tapeheadExchangeOffer_t *offer,
	uint8_t start)
{
	tapeheadExchangeDestination_t *destinations = calloc(offer->count,
		sizeof (*destinations));
	if (destinations == NULL || !tapeheadExchangeResolveDestinations(offer,
		start, destinations, NULL, 0))
	{
		free(destinations);
		return false;
	}
	for (uint16_t i = 0; i < offer->count; i++)
		if (instrumentOccupied(destinations[i].instrument))
		{
			free(destinations);
			return false;
		}
	free(destinations);
	return true;
}

static uint8_t preferredDestination(const tapeheadExchangeOffer_t *offer)
{
	uint8_t initial = editor.curInstr > 0 ? editor.curInstr : 1;
	if (destinationsAreEmpty(offer, initial))
		return initial;
	for (uint16_t instrument = initial + 1; instrument <= MAX_INST; instrument++)
		if (destinationsAreEmpty(offer, (uint8_t)instrument))
			return (uint8_t)instrument;
	for (uint16_t instrument = 1; instrument < initial; instrument++)
		if (destinationsAreEmpty(offer, (uint8_t)instrument))
			return (uint8_t)instrument;
	return initial;
}

static bool appendMessage(char *message, size_t capacity, const char *format, ...)
{
	const size_t used = strlen(message);
	if (used >= capacity)
		return false;
	va_list arguments;
	va_start(arguments, format);
	const int written = vsnprintf(message + used, capacity - used, format,
		arguments);
	va_end(arguments);
	return written >= 0 && (size_t)written < capacity - used;
}

static void showIncomingOffer(const exchangeRuntimeOffer_t *runtime)
{
	char instrumentText[4];
	snprintf(instrumentText, sizeof (instrumentText), "%u",
		preferredDestination(&runtime->manifest));
	if (inputBox(1, "TapeSister destination instrument (1-128):",
		instrumentText, sizeof (instrumentText) - 1) != 1)
	{
		deferFolder(runtime->folder);
		return;
	}
	char *end;
	const long selected = strtol(instrumentText, &end, 10);
	if (end == instrumentText || *end != '\0' || selected < 1 || selected > MAX_INST)
	{
		okBox(0, "TapeSister Inbox", "Destination instrument must be 1 through 128.", NULL);
		deferFolder(runtime->folder);
		return;
	}

	tapeheadExchangeDestination_t *destinations = calloc(runtime->manifest.count,
		sizeof (*destinations));
	char error[192];
	if (destinations == NULL || !tapeheadExchangeResolveDestinations(&runtime->manifest,
		(uint8_t)selected, destinations, error, sizeof (error)))
	{
		okBox(0, "TapeSister Inbox", destinations == NULL ?
			"Not enough memory to validate this transfer." : error, NULL);
		free(destinations);
		deferFolder(runtime->folder);
		return;
	}

	char message[4096] = { 0 };
	const bool multiPage = runtime->manifest.layout ==
		TAPEHEAD_EXCHANGE_LAYOUT_PAGE_INSTRUMENTS;
	const uint16_t span = tapeheadExchangeRelativeInstrumentSpan(
		&runtime->manifest);
	if (multiPage)
	{
		appendMessage(message, sizeof (message),
			"Multi-page TapeSister bank\nFolder: %.120s\n"
			"Pages/Instruments: %u\nWAV files: %u\n"
			"Page 1 -> Instrument %02ld\nPage %u -> Instrument %02ld\n",
			runtime->folderName, span, runtime->manifest.count, selected, span,
			selected + span - 1);
	}
	else
	{
		appendMessage(message, sizeof (message),
			"Folder: %.120s\nSamples: %u\nLayout: %s\n",
			runtime->folderName, runtime->manifest.count,
			tapeheadExchangeLayoutName(runtime->manifest.layout));
	}
	bool counted[MAX_INST + 1] = { false };
	uint8_t occupiedInstruments = 0;
	uint16_t occupiedSamples = 0, displayed = 0;
	for (uint16_t i = 0; i < runtime->manifest.count; i++)
	{
		const tapeheadExchangeItem_t *item = &runtime->manifest.items[i];
		const tapeheadExchangeDestination_t *destination = &destinations[i];
		const bool occupiedInstrument = instrumentOccupied(destination->instrument);
		const bool occupiedSlot = sampleOccupied(destination->instrument,
			destination->sample);
		if (occupiedSlot)
			occupiedSamples++;
		if (occupiedInstrument && !counted[destination->instrument])
		{
			counted[destination->instrument] = true;
			occupiedInstruments++;
		}
		if (!multiPage || displayed < 24)
		{
			if (multiPage)
			{
				appendMessage(message, sizeof (message),
					"Page %03u tile %02u -> I%03u:S%02u%s\n",
					item->ft2Instrument, item->tapeSisterTile,
					destination->instrument, destination->sample,
					occupiedSlot ? "  OCCUPIED SAMPLE" : "");
			}
			else
			{
				appendMessage(message, sizeof (message),
					"Tile %02u -> I%03u:S%02u%s\n", item->tapeSisterTile,
					destination->instrument, destination->sample,
					occupiedSlot ? "  OCCUPIED SAMPLE" : "");
			}
			displayed++;
		}
	}
	if (multiPage && runtime->manifest.count > displayed)
		appendMessage(message, sizeof (message), "...and %u more WAV mappings.\n",
			runtime->manifest.count - displayed);
	if (runtime->manifest.layout == TAPEHEAD_EXCHANGE_LAYOUT_INSTRUMENT_SAMPLES)
	{
		appendMessage(message, sizeof (message),
			"Import replaces instrument %03ld and clears all other sample slots.\n",
			selected);
	}
	else if (runtime->manifest.layout == TAPEHEAD_EXCHANGE_LAYOUT_SEPARATE_INSTRUMENTS)
	{
		appendMessage(message, sizeof (message),
			"Import replaces only the displayed destination instruments.\n");
	}
	else
	{
		appendMessage(message, sizeof (message),
			"Import updates only listed sample slots; empty TapeSister tiles do not erase samples.\n");
	}
	if (occupiedInstruments > 0 || occupiedSamples > 0)
		appendMessage(message, sizeof (message),
			"WARNING: %u destination instrument%s contain data; "
			"%u listed sample slot%s will be overwritten.",
			occupiedInstruments, occupiedInstruments == 1 ? "" : "s",
			occupiedSamples, occupiedSamples == 1 ? "" : "s");
	else
		appendMessage(message, sizeof (message), "No occupied destination conflicts.");

	const int16_t choice = okBox((occupiedInstruments > 0 || occupiedSamples > 0) ?
		SYSREQ_TYPE_TAPESISTER_REPLACE : SYSREQ_TYPE_TAPESISTER_IMPORT,
		"TapeSister Inbox", message, NULL);
	if (choice != 1)
	{
		free(destinations);
		deferFolder(runtime->folder);
		return;
	}
	deferFolder(runtime->folder);
	const bool started = loadTapeSisterExchange(runtime->folder,
		&runtime->manifest, destinations);
	free(destinations);
	if (!started)
		okBox(0, "TapeSister Inbox", "Could not start the atomic sample import.", NULL);
}

static void safeSampleName(const sample_t *sample, char *destination,
	size_t capacity)
{
	size_t position = 0;
	for (uint8_t i = 0; i < 22 && sample->name[i] != '\0' &&
		position + 1 < capacity; i++)
	{
		const unsigned char c = (unsigned char)sample->name[i];
		if (isalnum(c))
			destination[position++] = (char)c;
		else if (position > 0 && destination[position - 1] != '_')
			destination[position++] = '_';
	}
	while (position > 0 && destination[position - 1] == '_')
		position--;
	if (position == 0)
	{
		strncpy(destination, "Sample", capacity - 1);
		destination[capacity - 1] = '\0';
	}
	else
		destination[position] = '\0';
}

static bool collectCurrentInstrument(exchangeSource_t *source)
{
	if (!exchangeSourceInit(source) || editor.curInstr == 0 ||
		instr[editor.curInstr] == NULL)
		return false;
	strcpy(source->manifest.sender, "tapehead");
	strcpy(source->manifest.recipient, "tapesister");
	source->manifest.layout = TAPEHEAD_EXCHANGE_LAYOUT_INSTRUMENT_SAMPLES;
	for (uint8_t sample = 0; sample < MAX_SMP_PER_INST; sample++)
	{
		const sample_t *item = &instr[editor.curInstr]->smp[sample];
		if (item->dataPtr == NULL || item->length <= 0)
			continue;
		const uint8_t index = source->manifest.count++;
		source->instruments[index] = editor.curInstr;
		source->samples[index] = sample;
		source->manifest.items[index].tapeSisterTile = sample + 1;
		source->manifest.items[index].ft2Instrument = editor.curInstr;
		source->manifest.items[index].ft2Sample = sample + 1;
		char name[96];
		safeSampleName(item, name, sizeof (name));
		snprintf(source->manifest.items[index].filename,
			sizeof (source->manifest.items[index].filename),
			"I%03u_S%02u_%s.wav", editor.curInstr, sample + 1, name);
	}
	return source->manifest.count > 0;
}

static bool collectInstrumentRange(exchangeSource_t *source)
{
	if (!exchangeSourceInit(source) || editor.curInstr == 0)
		return false;
	strcpy(source->manifest.sender, "tapehead");
	strcpy(source->manifest.recipient, "tapesister");
	source->manifest.layout = TAPEHEAD_EXCHANGE_LAYOUT_SEPARATE_INSTRUMENTS;
	for (uint16_t instrument = editor.curInstr;
		instrument <= MAX_INST &&
		source->manifest.count < TAPEHEAD_EXCHANGE_MAX_V1_ITEMS;
		instrument++)
	{
		if (instr[instrument] == NULL)
			continue;
		for (uint8_t sample = 0; sample < MAX_SMP_PER_INST; sample++)
		{
			const sample_t *item = &instr[instrument]->smp[sample];
			if (item->dataPtr == NULL || item->length <= 0)
				continue;
			const uint8_t index = source->manifest.count++;
			source->instruments[index] = (uint8_t)instrument;
			source->samples[index] = sample;
			source->manifest.items[index].tapeSisterTile = index + 1;
			source->manifest.items[index].ft2Instrument = (uint8_t)instrument;
			source->manifest.items[index].ft2Sample = sample + 1;
			char name[96];
			safeSampleName(item, name, sizeof (name));
			snprintf(source->manifest.items[index].filename,
				sizeof (source->manifest.items[index].filename),
				"I%03u_S%02u_%s.wav", instrument, sample + 1, name);
			break;
		}
	}
	return source->manifest.count > 0;
}

static void cleanupPartial(const UNICHAR *folder, const exchangeSource_t *source)
{
	UNICHAR path[EXCHANGE_RUNTIME_PATH_CAPACITY];
	for (uint8_t i = 0; i < source->manifest.count; i++)
	{
		UNICHAR *filename = pathFromUtf8(source->manifest.items[i].filename);
		if (filename != NULL && joinPath(path, EXCHANGE_RUNTIME_PATH_CAPACITY,
			folder, filename))
		{
			UNICHAR_REMOVE(path);
		}
		free(filename);
	}
#ifdef _WIN32
	static const UNICHAR manifestName[] = L"exchange.tsexchange";
#else
	static const UNICHAR manifestName[] = "exchange.tsexchange";
#endif
	if (joinPath(path, EXCHANGE_RUNTIME_PATH_CAPACITY, folder, manifestName))
		UNICHAR_REMOVE(path);
	removeDirectory(folder);
}

static bool launchTapeSister(void)
{
	if (tapeheadConfig.tapeSisterExecutablePath[0] == '\0')
		return true;
	UNICHAR *executable = pathFromUtf8(tapeheadConfig.tapeSisterExecutablePath);
	if (executable == NULL)
		return false;
#ifdef _WIN32
	const size_t length = wcslen(executable);
	wchar_t *commandLine = malloc((length + 3) * sizeof (wchar_t));
	if (commandLine == NULL)
	{
		free(executable);
		return false;
	}
	swprintf(commandLine, length + 3, L"\"%ls\"", executable);
	STARTUPINFOW startup;
	PROCESS_INFORMATION process;
	memset(&startup, 0, sizeof (startup));
	memset(&process, 0, sizeof (process));
	startup.cb = sizeof (startup);
	const bool launched = CreateProcessW(executable, commandLine, NULL, NULL,
		FALSE, 0, NULL, NULL, &startup, &process) != 0;
	if (launched)
	{
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
	}
	free(commandLine);
#else
	if (access(executable, X_OK) != 0)
	{
		free(executable);
		return false;
	}
	const pid_t child = fork();
	const bool launched = child >= 0;
	if (child == 0)
	{
		execl(executable, executable, (char *)NULL);
		_exit(127);
	}
#endif
	free(executable);
	return launched;
}

static bool publishSource(const exchangeSource_t *source, char *folderName,
	size_t folderNameCapacity)
{
	UNICHAR *root = pathFromUtf8(tapeheadConfig.tapeSisterExchangePath);
	if (root == NULL || !pathIsDirectory(root))
	{
		free(root);
		return false;
	}
	UNICHAR finalFolder[EXCHANGE_RUNTIME_PATH_CAPACITY];
	UNICHAR partialFolder[EXCHANGE_RUNTIME_PATH_CAPACITY];
	bool created = false;
	for (uint32_t number = 1; number <= 999999; number++)
	{
		char finalName[64], partialName[72];
		snprintf(finalName, sizeof (finalName),
			"tapehead_to_tapesister_%06u", number);
		snprintf(partialName, sizeof (partialName), "%s.partial", finalName);
		UNICHAR *finalNameU = pathFromUtf8(finalName);
		UNICHAR *partialNameU = pathFromUtf8(partialName);
		const bool joined = finalNameU != NULL && partialNameU != NULL &&
			joinPath(finalFolder, EXCHANGE_RUNTIME_PATH_CAPACITY, root, finalNameU) &&
			joinPath(partialFolder, EXCHANGE_RUNTIME_PATH_CAPACITY, root, partialNameU);
		free(finalNameU);
		free(partialNameU);
		if (!joined)
			break;
		if (pathExists(finalFolder) || pathExists(partialFolder))
			continue;
		if (makeDirectory(partialFolder))
		{
			strncpy(folderName, finalName, folderNameCapacity - 1);
			folderName[folderNameCapacity - 1] = '\0';
			created = true;
		}
		break;
	}
	free(root);
	if (!created)
		return false;

	bool success = true;
	UNICHAR path[EXCHANGE_RUNTIME_PATH_CAPACITY];
	for (uint8_t i = 0; i < source->manifest.count && success; i++)
	{
		const uint8_t instrument = source->instruments[i];
		const uint8_t sample = source->samples[i];
		UNICHAR *filename = pathFromUtf8(source->manifest.items[i].filename);
		success = filename != NULL &&
			joinPath(path, EXCHANGE_RUNTIME_PATH_CAPACITY, partialFolder, filename) &&
			instr[instrument] != NULL &&
			saveWAVSampleDirect(path, instr[instrument],
				&instr[instrument]->smp[sample]);
		free(filename);
	}

#ifdef _WIN32
	static const UNICHAR manifestName[] = L"exchange.tsexchange";
#else
	static const UNICHAR manifestName[] = "exchange.tsexchange";
#endif
	FILE *manifest = NULL;
	if (success && joinPath(path, EXCHANGE_RUNTIME_PATH_CAPACITY, partialFolder,
		manifestName))
	{
		manifest = UNICHAR_FOPEN(path, "wb");
	}
	if (manifest == NULL)
		success = false;
	if (success)
	{
		fprintf(manifest,
			"TAPESISTER_EXCHANGE 1\nsender=tapehead\nrecipient=tapesister\n"
			"layout=%s\ncount=%u\n",
			tapeheadExchangeLayoutName(source->manifest.layout),
			source->manifest.count);
		for (uint8_t i = 0; i < source->manifest.count; i++)
		{
			const tapeheadExchangeItem_t *item = &source->manifest.items[i];
			fprintf(manifest, "item=%u,%u,%u,%s\n", item->tapeSisterTile,
				item->ft2Instrument, item->ft2Sample, item->filename);
		}
		success = ferror(manifest) == 0 && fclose(manifest) == 0;
		manifest = NULL;
	}
	if (manifest != NULL)
		fclose(manifest);
	if (success)
		success = UNICHAR_RENAME(partialFolder, finalFolder) == 0;
	if (!success)
	{
		cleanupPartial(partialFolder, source);
		return false;
	}
	return true;
}

static void confirmAndPublish(const exchangeSource_t *source)
{
	char message[4096] = { 0 };
	appendMessage(message, sizeof (message), "Layout: %s\nSamples: %u\n",
		tapeheadExchangeLayoutName(source->manifest.layout), source->manifest.count);
	for (uint8_t i = 0; i < source->manifest.count; i++)
	{
		const tapeheadExchangeItem_t *item = &source->manifest.items[i];
		appendMessage(message, sizeof (message),
			"I%03u:S%02u -> TapeSister tile %02u\n",
			item->ft2Instrument, item->ft2Sample, item->tapeSisterTile);
	}
	appendMessage(message, sizeof (message),
		"WAV files and the manifest will be published atomically.");
	const int16_t choice = okBox(SYSREQ_TYPE_TAPESISTER_PUBLISH,
		"Send to TapeSister", message, NULL);
	if (choice != 1 && choice != 2)
	{
		return;
	}
	const bool forceNewInstance = choice == 2;
	char folderName[64];
	if (!publishSource(source, folderName, sizeof (folderName)))
	{
		okBox(0, "Send to TapeSister",
			"Could not publish the transfer. No completed folder was created.", NULL);
		return;
	}
	char result[256];
	if (!forceNewInstance && tapeSisterIsRunning())
	{
		snprintf(result, sizeof (result),
			"Published %s. Open TapeSister will receive it.", folderName);
	}
	else if (tapeheadConfig.tapeSisterExecutablePath[0] == '\0')
	{
		snprintf(result, sizeof (result),
			"Published %s. TapeSister executable path is blank.", folderName);
	}
	else if (!launchTapeSister())
	{
		snprintf(result, sizeof (result),
			"Published %s, but TapeSister could not be launched.", folderName);
	}
	else
	{
		snprintf(result, sizeof (result), forceNewInstance ?
			"Published %s and launched another TapeSister." :
			"Published %s and launched TapeSister.", folderName);
	}
	okBox(0, "Send to TapeSister", result, NULL);
}

void tapeSisterExchangeInit(void)
{
	lastPollTick = SDL_GetTicks() - EXCHANGE_POLL_INTERVAL_MS;
	lastPresenceTick = SDL_GetTicks();
	free(deferredFolders);
	deferredFolders = NULL;
	deferredFolderCount = 0;
	deferredFolderCapacity = 0;
	inboxScanThread = NULL;
	inboxScanJob = NULL;
	manualScanRequested = false;
	refreshTapeheadPresence();
}

void tapeSisterExchangeShutdown(void)
{
	if (inboxScanThread != NULL)
	{
		SDL_WaitThread(inboxScanThread, NULL);
		inboxScanThread = NULL;
	}
	freeInboxScanJob(inboxScanJob);
	inboxScanJob = NULL;
	manualScanRequested = false;
	free(deferredFolders);
	deferredFolders = NULL;
	deferredFolderCount = 0;
	deferredFolderCapacity = 0;
}

void tapeSisterExchangePoll(bool manualRequest)
{
	const uint32_t now = SDL_GetTicks();
	if (manualRequest)
		manualScanRequested = true;
	if (manualRequest || (uint32_t)(now - lastPresenceTick) >=
		EXCHANGE_POLL_INTERVAL_MS)
	{
		refreshTapeheadPresence();
		lastPresenceTick = now;
	}
	if (tapeheadConfig.tapeSisterExchangePath[0] == '\0')
	{
		if (manualScanRequested)
		{
			okBox(0, "TapeSister Inbox", "Configure [TapeSister] ExchangePath in tapehead.ini first.", NULL);
			manualScanRequested = false;
		}
		return;
	}

	if (finishInboxScanIfReady())
		return;
	if (inboxScanThread != NULL)
		return;

	const bool startManualScan = manualScanRequested;
	if (!startManualScan)
	{
		if ((uint32_t)(now - lastPollTick) < EXCHANGE_POLL_INTERVAL_MS ||
			!inboxCanShowResult())
		{
			return;
		}
		lastPollTick = now;
	}
	else if (!inboxCanShowResult())
	{
		return;
	}

	if (!startInboxScan(startManualScan))
	{
		if (startManualScan)
		{
			okBox(0, "TapeSister Inbox",
				"Could not start the TapeSister inbox scan.", NULL);
			manualScanRequested = false;
		}
		return;
	}
	if (startManualScan)
		manualScanRequested = false;
}

void tapeSisterExchangeOpenMenu(void)
{
	const int16_t choice = okBox(SYSREQ_TYPE_TAPESISTER_MENU,
		"TapeSister Exchange",
		"Send samples, check the shared inbox, or open the exchange folder.", NULL);
	if (choice == 3)
	{
		tapeSisterExchangePoll(true);
		return;
	}
	if (choice == 4)
	{
		if (tapeheadConfig.tapeSisterExchangePath[0] == '\0')
			okBox(0, "TapeSister Exchange", "Configure the exchange path first.", NULL);
		else if (!openTapeSisterExchangeFolder())
			okBox(0, "TapeSister Exchange", "The configured exchange folder could not be opened.", NULL);
		return;
	}
	if (choice != 1 && choice != 2)
		return;
	if (tapeheadConfig.tapeSisterExchangePath[0] == '\0')
	{
		okBox(0, "Send to TapeSister", "Configure [TapeSister] ExchangePath in tapehead.ini first.", NULL);
		return;
	}
	exchangeSource_t source;
	const bool collected = choice == 1 ? collectCurrentInstrument(&source) :
		collectInstrumentRange(&source);
	if (!collected)
	{
		exchangeSourceFree(&source);
		okBox(0, "Send to TapeSister",
			choice == 1 ? "The current instrument has no populated samples." :
			"No occupied instruments were found from the current instrument onward.",
			NULL);
		return;
	}
	confirmAndPublish(&source);
	exchangeSourceFree(&source);
}
