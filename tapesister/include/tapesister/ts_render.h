#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TS_RENDERER_VERSION 1U
#define TS_FIXTURE_COUNT 6U

typedef enum ts_source_type
{
    TS_SOURCE_SINE,
    TS_SOURCE_TRIANGLE,
    TS_SOURCE_SAW,
    TS_SOURCE_PULSE,
    TS_SOURCE_CLICK
} ts_source_type;

typedef enum ts_noise_type
{
    TS_NOISE_WHITE,
    TS_NOISE_PINKISH,
    TS_NOISE_METALLIC
} ts_noise_type;

typedef enum ts_filter_mode
{
    TS_FILTER_LOW_PASS,
    TS_FILTER_BAND_PASS,
    TS_FILTER_HIGH_PASS,
    TS_FILTER_NOTCH
} ts_filter_mode;

typedef enum ts_shaper_type
{
    TS_SHAPER_SOFT,
    TS_SHAPER_HARD,
    TS_SHAPER_FOLD
} ts_shaper_type;

typedef enum ts_finishing_mode
{
    TS_FINISH_TARGET_PEAK,
    TS_FINISH_FIXED_HEADROOM
} ts_finishing_mode;

typedef struct ts_recipe
{
    const char *name;
    uint64_t seed;
    uint32_t sample_rate;
    uint32_t requested_frames;
    uint8_t root_midi_note;
    int32_t fine_tune_cent100;

    ts_source_type source;
    float source_shape;
    float harmonic_mix;
    ts_noise_type noise_type;
    float noise_amount;

    float attack_seconds;
    float decay_seconds;
    float sustain_level;
    float release_seconds;
    float pitch_env_semitones;
    float pitch_env_seconds;

    bool filter_enabled;
    ts_filter_mode filter_mode;
    float filter_cutoff_hz;
    float filter_resonance;
    float filter_env_octaves;

    ts_shaper_type shaper;
    float drive;
    float delay_seconds;
    float delay_feedback;
    float delay_mix;
    float reverb_decay;
    float reverb_mix;
    ts_finishing_mode finishing_mode;
    float target_peak;
    int32_t fixed_gain_centidb;
} ts_recipe;

typedef struct ts_rendered_sample
{
    float *samples;
    size_t frame_count;
    uint32_t sample_rate;
} ts_rendered_sample;

typedef struct ts_render_report
{
    float peak;
    float rms;
    float crest_factor;
    float dc_offset;
    float zero_crossing_rate;
    float attack_seconds;
    float spectral_centroid_hz;
    uint32_t non_finite_count;
} ts_render_report;

bool ts_recipe_validate(const ts_recipe *recipe);
float ts_recipe_derive_root_hz(uint8_t root_midi_note,
    int32_t fine_tune_cent100);
bool ts_render(const ts_recipe *recipe, ts_rendered_sample *output,
    ts_render_report *report);
void ts_rendered_sample_free(ts_rendered_sample *sample);
void ts_analyze(const ts_rendered_sample *sample, ts_render_report *report);
float ts_waveform_correlation(const ts_rendered_sample *a,
    const ts_rendered_sample *b);

const ts_recipe *ts_fixture_recipe(size_t index);
