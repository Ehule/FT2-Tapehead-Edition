#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "ft2_header.h"
#include "ft2_diskop.h"
#include "ft2_sample_loader.h"
#include "ft2_sample_matrix_editor.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct sampleMatrixBrowserEntry_t
{
	UNICHAR *nameU;
	char *displayName;
	bool directory, selected;
} sampleMatrixBrowserEntry_t;

static sampleMatrixBrowserEntry_t *entries;
static uint32_t entryCount, scrollOffset;
static int32_t selectionAnchor = -1;
static UNICHAR currentPath[PATH_MAX + 1];
static char displayPath[PATH_MAX + 1];
static bool pathInitialized;

static void freeEntries(void)
{
	for (uint32_t i = 0; i < entryCount; i++)
	{
		free(entries[i].nameU);
		free(entries[i].displayName);
	}
	free(entries);
	entries = NULL;
	entryCount = scrollOffset = 0;
	selectionAnchor = -1;
}

static bool extensionAccepted(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (dot == NULL || dot[1] == '\0')
		return false;
	for (uint32_t i = 0; ; i++)
	{
		const char *extension = supportedSmpExtensions[i];
		if (!_stricmp(extension, "END_OF_LIST"))
			return false;
		if (!_stricmp(extension, dot + 1))
			return true;
	}
}

static int naturalCompareText(const unsigned char *a, const unsigned char *b)
{
	while (*a != '\0' && *b != '\0')
	{
		if (isdigit(*a) && isdigit(*b))
		{
			while (*a == '0') a++;
			while (*b == '0') b++;
			const unsigned char *aDigits = a, *bDigits = b;
			while (isdigit(*a)) a++;
			while (isdigit(*b)) b++;
			const size_t aLength = (size_t)(a - aDigits);
			const size_t bLength = (size_t)(b - bDigits);
			if (aLength != bLength)
				return aLength < bLength ? -1 : 1;
			const int comparison = memcmp(aDigits, bDigits, aLength);
			if (comparison != 0)
				return comparison;
			continue;
		}
		const int ac = tolower(*a++), bc = tolower(*b++);
		if (ac != bc)
			return ac < bc ? -1 : 1;
	}
	return *a == *b ? 0 : (*a == '\0' ? -1 : 1);
}

static int entryCompare(const void *left, const void *right)
{
	const sampleMatrixBrowserEntry_t *a = left, *b = right;
	if (a->directory != b->directory)
		return a->directory ? -1 : 1;
	return naturalCompareText((const unsigned char *)a->displayName,
		(const unsigned char *)b->displayName);
}

static bool appendEntry(const UNICHAR *nameU, bool directory)
{
	char *displayName = unicharToCp850((UNICHAR *)nameU, true);
	if (displayName == NULL)
		return false;
	if (!directory && !extensionAccepted(displayName))
	{
		free(displayName);
		return true;
	}

	sampleMatrixBrowserEntry_t *newEntries = realloc(entries,
		(entryCount + 1) * sizeof (*entries));
	if (newEntries == NULL)
	{
		free(displayName);
		return false;
	}
	entries = newEntries;
	const size_t nameLength = UNICHAR_STRLEN(nameU);
	entries[entryCount].nameU = malloc((nameLength + 1) * sizeof (UNICHAR));
	if (entries[entryCount].nameU != NULL)
		UNICHAR_STRCPY(entries[entryCount].nameU, nameU);
	entries[entryCount].displayName = displayName;
	entries[entryCount].directory = directory;
	entries[entryCount].selected = false;
	if (entries[entryCount].nameU == NULL)
	{
		free(displayName);
		entries[entryCount].displayName = NULL;
		return false;
	}
	entryCount++;
	return true;
}

static bool joinPath(const UNICHAR *name, UNICHAR path[PATH_MAX + 1])
{
	const size_t pathLength = UNICHAR_STRLEN(currentPath);
	const size_t nameLength = UNICHAR_STRLEN(name);
	if (pathLength + nameLength + 2 > PATH_MAX)
		return false;
	UNICHAR_STRCPY(path, currentPath);
	if (pathLength > 0 && currentPath[pathLength-1] != DIR_DELIMITER)
	{
#ifdef _WIN32
		UNICHAR_STRCAT(path, L"\\");
#else
		UNICHAR_STRCAT(path, "/");
#endif
	}
	UNICHAR_STRCAT(path, name);
	return true;
}

bool sampleMatrixBrowserRefresh(void)
{
	freeEntries();
#ifdef _WIN32
	UNICHAR searchPath[PATH_MAX + 1];
	if (!joinPath(L"*", searchPath))
		return false;
	WIN32_FIND_DATAW data;
	HANDLE find = FindFirstFileW(searchPath, &data);
	if (find == INVALID_HANDLE_VALUE)
		return false;
	do
	{
		if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L".."))
			continue;
		const bool directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		if (!appendEntry(data.cFileName, directory))
		{
			FindClose(find);
			return false;
		}
	}
	while (FindNextFileW(find, &data));
	FindClose(find);
#else
	DIR *directory = opendir(currentPath);
	if (directory == NULL)
		return false;
	struct dirent *entry;
	while ((entry = readdir(directory)) != NULL)
	{
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..") ||
			entry->d_name[0] == '.')
		{
			continue;
		}
		UNICHAR fullPath[PATH_MAX + 1];
		if (!joinPath(entry->d_name, fullPath))
			continue;
		struct stat info;
		if (stat(fullPath, &info) != 0)
			continue;
		if (!appendEntry(entry->d_name, S_ISDIR(info.st_mode)))
		{
			closedir(directory);
			return false;
		}
	}
	closedir(directory);
#endif
	qsort(entries, entryCount, sizeof (*entries), entryCompare);
	char *path = unicharToCp850(currentPath, true);
	if (path != NULL)
	{
		strncpy(displayPath, path, PATH_MAX);
		displayPath[PATH_MAX] = '\0';
		free(path);
	}
	else
	{
		displayPath[0] = '\0';
	}
	return true;
}

bool sampleMatrixBrowserOpen(void)
{
	/* Disk Op supplies the initial folder, but after the browser has been used
	** it owns its navigation state. Closing the Sample Matrix Editor releases
	** only the directory listing; reopening it returns to this saved path. */
	if (!pathInitialized)
	{
		const UNICHAR *samplePath = getDiskOpSmpPath();
		if (samplePath != NULL && samplePath[0] != 0)
			UNICHAR_STRNCPY(currentPath, samplePath, PATH_MAX);
		else if (UNICHAR_GETCWD(currentPath, PATH_MAX) == NULL)
			return false;
		currentPath[PATH_MAX] = 0;
		pathInitialized = true;
	}
	return sampleMatrixBrowserRefresh();
}

void sampleMatrixBrowserClose(void)
{
	freeEntries();
}

bool sampleMatrixBrowserGoParent(void)
{
#ifdef _WIN32
	const size_t rootLength = 3;
#else
	const size_t rootLength = 1;
#endif

	size_t length = UNICHAR_STRLEN(currentPath);
	while (length > rootLength && currentPath[length-1] == DIR_DELIMITER)
		currentPath[--length] = 0;
	if (length <= rootLength)
		return false;
	while (length > 0 && currentPath[length-1] != DIR_DELIMITER)
		currentPath[--length] = 0;
	while (length > rootLength && currentPath[length-1] == DIR_DELIMITER)
		currentPath[--length] = 0;
	return sampleMatrixBrowserRefresh();
}

bool sampleMatrixBrowserOpenDirectory(uint32_t index)
{
	if (index >= entryCount || !entries[index].directory)
		return false;
	UNICHAR path[PATH_MAX + 1];
	if (!joinPath(entries[index].nameU, path))
		return false;
	UNICHAR_STRCPY(currentPath, path);
	return sampleMatrixBrowserRefresh();
}

uint32_t sampleMatrixBrowserGetCount(void) { return entryCount; }
uint32_t sampleMatrixBrowserGetScroll(void) { return scrollOffset; }

void sampleMatrixBrowserSetScroll(uint32_t scroll)
{
	const uint32_t maximum = entryCount > SAMPLE_MATRIX_BROWSER_VISIBLE_ROWS
		? entryCount - SAMPLE_MATRIX_BROWSER_VISIBLE_ROWS : 0;
	scrollOffset = scroll > maximum ? maximum : scroll;
}

void sampleMatrixBrowserScroll(int32_t amount)
{
	int64_t position = (int64_t)scrollOffset + amount;
	if (position < 0) position = 0;
	sampleMatrixBrowserSetScroll((uint32_t)position);
}

const char *sampleMatrixBrowserGetName(uint32_t index)
{
	return index < entryCount ? entries[index].displayName : "";
}

bool sampleMatrixBrowserEntryIsDirectory(uint32_t index)
{
	return index < entryCount && entries[index].directory;
}

bool sampleMatrixBrowserEntryIsSelected(uint32_t index)
{
	return index < entryCount && entries[index].selected;
}

void sampleMatrixBrowserClearSelection(void)
{
	for (uint32_t i = 0; i < entryCount; i++)
		entries[i].selected = false;
	selectionAnchor = -1;
}

void sampleMatrixBrowserSelect(uint32_t index, bool toggle, bool range)
{
	if (index >= entryCount || entries[index].directory)
		return;
	if (range && selectionAnchor >= 0)
	{
		const uint32_t first = index < (uint32_t)selectionAnchor
			? index : (uint32_t)selectionAnchor;
		const uint32_t last = index > (uint32_t)selectionAnchor
			? index : (uint32_t)selectionAnchor;
		if (!toggle)
			sampleMatrixBrowserClearSelection();
		for (uint32_t i = first; i <= last; i++)
		{
			if (!entries[i].directory)
				entries[i].selected = true;
		}
		return;
	}
	if (!toggle)
		sampleMatrixBrowserClearSelection();
	entries[index].selected = toggle ? !entries[index].selected : true;
	selectionAnchor = (int32_t)index;
}

uint32_t sampleMatrixBrowserGetSelectionCount(void)
{
	uint32_t count = 0;
	for (uint32_t i = 0; i < entryCount; i++)
		count += entries[i].selected && !entries[i].directory;
	return count;
}

const UNICHAR *sampleMatrixBrowserGetSelectedName(uint32_t selectedIndex)
{
	for (uint32_t i = 0; i < entryCount; i++)
	{
		if (entries[i].selected && !entries[i].directory)
		{
			if (selectedIndex-- == 0)
				return entries[i].nameU;
		}
	}
	return NULL;
}

uint32_t sampleMatrixBrowserGetFileCount(void)
{
	uint32_t count = 0;
	for (uint32_t i = 0; i < entryCount; i++)
		count += !entries[i].directory;
	return count;
}

const UNICHAR *sampleMatrixBrowserGetFileName(uint32_t fileIndex)
{
	for (uint32_t i = 0; i < entryCount; i++)
	{
		if (!entries[i].directory && fileIndex-- == 0)
			return entries[i].nameU;
	}
	return NULL;
}

const UNICHAR *sampleMatrixBrowserGetPath(void) { return currentPath; }
const char *sampleMatrixBrowserGetDisplayPath(void) { return displayPath; }
