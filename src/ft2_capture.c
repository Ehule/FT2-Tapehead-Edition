#include "ft2_capture.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wchar.h>
#include <wctype.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "ft2_config.h"
#include "ft2_audio.h"
#include "ft2_header.h"
#include "ft2_structs.h"
#include "ft2_sysreqs.h"
#include "ft2_unicode.h"
#include "ft2_video.h"
#include "ft2_wav_renderer.h"

#define CAPTURE_PATH_CAPACITY (TAPEHEAD_CONFIG_PATH_CAPACITY + 512)
#define CAPTURE_FILENAME_CAPACITY (TAPEHEAD_RENDER_FILENAME_CAPACITY + 64)
#define PERFORMANCE_RING_FRAMES (1U << 20)
#define PERFORMANCE_RING_MASK (PERFORMANCE_RING_FRAMES - 1)
#define PERFORMANCE_WRITE_CHUNK_FRAMES 4096

enum
{
	PERFORMANCE_CAPTURE_IDLE = 0,
	PERFORMANCE_CAPTURE_PREPARING,
	PERFORMANCE_CAPTURE_ARMED,
	PERFORMANCE_CAPTURE_RECORDING,
	PERFORMANCE_CAPTURE_STOP_PENDING,
	PERFORMANCE_CAPTURE_FINISHING,
	PERFORMANCE_CAPTURE_ABORTING,
	PERFORMANCE_CAPTURE_FINISHED
};

typedef struct tapeheadCaptureJob_t
{
	tapeheadRenderPlan_t plan;
	tapeheadBlockLoopSpec_t blockSpec;
	bool hasBlock, resumeBlockLoop, quietSuccess, success;
	uint64_t renderedFrames;
	UNICHAR path[CAPTURE_PATH_CAPACITY];
	char filename[CAPTURE_FILENAME_CAPACITY];
	SDL_atomic_t finished;
} tapeheadCaptureJob_t;

typedef struct tapeheadPerformanceCaptureJob_t
{
	float *ring;
	FILE *file;
	SDL_Thread *thread;
	uint32_t sampleRate;
	uint8_t bitDepth;
	bool userDisarmed;
	uint64_t renderedFrames;
	UNICHAR path[CAPTURE_PATH_CAPACITY];
	UNICHAR partialPath[CAPTURE_PATH_CAPACITY];
	char filename[CAPTURE_FILENAME_CAPACITY];
	SDL_atomic_t state, readPosition, writePosition, finished, startedEvent;
	SDL_atomic_t success, overflow, writeError;
} tapeheadPerformanceCaptureJob_t;

static tapeheadCaptureJob_t *captureJob;
static tapeheadPerformanceCaptureJob_t performanceCapture;

static UNICHAR *pathFromUtf8(const char *path)
{
	if (path == NULL)
		return NULL;
#ifdef _WIN32
	int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		path, -1, NULL, 0);
	if (length <= 0)
		length = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
	if (length <= 0)
		return NULL;
	UNICHAR *result = malloc((size_t)(length + 8) * sizeof (UNICHAR));
	if (result == NULL)
		return NULL;
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, result,
		length) <= 0 && MultiByteToWideChar(CP_ACP, 0, path, -1, result,
		length) <= 0)
	{
		free(result);
		return NULL;
	}
	const size_t plainLength = wcslen(result);
	const bool driveAbsolute = plainLength >= 3 && iswalpha(result[0]) &&
		result[1] == L':' && (result[2] == L'\\' || result[2] == L'/');
	const bool uncAbsolute = plainLength >= 2 && result[0] == L'\\' &&
		result[1] == L'\\';
	if (plainLength >= MAX_PATH - 1 && (driveAbsolute || uncAbsolute) &&
		wcsncmp(result, L"\\\\?\\", 4) != 0)
	{
		UNICHAR *extended = malloc((plainLength + 9) * sizeof (UNICHAR));
		if (extended == NULL)
		{
			free(result);
			return NULL;
		}
		if (uncAbsolute)
			swprintf(extended, plainLength + 9, L"\\\\?\\UNC\\%ls", result + 2);
		else
			swprintf(extended, plainLength + 9, L"\\\\?\\%ls", result);
		free(result);
		return extended;
	}
	return result;
#else
	const size_t length = strlen(path) + 1;
	UNICHAR *result = malloc(length);
	if (result != NULL)
		memcpy(result, path, length);
	return result;
#endif
}

static bool pathIsDirectory(const UNICHAR *path)
{
#ifdef _WIN32
	const DWORD attributes = GetFileAttributesW(path);
	return attributes != INVALID_FILE_ATTRIBUTES &&
		(attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	struct stat status;
	return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
#endif
}

static bool pathExists(const UNICHAR *path)
{
#ifdef _WIN32
	return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
#else
	return access(path, F_OK) == 0;
#endif
}

static bool makeDirectory(const UNICHAR *path)
{
#ifdef _WIN32
	return _wmkdir(path) == 0 || errno == EEXIST;
#else
	return mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0 ||
		errno == EEXIST;
#endif
}

static bool joinPath(UNICHAR *destination, size_t capacity,
	const UNICHAR *parent, const UNICHAR *child)
{
	const size_t parentLength = UNICHAR_STRLEN(parent);
	const size_t childLength = UNICHAR_STRLEN(child);
	const bool hasSeparator = parentLength > 0 &&
#ifdef _WIN32
		(parent[parentLength-1] == L'\\' || parent[parentLength-1] == L'/');
#else
		parent[parentLength-1] == '/';
#endif
	if (parentLength + (hasSeparator ? 0 : 1) + childLength + 1 > capacity)
		return false;
	UNICHAR_STRCPY(destination, parent);
	if (!hasSeparator)
#ifdef _WIN32
		UNICHAR_STRCAT(destination, L"\\");
#else
		UNICHAR_STRCAT(destination, "/");
#endif
	UNICHAR_STRCAT(destination, child);
	return true;
}

static bool defaultCaptureDirectory(UNICHAR directory[CAPTURE_PATH_CAPACITY])
{
	if (tapeheadConfig.captureFolder[0] != '\0')
	{
		UNICHAR *configured = pathFromUtf8(tapeheadConfig.captureFolder);
		if (configured == NULL ||
			UNICHAR_STRLEN(configured) + 1 > CAPTURE_PATH_CAPACITY)
		{
			free(configured);
			return false;
		}
		UNICHAR_STRCPY(directory, configured);
		free(configured);
	}
	else
	{
		if (editor.configFileLocationU == NULL ||
			UNICHAR_STRLEN(editor.configFileLocationU) + 1 > CAPTURE_PATH_CAPACITY)
		{
			return false;
		}
		UNICHAR_STRCPY(directory, editor.configFileLocationU);
#ifdef _WIN32
		UNICHAR *separator = wcsrchr(directory, L'\\');
		UNICHAR *slash = wcsrchr(directory, L'/');
		if (slash != NULL && (separator == NULL || slash > separator))
			separator = slash;
#else
		UNICHAR *separator = strrchr(directory, '/');
#endif
		if (separator == NULL)
			return false;
		*separator = 0;
#ifdef _WIN32
		static const UNICHAR capturesName[] = L"Captures";
#else
		static const UNICHAR capturesName[] = "Captures";
#endif
		UNICHAR parent[CAPTURE_PATH_CAPACITY];
		UNICHAR_STRCPY(parent, directory);
		if (!joinPath(directory, CAPTURE_PATH_CAPACITY, parent, capturesName))
			return false;
	}

	if (pathIsDirectory(directory))
		return true;
	if (pathExists(directory))
		return false;
	return makeDirectory(directory) && pathIsDirectory(directory);
}

static void safeSongName(char destination[40])
{
	const char *source = song.name[0] != '\0' ? song.name : "Untitled";
	size_t output = 0;
	for (size_t i = 0; source[i] != '\0' && output < 39; i++)
	{
		const unsigned char character = (unsigned char)source[i];
		if (isalnum(character))
			destination[output++] = (char)character;
		else if (output > 0 && destination[output-1] != '_')
			destination[output++] = '_';
	}
	while (output > 0 && destination[output-1] == '_')
		output--;
	if (output == 0)
	{
		memcpy(destination, "Untitled", 8);
		output = 8;
	}
	destination[output] = '\0';
}

static bool createUniqueCapturePathForName(const char *renderFilename,
	char filename[CAPTURE_FILENAME_CAPACITY],
	UNICHAR path[CAPTURE_PATH_CAPACITY])
{
	if (renderFilename == NULL || renderFilename[0] == '\0')
		return false;

	UNICHAR directory[CAPTURE_PATH_CAPACITY];
	if (!defaultCaptureDirectory(directory))
		return false;

	char songPrefix[40], stem[CAPTURE_FILENAME_CAPACITY - 16];
	safeSongName(songPrefix);
	snprintf(stem, sizeof (stem), "%s_%s", songPrefix, renderFilename);
	char *extension = strrchr(stem, '.');
	if (extension != NULL && !_stricmp(extension, ".wav"))
		*extension = '\0';

	for (uint32_t number = 1; number <= 999999; number++)
	{
		snprintf(filename, CAPTURE_FILENAME_CAPACITY, "%s_%03u.wav", stem,
			number);
		UNICHAR *filenameU = pathFromUtf8(filename);
		const bool joined = filenameU != NULL && joinPath(path,
			CAPTURE_PATH_CAPACITY, directory, filenameU);
		free(filenameU);
		if (!joined)
			return false;
		if (!pathExists(path))
			return true;
	}
	return false;
}

static bool createUniqueCapturePath(tapeheadCaptureJob_t *job)
{
	return createUniqueCapturePathForName(job->plan.filename, job->filename,
		job->path);
}

static void putU16LE(uint8_t *destination, uint16_t value)
{
	destination[0] = (uint8_t)value;
	destination[1] = (uint8_t)(value >> 8);
}

static void putU32LE(uint8_t *destination, uint32_t value)
{
	destination[0] = (uint8_t)value;
	destination[1] = (uint8_t)(value >> 8);
	destination[2] = (uint8_t)(value >> 16);
	destination[3] = (uint8_t)(value >> 24);
}

static bool writePerformanceWavHeader(FILE *file, uint32_t sampleRate,
	uint8_t bitDepth, uint64_t frames)
{
	if (file == NULL || sampleRate == 0 || (bitDepth != 16 && bitDepth != 32))
		return false;

	const uint32_t bytesPerFrame = (bitDepth / 8) * 2;
	if (frames > (UINT32_MAX - 36U) / bytesPerFrame)
		return false;

	const uint32_t dataBytes = (uint32_t)frames * bytesPerFrame;
	uint8_t header[44] = { 0 };
	memcpy(&header[0], "RIFF", 4);
	putU32LE(&header[4], 36U + dataBytes);
	memcpy(&header[8], "WAVEfmt ", 8);
	putU32LE(&header[16], 16);
	putU16LE(&header[20], bitDepth == 16 ? 1 : 3);
	putU16LE(&header[22], 2);
	putU32LE(&header[24], sampleRate);
	putU32LE(&header[28], sampleRate * bytesPerFrame);
	putU16LE(&header[32], (uint16_t)bytesPerFrame);
	putU16LE(&header[34], bitDepth);
	memcpy(&header[36], "data", 4);
	putU32LE(&header[40], dataBytes);

	if (fseek(file, 0, SEEK_SET) != 0 ||
		fwrite(header, 1, sizeof (header), file) != sizeof (header))
	{
		return false;
	}
	return !ferror(file);
}

#ifdef TAPEHEAD_CAPTURE_TEST
bool tapeheadTestWritePerformanceWavHeader(FILE *file, uint32_t sampleRate,
	uint8_t bitDepth, uint64_t frames)
{
	return writePerformanceWavHeader(file, sampleRate, bitDepth, frames);
}
#endif

static bool makePerformancePartialPath(void)
{
	const size_t length = UNICHAR_STRLEN(performanceCapture.path);
#ifdef _WIN32
	static const UNICHAR suffix[] = L".partial";
#else
	static const UNICHAR suffix[] = ".partial";
#endif
	if (length + (sizeof (suffix) / sizeof (suffix[0])) >
		CAPTURE_PATH_CAPACITY)
	{
		return false;
	}
	UNICHAR_STRCPY(performanceCapture.partialPath, performanceCapture.path);
	UNICHAR_STRCAT(performanceCapture.partialPath, suffix);
	return true;
}

static uint32_t performanceRingAvailable(void)
{
	const uint32_t readPosition =
		(uint32_t)SDL_AtomicGet(&performanceCapture.readPosition);
	const uint32_t writePosition =
		(uint32_t)SDL_AtomicGet(&performanceCapture.writePosition);
	return (writePosition - readPosition) & PERFORMANCE_RING_MASK;
}

static bool writePerformanceFrames(const float *samples, uint32_t frames)
{
	if (frames == 0)
		return true;

	const uint32_t bytesPerFrame = (performanceCapture.bitDepth / 8) * 2;
	if (performanceCapture.renderedFrames + frames >
		(UINT32_MAX - 36U) / bytesPerFrame)
	{
		SDL_AtomicSet(&performanceCapture.overflow, true);
		return false;
	}

	if (performanceCapture.bitDepth == 32)
	{
		if (fwrite(samples, sizeof (float), (size_t)frames * 2,
			performanceCapture.file) != (size_t)frames * 2)
		{
			return false;
		}
	}
	else
	{
		int16_t converted[PERFORMANCE_WRITE_CHUNK_FRAMES * 2];
		for (uint32_t i = 0; i < frames * 2; i++)
		{
			const float value = CLAMP(samples[i], -1.0f, 1.0f) * 32767.0f;
			converted[i] = (int16_t)(value >= 0.0f ? value + 0.5f : value - 0.5f);
		}
		if (fwrite(converted, sizeof (int16_t), (size_t)frames * 2,
			performanceCapture.file) != (size_t)frames * 2)
		{
			return false;
		}
	}

	performanceCapture.renderedFrames += frames;
	return !ferror(performanceCapture.file);
}

static int32_t performanceCaptureWriter(void *userdata)
{
	(void)userdata;
	bool fileOK = true;

	for (;;)
	{
		const int32_t state = SDL_AtomicGet(&performanceCapture.state);
		if (state == PERFORMANCE_CAPTURE_ABORTING)
		{
			fileOK = false;
			break;
		}

		const uint32_t available = performanceRingAvailable();
		if (available > 0)
		{
			const uint32_t readPosition =
				(uint32_t)SDL_AtomicGet(&performanceCapture.readPosition);
			uint32_t frames = MIN(available, PERFORMANCE_WRITE_CHUNK_FRAMES);
			frames = MIN(frames, PERFORMANCE_RING_FRAMES - readPosition);
			if (!writePerformanceFrames(
				&performanceCapture.ring[readPosition * 2], frames))
			{
				SDL_AtomicSet(&performanceCapture.writeError,
					!SDL_AtomicGet(&performanceCapture.overflow));
				SDL_AtomicSet(&performanceCapture.state,
					PERFORMANCE_CAPTURE_ABORTING);
				fileOK = false;
				break;
			}
			SDL_AtomicSet(&performanceCapture.readPosition,
				(int)((readPosition + frames) & PERFORMANCE_RING_MASK));
			continue;
		}

		if (state == PERFORMANCE_CAPTURE_FINISHING)
			break;
		SDL_Delay(2);
	}

	if (fileOK && performanceCapture.renderedFrames > 0)
	{
		fileOK = writePerformanceWavHeader(performanceCapture.file,
			performanceCapture.sampleRate, performanceCapture.bitDepth,
			performanceCapture.renderedFrames);
	}
	else
	{
		fileOK = false;
	}

	if (performanceCapture.file != NULL)
	{
		if (fclose(performanceCapture.file) != 0)
			fileOK = false;
		performanceCapture.file = NULL;
	}

	if (fileOK)
	{
		if (UNICHAR_RENAME(performanceCapture.partialPath,
			performanceCapture.path) != 0)
		{
			fileOK = false;
		}
	}
	if (!fileOK)
		UNICHAR_REMOVE(performanceCapture.partialPath);

	SDL_AtomicSet(&performanceCapture.success, fileOK);
	SDL_AtomicSet(&performanceCapture.state, PERFORMANCE_CAPTURE_FINISHED);
	SDL_AtomicSet(&performanceCapture.finished, true);
	return 0;
}

static bool startPerformanceCapture(void)
{
	if (captureJob != NULL || editor.wavIsRendering ||
		SDL_AtomicGet(&performanceCapture.state) != PERFORMANCE_CAPTURE_IDLE ||
		!tapeheadBlockLoopIsActive() || tapeheadBlockLoopIsOffline() ||
		!songPlaying || audio.freq == 0)
	{
		return false;
	}

	memset(&performanceCapture, 0, sizeof (performanceCapture));
	SDL_AtomicSet(&performanceCapture.state, PERFORMANCE_CAPTURE_PREPARING);
	performanceCapture.sampleRate = audio.freq;
	performanceCapture.bitDepth = getWavRenderBitDepth();
	if (performanceCapture.bitDepth != 16 && performanceCapture.bitDepth != 32)
		performanceCapture.bitDepth = 32;

	performanceCapture.ring = malloc(
		(size_t)PERFORMANCE_RING_FRAMES * 2 * sizeof (float));
	if (performanceCapture.ring == NULL ||
		!createUniqueCapturePathForName("BlockPerformance.wav",
			performanceCapture.filename, performanceCapture.path) ||
		!makePerformancePartialPath())
	{
		free(performanceCapture.ring);
		memset(&performanceCapture, 0, sizeof (performanceCapture));
		return false;
	}

	performanceCapture.file = UNICHAR_FOPEN(performanceCapture.partialPath, "wb");
	if (performanceCapture.file == NULL ||
		!writePerformanceWavHeader(performanceCapture.file,
			performanceCapture.sampleRate, performanceCapture.bitDepth, 0))
	{
		if (performanceCapture.file != NULL)
			fclose(performanceCapture.file);
		UNICHAR_REMOVE(performanceCapture.partialPath);
		free(performanceCapture.ring);
		memset(&performanceCapture, 0, sizeof (performanceCapture));
		return false;
	}

	performanceCapture.thread = SDL_CreateThread(performanceCaptureWriter,
		"Block performance capture", NULL);
	if (performanceCapture.thread == NULL)
	{
		fclose(performanceCapture.file);
		performanceCapture.file = NULL;
		UNICHAR_REMOVE(performanceCapture.partialPath);
		free(performanceCapture.ring);
		memset(&performanceCapture, 0, sizeof (performanceCapture));
		return false;
	}

	SDL_AtomicSet(&performanceCapture.state, PERFORMANCE_CAPTURE_ARMED);
	return true;
}

bool tapeheadPerformanceCaptureIsBusy(void)
{
	return SDL_AtomicGet(&performanceCapture.state) != PERFORMANCE_CAPTURE_IDLE;
}

tapeheadPerformanceCaptureToggleResult_t tapeheadPerformanceCaptureToggle(void)
{
	int32_t state = SDL_AtomicGet(&performanceCapture.state);
	if (state == PERFORMANCE_CAPTURE_IDLE)
	{
		return startPerformanceCapture()
			? TAPEHEAD_PERFORMANCE_CAPTURE_ARMED
			: TAPEHEAD_PERFORMANCE_CAPTURE_FAILED;
	}
	if (state == PERFORMANCE_CAPTURE_ARMED)
	{
		performanceCapture.userDisarmed = true;
		if (SDL_AtomicCAS(&performanceCapture.state, PERFORMANCE_CAPTURE_ARMED,
			PERFORMANCE_CAPTURE_ABORTING))
		{
			return TAPEHEAD_PERFORMANCE_CAPTURE_DISARMED;
		}
		performanceCapture.userDisarmed = false;
		state = SDL_AtomicGet(&performanceCapture.state);
	}
	if (state == PERFORMANCE_CAPTURE_RECORDING)
	{
		if (SDL_AtomicCAS(&performanceCapture.state,
			PERFORMANCE_CAPTURE_RECORDING,
			PERFORMANCE_CAPTURE_STOP_PENDING))
		{
			return TAPEHEAD_PERFORMANCE_CAPTURE_STOPPING;
		}
	}
	if (state == PERFORMANCE_CAPTURE_STOP_PENDING ||
		state == PERFORMANCE_CAPTURE_FINISHING ||
		state == PERFORMANCE_CAPTURE_FINISHED)
	{
		return TAPEHEAD_PERFORMANCE_CAPTURE_ALREADY_STOPPING;
	}
	return TAPEHEAD_PERFORMANCE_CAPTURE_FAILED;
}

void tapeheadPerformanceCaptureFeed(const float *left, const float *right,
	uint32_t offset, uint32_t frames, float normalizeMultiplier,
	bool blockSeam)
{
	int32_t state = SDL_AtomicGet(&performanceCapture.state);
	if ((state == PERFORMANCE_CAPTURE_RECORDING ||
		state == PERFORMANCE_CAPTURE_STOP_PENDING) && frames > 0 &&
		left != NULL && right != NULL)
	{
		const uint32_t readPosition =
			(uint32_t)SDL_AtomicGet(&performanceCapture.readPosition);
		const uint32_t writePosition =
			(uint32_t)SDL_AtomicGet(&performanceCapture.writePosition);
		const uint32_t used =
			(writePosition - readPosition) & PERFORMANCE_RING_MASK;
		const uint32_t freeFrames = PERFORMANCE_RING_MASK - used;
		if (frames > freeFrames)
		{
			SDL_AtomicSet(&performanceCapture.overflow, true);
			SDL_AtomicSet(&performanceCapture.state,
				PERFORMANCE_CAPTURE_ABORTING);
			return;
		}

		for (uint32_t i = 0; i < frames; i++)
		{
			const uint32_t destination =
				((writePosition + i) & PERFORMANCE_RING_MASK) * 2;
			performanceCapture.ring[destination] = CLAMP(
				left[offset + i] * normalizeMultiplier, -1.0f, 1.0f);
			performanceCapture.ring[destination + 1] = CLAMP(
				right[offset + i] * normalizeMultiplier, -1.0f, 1.0f);
		}
		SDL_AtomicSet(&performanceCapture.writePosition,
			(int)((writePosition + frames) & PERFORMANCE_RING_MASK));
	}

	if (!blockSeam)
		return;

	state = SDL_AtomicGet(&performanceCapture.state);
	if (state == PERFORMANCE_CAPTURE_ARMED &&
		SDL_AtomicCAS(&performanceCapture.state, PERFORMANCE_CAPTURE_ARMED,
			PERFORMANCE_CAPTURE_RECORDING))
	{
		SDL_AtomicSet(&performanceCapture.startedEvent, true);
	}
	else if (state == PERFORMANCE_CAPTURE_STOP_PENDING)
	{
		(void)SDL_AtomicCAS(&performanceCapture.state,
			PERFORMANCE_CAPTURE_STOP_PENDING, PERFORMANCE_CAPTURE_FINISHING);
	}
}

void tapeheadPerformanceCaptureAudioStopped(void)
{
	const int32_t state = SDL_AtomicGet(&performanceCapture.state);
	if (state == PERFORMANCE_CAPTURE_ARMED ||
		state == PERFORMANCE_CAPTURE_PREPARING)
	{
		(void)SDL_AtomicCAS(&performanceCapture.state, state,
			PERFORMANCE_CAPTURE_ABORTING);
	}
	else if (state == PERFORMANCE_CAPTURE_RECORDING ||
		state == PERFORMANCE_CAPTURE_STOP_PENDING)
	{
		(void)SDL_AtomicCAS(&performanceCapture.state, state,
			PERFORMANCE_CAPTURE_FINISHING);
	}
}

#ifdef TAPEHEAD_CAPTURE_TEST
bool tapeheadTestPerformanceCaptureFeedState(void)
{
	float *ring = calloc((size_t)PERFORMANCE_RING_FRAMES * 2, sizeof (float));
	if (ring == NULL)
		return false;

	memset(&performanceCapture, 0, sizeof (performanceCapture));
	performanceCapture.ring = ring;
	SDL_AtomicSet(&performanceCapture.state, PERFORMANCE_CAPTURE_ARMED);

	const float left[] = { -2.0f, -1.0f, 0.5f, 2.0f };
	const float right[] = { 2.0f, 1.0f, -0.5f, -2.0f };
	tapeheadPerformanceCaptureFeed(left, right, 0, 4, 0.5f, true);
	bool ok = SDL_AtomicGet(&performanceCapture.state) ==
		PERFORMANCE_CAPTURE_RECORDING &&
		SDL_AtomicGet(&performanceCapture.writePosition) == 0 &&
		SDL_AtomicGet(&performanceCapture.startedEvent);

	tapeheadPerformanceCaptureFeed(left, right, 0, 4, 0.5f, false);
	ok = ok && SDL_AtomicGet(&performanceCapture.writePosition) == 4 &&
		ring[0] == -1.0f && ring[1] == 1.0f &&
		ring[2] == -0.5f && ring[3] == 0.5f &&
		ring[4] == 0.25f && ring[5] == -0.25f &&
		ring[6] == 1.0f && ring[7] == -1.0f;

	SDL_AtomicSet(&performanceCapture.state, PERFORMANCE_CAPTURE_STOP_PENDING);
	tapeheadPerformanceCaptureFeed(left, right, 1, 2, 1.0f, true);
	ok = ok && SDL_AtomicGet(&performanceCapture.writePosition) == 6 &&
		SDL_AtomicGet(&performanceCapture.state) == PERFORMANCE_CAPTURE_FINISHING;

	SDL_AtomicSet(&performanceCapture.readPosition, 1);
	SDL_AtomicSet(&performanceCapture.writePosition, 0);
	SDL_AtomicSet(&performanceCapture.state, PERFORMANCE_CAPTURE_RECORDING);
	tapeheadPerformanceCaptureFeed(left, right, 0, 1, 1.0f, false);
	ok = ok && SDL_AtomicGet(&performanceCapture.state) ==
		PERFORMANCE_CAPTURE_ABORTING &&
		SDL_AtomicGet(&performanceCapture.overflow);

	free(ring);
	memset(&performanceCapture, 0, sizeof (performanceCapture));
	return ok;
}
#endif

static void captureCompleted(bool success, uint64_t renderedFrames,
	void *userdata)
{
	tapeheadCaptureJob_t *job = (tapeheadCaptureJob_t *)userdata;
	job->success = success;
	job->renderedFrames = renderedFrames;
	SDL_AtomicSet(&job->finished, true);
}

bool tapeheadCaptureRender(const tapeheadRenderPlan_t *plan,
	const tapeheadBlockLoopSpec_t *blockSpec, bool resumeBlockLoop,
	bool quietSuccess)
{
	if (plan == NULL || captureJob != NULL || editor.wavIsRendering ||
		tapeheadPerformanceCaptureIsBusy() ||
		(plan->scope == TAPEHEAD_RENDER_BLOCK && blockSpec == NULL))
	{
		return false;
	}

	tapeheadCaptureJob_t *job = calloc(1, sizeof (*job));
	if (job == NULL)
		return false;
	job->plan = *plan;
	job->hasBlock = blockSpec != NULL;
	if (blockSpec != NULL)
		job->blockSpec = *blockSpec;
	job->resumeBlockLoop = resumeBlockLoop;
	job->quietSuccess = quietSuccess;
	if (!createUniqueCapturePath(job))
	{
		free(job);
		return false;
	}

	FILE *file = UNICHAR_FOPEN(job->path, "wb");
	if (file == NULL)
	{
		free(job);
		return false;
	}
	captureJob = job;
	const bool started = job->hasBlock ?
		startWavBlockRenderToFile(file, &job->blockSpec, captureCompleted, job) :
		startWavRenderToFile(file, job->plan.startOrder, job->plan.stopOrder,
			job->plan.soloChannel, captureCompleted, job);
	if (!started)
	{
		captureJob = NULL;
		fclose(file);
		UNICHAR_REMOVE(job->path);
		free(job);
		return false;
	}
	return true;
}

bool tapeheadCaptureQuickBlock(void)
{
	tapeheadBlockLoopSpec_t spec;
	if (!tapeheadBlockLoopGetSelection(&spec))
		return false;
	tapeheadRenderPlan_t plan;
	if (!tapeheadRenderBlockPlanInit(&plan, &spec, (uint16_t)song.numChannels,
		getWavRenderFrequency(), getWavRenderBitDepth()))
	{
		return false;
	}
	return tapeheadCaptureRender(&plan, &spec, true, true);
}

void tapeheadCapturePoll(void)
{
	int32_t performanceState = SDL_AtomicGet(&performanceCapture.state);
	if (performanceState != PERFORMANCE_CAPTURE_IDLE &&
		performanceState != PERFORMANCE_CAPTURE_FINISHED &&
		!tapeheadBlockLoopIsActive())
	{
		/*
		** Stop/cancel outside the audio callback only while it is locked. This
		** prevents the writer from closing its file while a callback is still
		** publishing the last post-mixer frames into the ring.
		*/
		const bool audioWasLocked = audio.locked;
		if (!audioWasLocked)
			lockAudio();
		performanceState = SDL_AtomicGet(&performanceCapture.state);
		if (performanceState == PERFORMANCE_CAPTURE_ARMED ||
			performanceState == PERFORMANCE_CAPTURE_PREPARING)
		{
			(void)SDL_AtomicCAS(&performanceCapture.state, performanceState,
				PERFORMANCE_CAPTURE_ABORTING);
		}
		else if (performanceState == PERFORMANCE_CAPTURE_RECORDING ||
			performanceState == PERFORMANCE_CAPTURE_STOP_PENDING)
		{
			(void)SDL_AtomicCAS(&performanceCapture.state, performanceState,
				PERFORMANCE_CAPTURE_FINISHING);
		}
		if (!audioWasLocked)
			unlockAudio();
	}

	if (SDL_AtomicCAS(&performanceCapture.startedEvent, true, false) &&
		!SDL_AtomicGet(&performanceCapture.finished))
	{
		showRecPlusOverlay("PERFORMANCE CAPTURE");
	}

	if (SDL_AtomicGet(&performanceCapture.finished))
	{
		if (performanceCapture.thread != NULL)
		{
			SDL_WaitThread(performanceCapture.thread, NULL);
			performanceCapture.thread = NULL;
		}

		const bool success = SDL_AtomicGet(&performanceCapture.success) != 0;
		const bool overflow = SDL_AtomicGet(&performanceCapture.overflow) != 0;
		const bool writeError =
			SDL_AtomicGet(&performanceCapture.writeError) != 0;
		const bool userDisarmed = performanceCapture.userDisarmed;
		free(performanceCapture.ring);
		memset(&performanceCapture, 0, sizeof (performanceCapture));

		if (success)
			showRecPlusOverlay("PERFORMANCE SAVED");
		else if (!userDisarmed)
		{
			if (overflow)
			{
				okBox(0, "Performance capture",
					"The live capture buffer overflowed. No capture was kept.",
					NULL);
			}
			else if (writeError)
			{
				okBox(0, "Performance capture",
					"The capture could not be written. No capture was kept.",
					NULL);
			}
			else
			{
				okBox(0, "Performance capture",
					"The performance capture stopped before audio was recorded. No capture was kept.",
					NULL);
			}
		}
	}

	tapeheadCaptureJob_t *job = captureJob;
	if (job == NULL || !SDL_AtomicGet(&job->finished))
		return;
	captureJob = NULL;
	if (!job->success)
		UNICHAR_REMOVE(job->path);
	if (job->resumeBlockLoop)
		(void)tapeheadBlockLoopStart(&job->blockSpec);

	if (job->success)
	{
		if (job->quietSuccess)
			showRecPlusOverlay("CAPTURE SAVED");
		else
		{
			char message[256];
			snprintf(message, sizeof (message),
				"Saved %s in the Captures folder.", job->filename);
			okBox(0, "Render audio", message, NULL);
		}
	}
	else
	{
		okBox(0, "Render audio",
			"The WAV render was cancelled or failed. No capture was kept.", NULL);
	}
	free(job);
}

void tapeheadCaptureShutdown(void)
{
	if (SDL_AtomicGet(&performanceCapture.state) == PERFORMANCE_CAPTURE_IDLE)
		return;

	/* closeAudio() has stopped the device before this is called. */
	tapeheadPerformanceCaptureAudioStopped();
	if (performanceCapture.thread != NULL)
	{
		SDL_WaitThread(performanceCapture.thread, NULL);
		performanceCapture.thread = NULL;
	}
	if (!SDL_AtomicGet(&performanceCapture.success))
		UNICHAR_REMOVE(performanceCapture.partialPath);
	free(performanceCapture.ring);
	memset(&performanceCapture, 0, sizeof (performanceCapture));
}
