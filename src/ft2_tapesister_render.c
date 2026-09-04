#include "ft2_tapesister_render.h"

#include <string.h>

const char *tapeheadRenderScopeName(tapeheadRenderScope_t scope)
{
	switch (scope)
	{
		case TAPEHEAD_RENDER_BLOCK: return "block";
		case TAPEHEAD_RENDER_PATTERN_MIX: return "pattern_mix";
		case TAPEHEAD_RENDER_PATTERN_TRACK: return "pattern_track";
		case TAPEHEAD_RENDER_SONG_TRACK: return "song_track";
		case TAPEHEAD_RENDER_SONG_MIX: return "song_mix";
		default: return "invalid";
	}
}

const char *tapeheadRenderScopeLabel(tapeheadRenderScope_t scope)
{
	switch (scope)
	{
		case TAPEHEAD_RENDER_BLOCK: return "Selected block";
		case TAPEHEAD_RENDER_PATTERN_MIX: return "Current pattern mix";
		case TAPEHEAD_RENDER_PATTERN_TRACK: return "Current pattern track";
		case TAPEHEAD_RENDER_SONG_TRACK: return "Full-song track";
		case TAPEHEAD_RENDER_SONG_MIX: return "Full-song mix";
		default: return "Invalid render";
	}
}

bool tapeheadRenderPlanInit(tapeheadRenderPlan_t *plan,
	tapeheadRenderScope_t scope, uint16_t songLength, uint16_t songPosition,
	uint16_t pattern, uint16_t track, uint16_t sourceChannels,
	uint16_t initialBPM, uint16_t initialSpeed, uint32_t sampleRate,
	uint8_t bitDepth)
{
	if (plan == NULL || scope <= TAPEHEAD_RENDER_BLOCK ||
		scope >= TAPEHEAD_RENDER_SCOPE_COUNT ||
		songLength == 0 || songLength > 256 || songPosition >= songLength ||
		pattern > 255 || sourceChannels == 0 || sourceChannels > 255 ||
		track >= sourceChannels || sampleRate == 0 ||
		(bitDepth != 16 && bitDepth != 32))
	{
		return false;
	}

	memset(plan, 0, sizeof (*plan));
	plan->scope = scope;
	plan->startOrder = (scope == TAPEHEAD_RENDER_PATTERN_MIX ||
		scope == TAPEHEAD_RENDER_PATTERN_TRACK) ? (uint8_t)songPosition : 0;
	plan->stopOrder = (scope == TAPEHEAD_RENDER_PATTERN_MIX ||
		scope == TAPEHEAD_RENDER_PATTERN_TRACK) ? (uint8_t)songPosition :
		(uint8_t)(songLength - 1);
	plan->pattern = (scope == TAPEHEAD_RENDER_PATTERN_MIX ||
		scope == TAPEHEAD_RENDER_PATTERN_TRACK) ? (int16_t)pattern : -1;
	plan->sourceChannels = (uint8_t)sourceChannels;
	plan->rowStart = plan->rowEnd = -1;
	plan->channelStart = plan->channelEnd = -1;
	plan->soloChannel = (scope == TAPEHEAD_RENDER_PATTERN_TRACK ||
		scope == TAPEHEAD_RENDER_SONG_TRACK) ? (int16_t)track : -1;
	plan->initialBPM = initialBPM;
	plan->initialSpeed = initialSpeed;
	plan->sampleRate = sampleRate;
	plan->bitDepth = bitDepth;

	int written;
	if (scope == TAPEHEAD_RENDER_PATTERN_MIX)
	{
		written = snprintf(plan->filename, sizeof (plan->filename),
			"Pattern_%02X_Order_%02X_Mix.wav", (unsigned int)pattern,
			(unsigned int)songPosition);
	}
	else if (scope == TAPEHEAD_RENDER_PATTERN_TRACK)
	{
		written = snprintf(plan->filename, sizeof (plan->filename),
			"Pattern_%02X_Order_%02X_Track_%02u.wav", (unsigned int)pattern,
			(unsigned int)songPosition, (unsigned int)track + 1);
	}
	else if (scope == TAPEHEAD_RENDER_SONG_TRACK)
	{
		written = snprintf(plan->filename, sizeof (plan->filename),
			"Song_Track_%02u.wav", (unsigned int)track + 1);
	}
	else
	{
		written = snprintf(plan->filename, sizeof (plan->filename),
			"Song_Mix.wav");
	}

	return written > 0 && (size_t)written < sizeof (plan->filename);
}

bool tapeheadRenderBlockPlanInit(tapeheadRenderPlan_t *plan,
	const tapeheadBlockLoopSpec_t *spec, uint16_t sourceChannels,
	uint32_t sampleRate, uint8_t bitDepth)
{
	if (plan == NULL || spec == NULL || spec->pattern > 255 ||
		spec->rowEnd <= spec->rowStart ||
		spec->channelEnd < spec->channelStart || sourceChannels == 0 ||
		sourceChannels > 255 || spec->channelEnd >= sourceChannels ||
		sampleRate == 0 || (bitDepth != 16 && bitDepth != 32))
	{
		return false;
	}

	memset(plan, 0, sizeof (*plan));
	plan->scope = TAPEHEAD_RENDER_BLOCK;
	plan->pattern = (int16_t)spec->pattern;
	plan->soloChannel = -1;
	plan->rowStart = (int16_t)spec->rowStart;
	plan->rowEnd = (int16_t)spec->rowEnd;
	plan->channelStart = (int16_t)spec->channelStart;
	plan->channelEnd = (int16_t)spec->channelEnd;
	plan->sourceChannels = (uint8_t)sourceChannels;
	plan->initialBPM = spec->initialBPM;
	plan->initialSpeed = spec->initialSpeed;
	plan->sampleRate = sampleRate;
	plan->bitDepth = bitDepth;
	const int written = snprintf(plan->filename, sizeof (plan->filename),
		"Block_P%02X_R%03u-%03u_T%02u-%02u.wav",
		(unsigned int)spec->pattern, (unsigned int)spec->rowStart,
		(unsigned int)spec->rowEnd - 1, (unsigned int)spec->channelStart + 1,
		(unsigned int)spec->channelEnd + 1);
	return written > 0 && (size_t)written < sizeof (plan->filename);
}

bool tapeheadRenderWriteMetadata(FILE *file,
	const tapeheadRenderPlan_t *plan, uint64_t renderedFrames)
{
	if (file == NULL || plan == NULL || plan->sampleRate == 0 ||
		plan->scope < 0 || plan->scope >= TAPEHEAD_RENDER_SCOPE_COUNT)
	{
		return false;
	}

	const uint64_t durationMilliseconds =
		(renderedFrames * UINT64_C(1000) + plan->sampleRate / 2) /
		plan->sampleRate;
	const int written = fprintf(file,
		"TAPEHEAD_RENDER 1\n"
		"scope=%s\n"
		"wav=%s\n"
		"order_start=%u\n"
		"order_end=%u\n"
		"pattern=%d\n"
		"track=%u\n"
		"row_start=%d\n"
		"row_end=%d\n"
		"channel_start=%d\n"
		"channel_end=%d\n"
		"source_channels=%u\n"
		"output_channels=2\n"
		"sample_rate=%u\n"
		"bit_depth=%u\n"
		"initial_bpm=%u\n"
		"initial_speed=%u\n"
		"frames=%llu\n"
		"duration_ms=%llu\n",
		tapeheadRenderScopeName(plan->scope), plan->filename,
		(unsigned int)plan->startOrder, (unsigned int)plan->stopOrder,
		plan->pattern,
		plan->soloChannel >= 0 ? (unsigned int)plan->soloChannel + 1 : 0,
		plan->rowStart,
		plan->rowEnd >= 0 ? plan->rowEnd - 1 : -1,
		plan->channelStart >= 0 ? plan->channelStart + 1 : -1,
		plan->channelEnd >= 0 ? plan->channelEnd + 1 : -1,
		(unsigned int)plan->sourceChannels, plan->sampleRate,
		(unsigned int)plan->bitDepth, (unsigned int)plan->initialBPM,
		(unsigned int)plan->initialSpeed,
		(unsigned long long)renderedFrames,
		(unsigned long long)durationMilliseconds);
	return written > 0 && ferror(file) == 0;
}
