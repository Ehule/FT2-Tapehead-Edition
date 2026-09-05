#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_exs_manifest.h"

enum
{
	HEADER_VERSION = 1 << 0,
	HEADER_SOURCE = 1 << 1,
	HEADER_MODE = 1 << 2,
	HEADER_INSTRUMENTS = 1 << 3,
	HEADER_SAMPLES = 1 << 4,
	HEADER_REQUIRED = (1 << 5) - 1,
	SAMPLE_INSTRUMENT_INDEX = 1 << 0,
	SAMPLE_SAMPLE_INDEX = 1 << 1,
	SAMPLE_INSTRUMENT_NAME = 1 << 2,
	SAMPLE_SAMPLE_NAME = 1 << 3,
	SAMPLE_FILE = 1 << 4,
	SAMPLE_LENGTH = 1 << 5,
	SAMPLE_BIT_DEPTH = 1 << 6,
	SAMPLE_RELATIVE_NOTE = 1 << 7,
	SAMPLE_FINETUNE = 1 << 8,
	SAMPLE_VOLUME = 1 << 9,
	SAMPLE_PANNING = 1 << 10,
	SAMPLE_LOOP_TYPE = 1 << 11,
	SAMPLE_LOOP_START = 1 << 12,
	SAMPLE_LOOP_LENGTH = 1 << 13,
	SAMPLE_C4 = 1 << 14,
	SAMPLE_FLAGS = 1 << 15,
	SAMPLE_REQUIRED = (1 << 16) - 1
};

static bool asciiEqualIgnoreCase(const char *a, const char *b)
{
	while (*a != '\0' && *b != '\0')
	{
		if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++))
			return false;
	}
	return *a == '\0' && *b == '\0';
}

void exsStripKnownSampleExtension(char *name)
{
	static const char *extensions[] =
	{
		"iff", "raw", "wav", "snd", "smp", "sam", "aif", "pat",
		"aiff", "flac", "ogg", "mp3", "brr"
	};
	if (name == NULL)
		return;

	char *dot = strrchr(name, '.');
	if (dot == NULL || dot == name || dot[1] == '\0')
		return;

	for (size_t i = 0; i < sizeof (extensions) / sizeof (extensions[0]); i++)
	{
		if (asciiEqualIgnoreCase(dot+1, extensions[i]))
		{
			*dot = '\0';
			while (dot > name && (dot[-1] == ' ' || dot[-1] == '.'))
				*--dot = '\0';
			return;
		}
	}
}

static void setError(char *error, size_t errorSize, const char *format, ...)
{
	if (error == NULL || errorSize == 0)
		return;
	va_list args;
	va_start(args, format);
	vsnprintf(error, errorSize, format, args);
	va_end(args);
}

static char *trim(char *text)
{
	while (isspace((unsigned char)*text))
		text++;
	char *end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1]))
		*--end = '\0';
	return text;
}

static bool copyValue(char *destination, size_t capacity, const char *value)
{
	const size_t length = strlen(value);
	if (length >= capacity)
		return false;
	memcpy(destination, value, length + 1);
	return true;
}

static bool parseSigned(const char *text, int32_t minimum, int32_t maximum,
	int32_t *value)
{
	if (text == NULL || *text == '\0')
		return false;
	errno = 0;
	char *end;
	const long parsed = strtol(text, &end, 10);
	if (errno != 0 || *end != '\0' || parsed < minimum || parsed > maximum)
		return false;
	*value = (int32_t)parsed;
	return true;
}

static bool parseUnsigned(const char *text, uint32_t maximum, uint32_t *value)
{
	if (text == NULL || *text == '\0' || *text == '-')
		return false;
	errno = 0;
	char *end;
	const unsigned long parsed = strtoul(text, &end, 10);
	if (errno != 0 || *end != '\0' || parsed > maximum)
		return false;
	*value = (uint32_t)parsed;
	return true;
}

void exsManifestInit(exsManifest_t *manifest)
{
	if (manifest != NULL)
		memset(manifest, 0, sizeof (*manifest));
}

void exsManifestFree(exsManifest_t *manifest)
{
	if (manifest == NULL)
		return;
	free(manifest->samples);
	exsManifestInit(manifest);
}

bool exsManifestRelativePathIsSafe(const char *path)
{
	if (path == NULL || path[0] == '\0' || path[0] == '/' || path[0] == '\\')
		return false;

	const char *segment = path;
	for (const unsigned char *p = (const unsigned char *)path; ; p++)
	{
		if ((*p != '\0' && *p < 32) || *p == ':' || *p == 127)
			return false;
		if (*p == '/' || *p == '\\' || *p == '\0')
		{
			const size_t length = (const char *)p - segment;
			if (length == 0 || (length == 1 && segment[0] == '.') ||
				(length == 2 && segment[0] == '.' && segment[1] == '.'))
			{
				return false;
			}
			if (*p == '\0')
				break;
			segment = (const char *)p + 1;
		}
	}
	return true;
}

static bool samePath(const char *a, const char *b)
{
	while (*a != '\0' && *b != '\0')
	{
		const int ca = (*a == '\\' ? '/' : tolower((unsigned char)*a));
		const int cb = (*b == '\\' ? '/' : tolower((unsigned char)*b));
		if (ca != cb)
			return false;
		a++;
		b++;
	}
	return *a == *b;
}

static bool finishSample(exsManifest_t *manifest, exsManifestSample_t *sample,
	uint32_t fields, uint16_t *entryCount, bool destinations[EXS_MAX_INSTRUMENTS+1]
	[EXS_MAX_SAMPLES_PER_INSTRUMENT], char *error, size_t errorSize)
{
	if (*entryCount >= EXS_MAX_ENTRIES)
	{
		setError(error, errorSize, "EXS manifest contains too many sample entries");
		return false;
	}
	if (fields != SAMPLE_REQUIRED)
	{
		setError(error, errorSize, "Incomplete sample entry %u", *entryCount + 1);
		return false;
	}
	if (!exsManifestRelativePathIsSafe(sample->file))
	{
		setError(error, errorSize, "Unsafe WAV path in sample entry %u", *entryCount + 1);
		return false;
	}
	if (destinations[sample->instrumentIndex][sample->sampleIndex])
	{
		setError(error, errorSize, "Duplicate destination I%02u:S%02u",
			sample->instrumentIndex, sample->sampleIndex);
		return false;
	}
	for (uint16_t i = 0; i < *entryCount; i++)
	{
		if (samePath(manifest->samples[i].file, sample->file))
		{
			setError(error, errorSize, "Duplicate WAV path in manifest");
			return false;
		}
	}
	destinations[sample->instrumentIndex][sample->sampleIndex] = true;
	manifest->samples[*entryCount] = *sample;
	(*entryCount)++;
	return true;
}

bool exsManifestParse(FILE *file, exsManifest_t *manifest, char *error,
	size_t errorSize)
{
	if (file == NULL || manifest == NULL)
	{
		setError(error, errorSize, "Invalid EXS manifest input");
		return false;
	}
	exsManifestFree(manifest);
	if (error != NULL && errorSize > 0)
		error[0] = '\0';

	exsManifestSample_t *samples = calloc(EXS_MAX_ENTRIES, sizeof (*samples));
	if (samples == NULL)
	{
		setError(error, errorSize, "Not enough memory for EXS manifest");
		return false;
	}
	manifest->samples = samples;

	enum { SECTION_NONE, SECTION_HEADER, SECTION_SAMPLE } section = SECTION_NONE;
	uint32_t headerFields = 0, sampleFields = 0;
	uint16_t entryCount = 0;
	bool destinations[EXS_MAX_INSTRUMENTS+1][EXS_MAX_SAMPLES_PER_INSTRUMENT] = { { false } };
	exsManifestSample_t current = { 0 };
	char line[1024];
	uint32_t lineNumber = 0;
	while (fgets(line, sizeof (line), file) != NULL)
	{
		lineNumber++;
		if (strchr(line, '\n') == NULL && !feof(file))
		{
			setError(error, errorSize, "Manifest line %u is too long", lineNumber);
			goto fail;
		}
		char *text = trim(line);
		if (*text == '\0' || *text == ';' || *text == '#')
			continue;
		if (*text == '[')
		{
			const size_t length = strlen(text);
			if (length < 3 || text[length-1] != ']')
			{
				setError(error, errorSize, "Malformed section on line %u", lineNumber);
				goto fail;
			}
			if (section == SECTION_SAMPLE &&
				!finishSample(manifest, &current, sampleFields, &entryCount,
					destinations, error, errorSize))
			{
				goto fail;
			}
			memset(&current, 0, sizeof (current));
			sampleFields = 0;
			text[length-1] = '\0';
			if (!strcmp(text + 1, "EXS"))
				section = SECTION_HEADER;
			else if (!strncmp(text + 1, "Instrument", 10) &&
				strstr(text + 1, ".Sample") != NULL)
				section = SECTION_SAMPLE;
			else
			{
				setError(error, errorSize, "Unsupported section on line %u", lineNumber);
				goto fail;
			}
			continue;
		}

		char *separator = strchr(text, '=');
		if (separator == NULL || section == SECTION_NONE)
		{
			setError(error, errorSize, "Malformed field on line %u", lineNumber);
			goto fail;
		}
		*separator++ = '\0';
		char *key = trim(text);
		char *value = trim(separator);
		int32_t signedValue = 0;
		uint32_t unsignedValue = 0;
		uint32_t bit = 0;
		bool valid = true;
		if (section == SECTION_HEADER)
		{
			if (!strcmp(key, "FormatVersion"))
			{
				bit = HEADER_VERSION;
				valid = parseUnsigned(value, UINT16_MAX, &unsignedValue);
				manifest->formatVersion = (uint16_t)unsignedValue;
			}
			else if (!strcmp(key, "SourceModule"))
			{
				bit = HEADER_SOURCE;
				valid = copyValue(manifest->sourceModule, sizeof (manifest->sourceModule), value);
			}
			else if (!strcmp(key, "ExportMode"))
			{
				bit = HEADER_MODE;
				valid = copyValue(manifest->exportMode, sizeof (manifest->exportMode), value);
			}
			else if (!strcmp(key, "InstrumentCount"))
			{
				bit = HEADER_INSTRUMENTS;
				valid = parseUnsigned(value, EXS_MAX_INSTRUMENTS, &unsignedValue);
				manifest->instrumentCount = (uint16_t)unsignedValue;
			}
			else if (!strcmp(key, "SampleCount"))
			{
				bit = HEADER_SAMPLES;
				valid = parseUnsigned(value, EXS_MAX_ENTRIES, &unsignedValue);
				manifest->sampleCount = (uint16_t)unsignedValue;
			}
			if (bit != 0)
			{
				if (!valid || (headerFields & bit) != 0)
				{
					setError(error, errorSize, "Invalid or duplicate field on line %u", lineNumber);
					goto fail;
				}
				headerFields |= bit;
			}
		}
		else
		{
#define EXS_SIGNED_FIELD(name, field, flag, minimum, maximum) \
			if (!strcmp(key, name)) { bit = flag; valid = parseSigned(value, minimum, maximum, &signedValue); current.field = signedValue; }
#define EXS_UNSIGNED_FIELD(name, field, flag, maximum) \
			if (!strcmp(key, name)) { bit = flag; valid = parseUnsigned(value, maximum, &unsignedValue); current.field = unsignedValue; }
			EXS_UNSIGNED_FIELD("InstrumentIndex", instrumentIndex, SAMPLE_INSTRUMENT_INDEX, EXS_MAX_INSTRUMENTS)
			else EXS_UNSIGNED_FIELD("SampleIndex", sampleIndex, SAMPLE_SAMPLE_INDEX, EXS_MAX_SAMPLES_PER_INSTRUMENT-1)
			else if (!strcmp(key, "InstrumentName")) { bit = SAMPLE_INSTRUMENT_NAME; valid = copyValue(current.instrumentName, sizeof (current.instrumentName), value); }
			else if (!strcmp(key, "SampleName")) { bit = SAMPLE_SAMPLE_NAME; valid = copyValue(current.sampleName, sizeof (current.sampleName), value); }
			else if (!strcmp(key, "File")) { bit = SAMPLE_FILE; valid = copyValue(current.file, sizeof (current.file), value); }
			else EXS_SIGNED_FIELD("LengthFrames", lengthFrames, SAMPLE_LENGTH, 1, INT32_MAX)
			else if (!strcmp(key, "SourceBitDepth"))
			{
				bit = SAMPLE_BIT_DEPTH;
				valid = parseSigned(value, 8, 16, &signedValue) &&
					(signedValue == 8 || signedValue == 16);
				current.sourceBitDepth = signedValue;
			}
			else EXS_SIGNED_FIELD("RelativeNote", relativeNote, SAMPLE_RELATIVE_NOTE, -128, 127)
			else EXS_SIGNED_FIELD("Finetune", finetune, SAMPLE_FINETUNE, -128, 127)
			else EXS_UNSIGNED_FIELD("DefaultVolume", defaultVolume, SAMPLE_VOLUME, 64)
			else EXS_UNSIGNED_FIELD("DefaultPanning", defaultPanning, SAMPLE_PANNING, 255)
			else if (!strcmp(key, "LoopType"))
			{
				bit = SAMPLE_LOOP_TYPE;
				if (!strcmp(value, "None")) current.loopType = EXS_LOOP_NONE;
				else if (!strcmp(value, "Forward")) current.loopType = EXS_LOOP_FORWARD;
				else if (!strcmp(value, "PingPong")) current.loopType = EXS_LOOP_PINGPONG;
				else valid = false;
			}
			else EXS_SIGNED_FIELD("LoopStart", loopStart, SAMPLE_LOOP_START, 0, INT32_MAX)
			else EXS_SIGNED_FIELD("LoopLength", loopLength, SAMPLE_LOOP_LENGTH, 0, INT32_MAX)
			else EXS_SIGNED_FIELD("C4Frequency", c4Frequency, SAMPLE_C4, 1, INT32_MAX)
			else EXS_UNSIGNED_FIELD("Flags", flags, SAMPLE_FLAGS, UINT8_MAX)
#undef EXS_SIGNED_FIELD
#undef EXS_UNSIGNED_FIELD
			if (bit != 0)
			{
				if (!valid || (sampleFields & bit) != 0)
				{
					setError(error, errorSize, "Invalid or duplicate field on line %u", lineNumber);
					goto fail;
				}
				sampleFields |= bit;
			}
		}
	}
	if (ferror(file))
	{
		setError(error, errorSize, "Could not read EXS manifest");
		goto fail;
	}
	if (section == SECTION_SAMPLE &&
		!finishSample(manifest, &current, sampleFields, &entryCount,
			destinations, error, errorSize))
	{
		goto fail;
	}
	if (headerFields != HEADER_REQUIRED || manifest->formatVersion != 1 ||
		manifest->sampleCount == 0 || manifest->sampleCount != entryCount)
	{
		setError(error, errorSize, "Incomplete or unsupported EXS v1 manifest");
		goto fail;
	}
	if (manifest->sourceModule[0] == '\0' ||
		(strcmp(manifest->exportMode, "UsedInstruments") != 0 &&
		 strcmp(manifest->exportMode, "AllInstruments") != 0))
	{
		setError(error, errorSize, "Invalid EXS source or export mode");
		goto fail;
	}
	bool instruments[EXS_MAX_INSTRUMENTS+1] = { false };
	uint16_t instrumentCount = 0;
	for (uint16_t i = 0; i < entryCount; i++)
	{
		if (manifest->samples[i].instrumentIndex == 0)
		{
			setError(error, errorSize, "InstrumentIndex 0 cannot hold samples");
			goto fail;
		}
		if (!instruments[manifest->samples[i].instrumentIndex])
		{
			instruments[manifest->samples[i].instrumentIndex] = true;
			instrumentCount++;
		}
	}
	if (manifest->instrumentCount != instrumentCount)
	{
		setError(error, errorSize, "InstrumentCount does not match sample entries");
		goto fail;
	}
	return true;

fail:
	exsManifestFree(manifest);
	return false;
}
