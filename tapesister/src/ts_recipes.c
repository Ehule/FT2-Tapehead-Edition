#include "tapesister/ts_render.h"

/* Phase 1A's recipe corpus is compiled data until versioned recipe I/O lands
 * in Phase 1B. Every value is explicit so the fixtures remain reproducible. */
static const ts_recipe fixtures[TS_FIXTURE_COUNT] =
{
    { "clean_sustain", UINT64_C(0x1020304050607080), 48000, 57600, 57, 0,
      TS_SOURCE_SINE, 0.5f, 0.18f, TS_NOISE_WHITE, 0.0f,
      0.025f, 0.18f, 0.82f, 0.28f, 0.0f, 0.1f,
      true, TS_FILTER_LOW_PASS, 5800.0f, 0.10f, 0.0f,
      TS_SHAPER_SOFT, 1.05f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, TS_FINISH_TARGET_PEAK, 0.82f, -600 },

    { "percussive_pluck", UINT64_C(0x2131415161718191), 48000, 20160, 52, 0,
      TS_SOURCE_CLICK, 0.18f, 0.55f, TS_NOISE_WHITE, 0.16f,
      0.0005f, 0.035f, 0.0f, 0.12f, 19.0f, 0.055f,
      true, TS_FILTER_BAND_PASS, 1800.0f, 0.64f, 2.1f,
      TS_SHAPER_SOFT, 1.7f, 0.028f, 0.19f, 0.12f, 0.22f, 0.04f, TS_FINISH_TARGET_PEAK, 0.88f, -600 },

    { "noisy_metal", UINT64_C(0x32425262728292A2), 48000, 32640, 61, 0,
      TS_SOURCE_PULSE, 0.23f, 0.72f, TS_NOISE_METALLIC, 0.72f,
      0.001f, 0.095f, 0.08f, 0.31f, 7.0f, 0.08f,
      true, TS_FILTER_HIGH_PASS, 2100.0f, 0.52f, 1.4f,
      TS_SHAPER_FOLD, 3.4f, 0.013f, 0.51f, 0.18f, 0.68f, 0.12f, TS_FINISH_TARGET_PEAK, 0.86f, -600 },

    { "unstable_drone", UINT64_C(0x435363738393A3B3), 48000, 74400, 45, 0,
      TS_SOURCE_TRIANGLE, 0.67f, 0.46f, TS_NOISE_PINKISH, 0.13f,
      0.16f, 0.32f, 0.63f, 0.46f, -4.5f, 0.9f,
      true, TS_FILTER_NOTCH, 1260.0f, 0.71f, -0.8f,
      TS_SHAPER_SOFT, 1.8f, 0.071f, 0.44f, 0.24f, 0.79f, 0.28f, TS_FINISH_TARGET_PEAK, 0.78f, -600 },

    { "digital_bass", UINT64_C(0x5464748494A4B4C4), 48000, 39360, 40, 0,
      TS_SOURCE_SAW, 0.41f, 0.84f, TS_NOISE_METALLIC, 0.04f,
      0.004f, 0.11f, 0.58f, 0.22f, 12.0f, 0.09f,
      true, TS_FILTER_LOW_PASS, 720.0f, 0.57f, 2.6f,
      TS_SHAPER_HARD, 4.8f, 0.0f, 0.0f, 0.0f, 0.18f, 0.0f, TS_FINISH_TARGET_PEAK, 0.90f, -600 },

    { "spacious_decay", UINT64_C(0x65758595A5B5C5D5), 48000, 86400, 64, 0,
      TS_SOURCE_SINE, 0.5f, 0.61f, TS_NOISE_WHITE, 0.025f,
      0.008f, 0.24f, 0.18f, 0.82f, 5.0f, 0.31f,
      true, TS_FILTER_LOW_PASS, 4100.0f, 0.28f, -1.2f,
      TS_SHAPER_SOFT, 1.35f, 0.19f, 0.57f, 0.31f, 0.86f, 0.47f, TS_FINISH_TARGET_PEAK, 0.80f, -600 }
};

const ts_recipe *ts_fixture_recipe(const size_t index)
{
    return index < TS_FIXTURE_COUNT ? &fixtures[index] : NULL;
}
