#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "ft2_microtonal.h"

static void test_tune_byte_mapping_and_persistence(void)
{
	microtonalState_t state;
	microtonalReset(&state, 0);
	assert(microtonalCurrentCents16(&state) == 0);

	microtonalSetTune(&state, 0x80);
	assert(state.tuneCents == 0);
	microtonalSetTune(&state, 0x87);
	assert(state.tuneCents == 7);
	assert(microtonalCurrentCents16(&state) == 7 * MICROTONAL_CENTS_ONE);

	/* Rows and note retriggers do not call a tune reset. */
	for (int rowOrRetrigger = 0; rowOrRetrigger < 64; rowOrRetrigger++)
		assert(state.tuneCents == 7);

	microtonalSetTune(&state, 0x79);
	assert(state.tuneCents == -7);
	microtonalSetTune(&state, 0x00);
	assert(state.tuneCents == -128);
	microtonalSetTune(&state, 0xFF);
	assert(state.tuneCents == 127);
	microtonalSetTune(&state, 0x80);
	assert(state.tuneCents == 0);
}

static void test_delta_scaling_and_modulation_composition(void)
{
	const int64_t nominal = INT64_C(1000000000);
	assert(microtonalScaleDelta(nominal, 0) == nominal);

	const int64_t sharp = microtonalScaleDelta(nominal,
		7 * MICROTONAL_CENTS_ONE);
	const int64_t flat = microtonalScaleDelta(nominal,
		-7 * MICROTONAL_CENTS_ONE);
	assert(sharp > nominal);
	assert(flat < nominal);
	assert(fabs((double)sharp / nominal - exp2(7.0 / 1200.0)) < 1e-9);
	assert(fabs((double)flat / nominal - exp2(-7.0 / 1200.0)) < 1e-9);

	/* Vibrato and portamento change the base delta first. The same persistent
	** cent ratio is then applied around either modulated result. */
	const int64_t vibratoBase = nominal + 2500000;
	const int64_t portamentoBase = nominal - 5000000;
	const double vibratoRatio = (double)microtonalScaleDelta(vibratoBase,
		-11 * MICROTONAL_CENTS_ONE) / vibratoBase;
	const double portamentoRatio = (double)microtonalScaleDelta(portamentoBase,
		-11 * MICROTONAL_CENTS_ONE) / portamentoBase;
	assert(fabs(vibratoRatio - portamentoRatio) < 1e-9);
	assert(microtonalScaleDelta(1, -383 * MICROTONAL_CENTS_ONE) >= 1);
	assert(microtonalScaleDelta(INT64_MAX / 2,
		382 * MICROTONAL_CENTS_ONE) > 0);
}

static void test_drift_disabled_small_bounded_and_deterministic(void)
{
	microtonalState_t first, replay, otherVoice;
	microtonalReset(&first, 3);
	microtonalReset(&replay, 3);
	microtonalReset(&otherVoice, 4);

	for (int tick = 0; tick < 100; tick++)
	{
		assert(!microtonalAdvance(&first, 125));
		assert(first.driftCents16 == 0);
	}

	microtonalSetTune(&first, 0x75); /* -11 cents */
	microtonalSetTune(&replay, 0x75);
	microtonalSetTune(&otherVoice, 0x75);
	microtonalSetDriftDepth(&first, 4);
	microtonalSetDriftDepth(&replay, 4);
	microtonalSetDriftDepth(&otherVoice, 4);

	bool voicesDiverged = false;
	for (int tick = 0; tick < 4000; tick++)
	{
		microtonalAdvance(&first, 125);
		microtonalAdvance(&replay, 125);
		microtonalAdvance(&otherVoice, 125);

		assert(first.tuneCents == replay.tuneCents);
		assert(first.driftDepthCents == replay.driftDepthCents);
		assert(first.driftCents16 == replay.driftCents16);
		assert(first.driftTargetCents16 == replay.driftTargetCents16);
		assert(first.driftTimeLeft16 == replay.driftTimeLeft16);
		assert(first.driftPrngState == replay.driftPrngState);
		assert(first.driftCents16 >= -4 * MICROTONAL_CENTS_ONE);
		assert(first.driftCents16 <= 4 * MICROTONAL_CENTS_ONE);
		const int32_t combined = microtonalCurrentCents16(&first);
		assert(combined >= -15 * MICROTONAL_CENTS_ONE);
		assert(combined <= -7 * MICROTONAL_CENTS_ONE);
		if (first.driftCents16 != otherVoice.driftCents16)
			voicesDiverged = true;
	}
	assert(voicesDiverged);

	microtonalSetDriftDepth(&first, 1);
	for (int tick = 0; tick < 1000; tick++)
	{
		microtonalAdvance(&first, 32);
		assert(first.driftCents16 >= -MICROTONAL_CENTS_ONE);
		assert(first.driftCents16 <= MICROTONAL_CENTS_ONE);
	}

	microtonalSetDriftDepth(&first, 0);
	assert(first.driftCents16 == 0);
	assert(microtonalCurrentCents16(&first) == -11 * MICROTONAL_CENTS_ONE);
}

static void test_effect_namespace(void)
{
	assert(microtonalEffectIsPitchExtension(TAPEHEAD_EFX_MICROTUNE));
	assert(microtonalEffectIsPitchExtension(TAPEHEAD_EFX_MICRODRIFT));
	assert(!microtonalEffectIsPitchExtension(0x0D));
	assert(!microtonalEffectIsPitchExtension(0x1D)); /* Txx tremor */
	assert(!microtonalEffectIsPitchExtension(0x23)); /* Zxx FastTracks */
}

int main(void)
{
	test_tune_byte_mapping_and_persistence();
	test_delta_scaling_and_modulation_composition();
	test_drift_disabled_small_bounded_and_deterministic();
	test_effect_namespace();
	puts("Microtonal Tune/Drift core tests passed.");
	return 0;
}
