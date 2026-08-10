#include <limits.h>
#include <math.h>
#include <stddef.h>
#include "ft2_microtonal.h"
#include "ft2_replayer.h"

#define DRIFT_MIN_SECONDS 6
#define DRIFT_EXTRA_SECONDS 8
#define SECONDS_FRAC_BITS 16
#define SECONDS_ONE (1U << SECONDS_FRAC_BITS)

static uint32_t nextRandom(microtonalState_t *state)
{
	/* A private xorshift stream makes every voice repeatable without sharing
	** the dither, UI or mutation generators. */
	uint32_t value = state->driftPrngState;
	value ^= value << 13;
	value ^= value >> 17;
	value ^= value << 5;
	state->driftPrngState = value;
	return value;
}

static void chooseDriftTarget(microtonalState_t *state)
{
	const uint32_t randomTarget = nextRandom(state);
	const int64_t span16 = (int64_t)state->driftDepthCents * MICROTONAL_CENTS_ONE;
	const uint64_t range16 = (uint64_t)(span16 * 2) + 1;
	state->driftTargetCents16 = (int32_t)((int64_t)(randomTarget % range16) - span16);

	const uint32_t randomDuration = nextRandom(state);
	state->driftTimeLeft16 = (DRIFT_MIN_SECONDS * SECONDS_ONE) +
		(randomDuration % (DRIFT_EXTRA_SECONDS * SECONDS_ONE + 1U));
}

void microtonalReset(microtonalState_t *state, uint32_t voiceIndex)
{
	if (state == NULL)
		return;

	state->tuneCents = 0;
	state->driftDepthCents = 0;
	state->driftCents16 = 0;
	state->driftTargetCents16 = 0;
	state->driftTimeLeft16 = 0;
	state->driftPrngState = 0x6D2B79F5U ^ ((voiceIndex + 1U) * 0x9E3779B9U);
	if (state->driftPrngState == 0)
		state->driftPrngState = 1;
}

void microtonalSetTune(microtonalState_t *state, uint8_t parameter)
{
	if (state != NULL)
		state->tuneCents = (int16_t)parameter - 0x80;
}

void microtonalSetDriftDepth(microtonalState_t *state, uint8_t depthCents)
{
	if (state == NULL)
		return;

	state->driftDepthCents = depthCents;
	if (depthCents == 0)
	{
		state->driftCents16 = 0;
		state->driftTargetCents16 = 0;
		state->driftTimeLeft16 = 0;
		return;
	}

	const int32_t limit16 = (int32_t)depthCents * MICROTONAL_CENTS_ONE;
	if (state->driftCents16 > limit16)
		state->driftCents16 = limit16;
	else if (state->driftCents16 < -limit16)
		state->driftCents16 = -limit16;

	chooseDriftTarget(state);
}

bool microtonalAdvance(microtonalState_t *state, uint16_t bpm)
{
	if (state == NULL || state->driftDepthCents == 0)
		return false;

	if (bpm == 0)
		bpm = 1;

	/* One tracker tick is 2.5/BPM seconds. Keeping time in fixed point makes
	** the wander independent of audio sample rate and deterministic in both
	** live playback and offline rendering. */
	const uint32_t tickSeconds16 = ((5U * SECONDS_ONE) / 2U) / bpm;
	const int32_t previous = state->driftCents16;

	if (state->driftTimeLeft16 == 0)
		chooseDriftTarget(state);

	if (tickSeconds16 >= state->driftTimeLeft16)
	{
		state->driftCents16 = state->driftTargetCents16;
		chooseDriftTarget(state);
	}
	else
	{
		const int64_t distance =
			(int64_t)state->driftTargetCents16 - state->driftCents16;
		state->driftCents16 += (int32_t)((distance * tickSeconds16) /
			state->driftTimeLeft16);
		state->driftTimeLeft16 -= tickSeconds16;
	}

	return state->driftCents16 != previous;
}

bool microtonalEffectIsPitchExtension(uint8_t effect)
{
	return effect == TAPEHEAD_EFX_MICROTUNE ||
		effect == TAPEHEAD_EFX_MICRODRIFT;
}

bool microtonalLaneTypeIsValid(uint8_t type)
{
	return type == 0 || microtonalEffectIsPitchExtension(type);
}

bool microtonalPromoteLegacyEffect(note_t *event)
{
	if (event == NULL || !microtonalEffectIsPitchExtension(event->efx) ||
		event->tuneType != 0 || event->tuneData != 0)
		return false;

	event->tuneType = event->efx;
	event->tuneData = event->efxData;
	event->efx = event->efxData = 0;
	return true;
}

int32_t microtonalCurrentCents16(const microtonalState_t *state)
{
	if (state == NULL)
		return 0;

	return ((int32_t)state->tuneCents * MICROTONAL_CENTS_ONE) +
		state->driftCents16;
}

int64_t microtonalScaleDelta(int64_t baseDelta, int32_t cents16)
{
	if (baseDelta <= 0 || cents16 == 0)
		return baseDelta;

	const double cents = cents16 * (1.0 / MICROTONAL_CENTS_ONE);
	const double scaled = baseDelta * exp2(cents / 1200.0);
	if (scaled >= INT64_MAX)
		return INT64_MAX;

	const int64_t result = (int64_t)llround(scaled);
	return result < 1 ? 1 : result;
}
