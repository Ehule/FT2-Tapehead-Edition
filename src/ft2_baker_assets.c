#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_replayer.h"
#include "ft2_baker_assets.h"

#define BAKER_SMP_DATA_OFFSET ((MAX_LEFT_TAPS * 2) + 1)
#define BAKER_SAMPLE_PAD_LENGTH (BAKER_SMP_DATA_OFFSET + (MAX_RIGHT_TAPS * 2))

typedef struct bakerPrivateAsset_t
{
	uint8_t sourceInstrument, sourceSample, destinationInstrument;
	uint64_t fingerprint;
	instr_t *instrument;
} bakerPrivateAsset_t;

static bakerPrivateAsset_t assets[MAX_INST];
static uint16_t assetCount;
static bakerAssetError_t assetError;
static bool assetsInstalled;

static uint64_t hashBytes(uint64_t hash, const void *data, size_t length)
{
	const uint8_t *bytes = (const uint8_t *)data;
	while (length-- > 0)
	{
		hash ^= *bytes++;
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

static uint64_t audibleFingerprint(const instr_t *instrument, const sample_t *sample)
{
	uint64_t hash = UINT64_C(1469598103934665603);
	/* All standard-XM instrument behavior precedes the sample array. Runtime
	** timeline metadata is harmlessly included and makes the identity stricter. */
	hash = hashBytes(hash, instrument, offsetof(instr_t, smp));
	hash = hashBytes(hash, sample->name, sizeof (sample->name));
	hash = hashBytes(hash, &sample->finetune,
		offsetof(sample_t, dataPtr) - offsetof(sample_t, finetune));
	hash = hashBytes(hash, &sample->volume,
		offsetof(sample_t, leftEdgeTapSamples8) - offsetof(sample_t, volume));
	hash = hashBytes(hash, sample->dataPtr, SAMPLE_LENGTH_BYTES(sample));
	return hash;
}

static void freePrivateInstrument(instr_t *instrument)
{
	if (instrument == NULL)
		return;
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
		free(instrument->smp[i].origDataPtr);
	free(instrument);
}

void bakerAssetsFree(void)
{
	bakerAssetsUninstall();
	for (uint16_t i = 0; i < assetCount; i++)
		freePrivateInstrument(assets[i].instrument);
	memset(assets, 0, sizeof (assets));
	assetCount = 0;
	assetError = BAKER_ASSET_OK;
}

void bakerAssetsReset(void)
{
	bakerAssetsFree();
}

static uint8_t findDestinationInstrument(void)
{
	for (uint16_t candidate = 1; candidate <= MAX_INST; candidate++)
	{
		if (instr[candidate] != NULL)
			continue;
		bool reserved = false;
		for (uint16_t i = 0; i < assetCount; i++)
			reserved |= assets[i].destinationInstrument == candidate;
		if (!reserved)
			return (uint8_t)candidate;
	}
	return 0;
}

static instr_t *copyPrivateInstrument(const instr_t *source, uint8_t sourceSample)
{
	instr_t *copy = (instr_t *)calloc(1, sizeof (*copy));
	if (copy == NULL)
		return NULL;

	/* Copy instrument envelopes, fadeout, vibrato and standard XM settings, but
	** own exactly one selected sample in slot zero. */
	memcpy(copy, source, offsetof(instr_t, smp));
	memset(copy->note2SampleLUT, 0, sizeof (copy->note2SampleLUT));
	copy->numSamples = 1;
	const sample_t *sourceSmp = &source->smp[sourceSample];
	sample_t *destinationSmp = &copy->smp[0];
	*destinationSmp = *sourceSmp;
	destinationSmp->dataPtr = destinationSmp->origDataPtr = NULL;
	const size_t bytes = SAMPLE_LENGTH_BYTES(sourceSmp);
	destinationSmp->origDataPtr = (int8_t *)calloc(1, bytes + BAKER_SAMPLE_PAD_LENGTH);
	if (destinationSmp->origDataPtr == NULL)
	{
		free(copy);
		return NULL;
	}
	destinationSmp->dataPtr = destinationSmp->origDataPtr + BAKER_SMP_DATA_OFFSET;
	memcpy(destinationSmp->dataPtr, sourceSmp->dataPtr, bytes);
	return copy;
}

uint8_t bakerAssetsResolveInstrument(uint8_t note, uint8_t sourceInstrument,
	uint8_t resolvedSample)
{
	if (assetError != BAKER_ASSET_OK || note < 1 || note > 96 ||
		sourceInstrument == 0 || sourceInstrument > MAX_INST ||
		resolvedSample >= MAX_SMP_PER_INST || instr[sourceInstrument] == NULL)
		return 0;

	const instr_t *source = instr[sourceInstrument];
	const sample_t *sample = &source->smp[resolvedSample];
	if (sample->dataPtr == NULL || sample->length <= 0)
		return 0;

	/* Reuse is faithful only at the actual played note. Never transpose to some
	** other LUT entry merely to reach the selected sample. */
	if ((source->note2SampleLUT[note-1] & 0x0F) == resolvedSample)
		return sourceInstrument;

	const uint64_t fingerprint = audibleFingerprint(source, sample);
	for (uint16_t i = 0; i < assetCount; i++)
	{
		if (assets[i].sourceInstrument == sourceInstrument &&
			assets[i].sourceSample == resolvedSample &&
			assets[i].fingerprint == fingerprint)
			return assets[i].destinationInstrument;
	}

	const uint8_t destination = findDestinationInstrument();
	if (destination == 0)
	{
		assetError = BAKER_ASSET_INSTRUMENT_LIMIT;
		return 0;
	}
	instr_t *copy = copyPrivateInstrument(source, resolvedSample);
	if (copy == NULL)
	{
		assetError = BAKER_ASSET_MEMORY;
		return 0;
	}
	assets[assetCount++] = (bakerPrivateAsset_t)
	{
		sourceInstrument, resolvedSample, destination, fingerprint, copy
	};
	return destination;
}

bool bakerAssetsResolveEvent(note_t *event, uint8_t sourceInstrument,
	uint8_t resolvedSample)
{
	if (event == NULL)
		return false;

	/* The performed note is the pitch contract. Sample Morph may change the
	** instrument needed to represent the resolved sample, never this field. */
	const uint8_t playedNote = event->note;
	const uint8_t destination = bakerAssetsResolveInstrument(playedNote,
		sourceInstrument, resolvedSample);
	if (destination == 0)
		return false;

	event->instr = destination;
	event->note = playedNote;
	return true;
}

bool bakerAssetsInstall(void)
{
	if (assetError != BAKER_ASSET_OK || assetsInstalled)
		return assetError == BAKER_ASSET_OK;
	for (uint16_t i = 0; i < assetCount; i++)
	{
		if (instr[assets[i].destinationInstrument] != NULL)
		{
			assetError = BAKER_ASSET_INSTRUMENT_LIMIT;
			return false;
		}
	}
	for (uint16_t i = 0; i < assetCount; i++)
		instr[assets[i].destinationInstrument] = assets[i].instrument;
	assetsInstalled = true;
	return true;
}

void bakerAssetsUninstall(void)
{
	if (!assetsInstalled)
		return;
	for (uint16_t i = 0; i < assetCount; i++)
	{
		if (instr[assets[i].destinationInstrument] == assets[i].instrument)
			instr[assets[i].destinationInstrument] = NULL;
	}
	assetsInstalled = false;
}

bakerAssetError_t bakerAssetsGetError(void)
{
	return assetError;
}

uint16_t bakerAssetsGetPrivateCount(void)
{
	return assetCount;
}
