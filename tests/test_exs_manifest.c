#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ft2_exs_manifest.h"

static const char *header =
	"[EXS]\n"
	"FormatVersion=1\n"
	"SourceModule=Round Trip.xm\n"
	"ExportMode=UsedInstruments\n"
	"InstrumentCount=1\n"
	"SampleCount=1\n\n";

static const char *sample =
	"[Instrument01.Sample00]\n"
	"InstrumentIndex=1\n"
	"SampleIndex=0\n"
	"InstrumentName=Drums\n"
	"SampleName=Kick\n"
	"File=instrument_01_Drums/I01_S00_Kick.wav\n"
	"LengthFrames=44100\n"
	"SourceBitDepth=16\n"
	"RelativeNote=0\n"
	"Finetune=-8\n"
	"DefaultVolume=64\n"
	"DefaultPanning=128\n"
	"LoopType=Forward\n"
	"LoopStart=100\n"
	"LoopLength=4000\n"
	"C4Frequency=44100\n"
	"Flags=17\n";

static bool parseText(const char *text, exsManifest_t *manifest,
	char *error, size_t errorSize)
{
	FILE *file = tmpfile();
	assert(file != NULL);
	assert(fputs(text, file) >= 0);
	rewind(file);
	const bool result = exsManifestParse(file, manifest, error, errorSize);
	fclose(file);
	return result;
}

static void testValidManifest(void)
{
	char text[4096], error[192];
	snprintf(text, sizeof (text), "%s%s", header, sample);
	exsManifest_t manifest;
	exsManifestInit(&manifest);
	const bool parsed = parseText(text, &manifest, error, sizeof (error));
	if (!parsed)
		fprintf(stderr, "Valid manifest rejected: %s\n", error);
	assert(parsed);
	assert(manifest.formatVersion == 1);
	assert(manifest.instrumentCount == 1);
	assert(manifest.sampleCount == 1);
	assert(manifest.samples[0].instrumentIndex == 1);
	assert(manifest.samples[0].sampleIndex == 0);
	assert(manifest.samples[0].loopType == EXS_LOOP_FORWARD);
	assert(manifest.samples[0].loopLength == 4000);
	assert(!strcmp(manifest.samples[0].sampleName, "Kick"));
	exsManifestFree(&manifest);
}

static void expectRejected(const char *text, const char *errorPart)
{
	exsManifest_t manifest;
	exsManifestInit(&manifest);
	char error[192];
	assert(!parseText(text, &manifest, error, sizeof (error)));
	assert(strstr(error, errorPart) != NULL);
	exsManifestFree(&manifest);
}

static void testUnsafePaths(void)
{
	assert(exsManifestRelativePathIsSafe("instrument/I01.wav"));
	assert(exsManifestRelativePathIsSafe("instrument\\I01.wav"));
	assert(!exsManifestRelativePathIsSafe("../I01.wav"));
	assert(!exsManifestRelativePathIsSafe("instrument/../I01.wav"));
	assert(!exsManifestRelativePathIsSafe("/tmp/I01.wav"));
	assert(!exsManifestRelativePathIsSafe("C:\\I01.wav"));
	assert(!exsManifestRelativePathIsSafe("instrument//I01.wav"));
}

static void testSampleExtensionNormalization(void)
{
	char wav[] = "terra01.wav";
	exsStripKnownSampleExtension(wav);
	assert(!strcmp(wav, "terra01"));

	char upper[] = "Texture.WAV";
	exsStripKnownSampleExtension(upper);
	assert(!strcmp(upper, "Texture"));

	char otherAudio[] = "grain.aiff";
	exsStripKnownSampleExtension(otherAudio);
	assert(!strcmp(otherAudio, "grain"));

	char descriptiveDot[] = "take.v2";
	exsStripKnownSampleExtension(descriptiveDot);
	assert(!strcmp(descriptiveDot, "take.v2"));
}

static void testMalformedAndUnsafeManifests(void)
{
	char text[4096];
	snprintf(text, sizeof (text), "%s%s", header, sample);
	char *path = strstr(text, "instrument_01_Drums/I01_S00_Kick.wav");
	assert(path != NULL);
	memmove(path + 10, path + strlen("instrument_01_Drums/I01_S00_Kick.wav"),
		strlen(path + strlen("instrument_01_Drums/I01_S00_Kick.wav")) + 1);
	memcpy(path, "../bad.wav", 10);
	expectRejected(text, "Unsafe WAV path");

	snprintf(text, sizeof (text), "%s%s", header, sample);
	char *bitDepth = strstr(text, "SourceBitDepth=16");
	assert(bitDepth != NULL);
	memcpy(bitDepth + strlen("SourceBitDepth="), "12", 2);
	expectRejected(text, "Invalid or duplicate field");

	snprintf(text, sizeof (text), "%s%s", header, sample);
	char *sampleCount = strstr(text, "SampleCount=1");
	assert(sampleCount != NULL);
	sampleCount[strlen("SampleCount=")] = '2';
	expectRejected(text, "Incomplete or unsupported");
}

static void testDuplicateDestination(void)
{
	char text[8192];
	char second[4096];
	snprintf(second, sizeof (second), "%s", sample);
	char *section = strstr(second, "[Instrument01.Sample00]");
	char *file = strstr(second, "I01_S00_Kick.wav");
	assert(section != NULL && file != NULL);
	memcpy(section, "[Instrument01.Sample01]", strlen("[Instrument01.Sample01]"));
	memcpy(file, "I01_S01_Kick.wav", strlen("I01_S01_Kick.wav"));

	char twoHeader[1024];
	snprintf(twoHeader, sizeof (twoHeader), "%s", header);
	char *count = strstr(twoHeader, "SampleCount=1");
	assert(count != NULL);
	count[strlen("SampleCount=")] = '2';
	snprintf(text, sizeof (text), "%s%s%s", twoHeader, sample, second);
	/* Section labels and filenames are descriptive. The authoritative index
	** pair remains duplicated, so the manifest must be rejected. */
	expectRejected(text, "Duplicate destination");
}

int main(void)
{
	testValidManifest();
	testUnsafePaths();
	testSampleExtensionNormalization();
	testMalformedAndUnsafeManifests();
	testDuplicateDestination();
	puts("EXS manifest contract tests passed.");
	return 0;
}
