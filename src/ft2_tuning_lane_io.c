#include <stdlib.h>
#include <string.h>
#include "ft2_replayer.h"
#include "ft2_module_loader.h"
#include "ft2_tuning_lane_io.h"

#define TUNING_LANE_VERSION 1
static const char signature[8] = { 'T','H','T','U','N','E','1','\0' };

typedef struct tuningEntry_t
{
	uint8_t pattern, row, channel, type, data;
} tuningEntry_t;

bool tuningLaneWriteXMExtension(FILE *f, uint16_t numPatterns)
{
	uint32_t count = 0;
	for (uint16_t p = 0; p < numPatterns; p++)
		if (pattern[p] != NULL)
			for (int32_t r = 0; r < patternNumRows[p]; r++)
				for (int32_t c = 0; c < song.numChannels; c++)
					if (pattern[p][r * MAX_CHANNELS + c].tuneType != 0) count++;

	const uint16_t version = TUNING_LANE_VERSION;
	if (fwrite(signature, 1, 8, f) != 8 || fwrite(&version, 2, 1, f) != 1 ||
		fwrite(&count, 4, 1, f) != 1) return false;
	for (uint16_t p = 0; p < numPatterns; p++)
		if (pattern[p] != NULL)
			for (int32_t r = 0; r < patternNumRows[p]; r++)
				for (int32_t c = 0; c < song.numChannels; c++)
				{
					const note_t *n = &pattern[p][r * MAX_CHANNELS + c];
					if (n->tuneType == 0) continue;
					const tuningEntry_t e = { (uint8_t)p, (uint8_t)r,
						(uint8_t)c, n->tuneType, n->tuneData };
					if (fwrite(&e, 1, sizeof (e), f) != sizeof (e)) return false;
				}
	return true;
}

bool tuningLaneReadXMExtension(FILE *f, uint32_t fileSize)
{
	long pos = ftell(f);
	if (pos < 0 || (uint64_t)pos + 14 > fileSize) return true;
	char magic[8]; uint16_t version; uint32_t count;
	if (fread(magic, 1, 8, f) != 8 || memcmp(magic, signature, 8) != 0)
		return true; /* no extension is valid */
	if (fread(&version, 2, 1, f) != 1 || fread(&count, 4, 1, f) != 1 ||
		version != TUNING_LANE_VERSION || count > MAX_PATTERNS * MAX_PATT_LEN * MAX_CHANNELS ||
		(uint64_t)ftell(f) + (uint64_t)count * sizeof (tuningEntry_t) > fileSize) return false;

	tuningEntry_t *entries = count ? malloc(count * sizeof (*entries)) : NULL;
	if (count && (entries == NULL || fread(entries, sizeof (*entries), count, f) != count))
	{ free(entries); return false; }
	for (uint32_t i = 0; i < count; i++)
	{
		const tuningEntry_t *e = &entries[i];
		if (e->pattern >= MAX_PATTERNS || e->row >= patternNumRowsTmp[e->pattern] ||
			e->channel >= songTmp.numChannels || !microtonalEffectIsPitchExtension(e->type))
		{ free(entries); return false; }
	}
	for (uint32_t i = 0; i < count; i++)
	{
		const tuningEntry_t *e = &entries[i];
		if (patternTmp[e->pattern] == NULL && !allocateTmpPatt(e->pattern, patternNumRowsTmp[e->pattern]))
		{ free(entries); return false; }
		note_t *n = &patternTmp[e->pattern][e->row * MAX_CHANNELS + e->channel];
		if (n->tuneType != 0 && n->efx == 0) { n->efx = n->tuneType; n->efxData = n->tuneData; }
		n->tuneType = e->type; n->tuneData = e->data;
	}
	free(entries);
	return true;
}
