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
#include "ft2_header.h"
#include "ft2_structs.h"
#include "ft2_sysreqs.h"
#include "ft2_unicode.h"
#include "ft2_video.h"
#include "ft2_wav_renderer.h"

#define CAPTURE_PATH_CAPACITY (TAPEHEAD_CONFIG_PATH_CAPACITY + 512)

typedef struct tapeheadCaptureJob_t
{
	tapeheadRenderPlan_t plan;
	tapeheadBlockLoopSpec_t blockSpec;
	bool hasBlock, resumeBlockLoop, quietSuccess, success;
	uint64_t renderedFrames;
	UNICHAR path[CAPTURE_PATH_CAPACITY];
	char filename[TAPEHEAD_RENDER_FILENAME_CAPACITY + 64];
	SDL_atomic_t finished;
} tapeheadCaptureJob_t;

static tapeheadCaptureJob_t *captureJob;

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

static bool createUniqueCapturePath(tapeheadCaptureJob_t *job)
{
	UNICHAR directory[CAPTURE_PATH_CAPACITY];
	if (!defaultCaptureDirectory(directory))
		return false;

	char songPrefix[40], stem[TAPEHEAD_RENDER_FILENAME_CAPACITY + 64];
	safeSongName(songPrefix);
	snprintf(stem, sizeof (stem), "%s_%s", songPrefix, job->plan.filename);
	char *extension = strrchr(stem, '.');
	if (extension != NULL && !_stricmp(extension, ".wav"))
		*extension = '\0';

	for (uint32_t number = 1; number <= 999999; number++)
	{
		snprintf(job->filename, sizeof (job->filename), "%s_%03u.wav", stem,
			number);
		UNICHAR *filename = pathFromUtf8(job->filename);
		const bool joined = filename != NULL && joinPath(job->path,
			CAPTURE_PATH_CAPACITY, directory, filename);
		free(filename);
		if (!joined)
			return false;
		if (!pathExists(job->path))
			return true;
	}
	return false;
}

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
