#include "tapesister/ts_render.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TS_PI 3.14159265358979323846

typedef struct ts_rng { uint64_t state; } ts_rng;

static uint32_t rng_next(ts_rng *rng)
{
    uint64_t x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;
    return (uint32_t)((x * UINT64_C(2685821657736338717)) >> 32);
}

static float rng_bipolar(ts_rng *rng)
{
    return ((float)(rng_next(rng) >> 8) * (1.0f / 8388607.5f)) - 1.0f;
}

static float clampf(const float value, const float low, const float high)
{
    return value < low ? low : (value > high ? high : value);
}

static float amp_envelope(const ts_recipe *r, const float time,
    const float duration_seconds)
{
    const float release_start = fmaxf(r->attack_seconds + r->decay_seconds,
        duration_seconds - r->release_seconds);
    if (time < r->attack_seconds)
        return time / fmaxf(r->attack_seconds, 0.00001f);
    if (time < r->attack_seconds + r->decay_seconds)
    {
        const float t = (time - r->attack_seconds) / r->decay_seconds;
        return 1.0f + (r->sustain_level - 1.0f) * t;
    }
    if (time < release_start)
        return r->sustain_level;
    return r->sustain_level * clampf((duration_seconds - time) /
        fmaxf(duration_seconds - release_start, 0.00001f), 0.0f, 1.0f);
}

static float oscillator(const ts_recipe *r, const double phase,
    const double phase2, const float time)
{
    const double p = phase - floor(phase);
    const double p2 = phase2 - floor(phase2);
    float fundamental;
    switch (r->source)
    {
        case TS_SOURCE_SINE: fundamental = (float)sin(2.0 * TS_PI * p); break;
        case TS_SOURCE_TRIANGLE:
            fundamental = (float)(1.0 - (4.0 * fabs(p - 0.5))); break;
        case TS_SOURCE_SAW: fundamental = (float)((2.0 * p) - 1.0); break;
        case TS_SOURCE_PULSE:
            fundamental = p < (double)r->source_shape ? 1.0f : -1.0f; break;
        case TS_SOURCE_CLICK:
            fundamental = (float)(sin(2.0 * TS_PI * p) * exp(-time * 95.0f)); break;
        default: fundamental = 0.0f; break;
    }
    const float harmonic = (float)sin(2.0 * TS_PI * p2);
    return fundamental * (1.0f - 0.42f * r->harmonic_mix) +
        harmonic * (0.42f * r->harmonic_mix);
}

static float shape_sample(const ts_shaper_type type, float x, const float drive)
{
    x *= drive;
    if (type == TS_SHAPER_SOFT)
        return x / (1.0f + fabsf(x));
    if (type == TS_SHAPER_HARD)
        return clampf(x, -1.0f, 1.0f);
    for (int folds = 0; folds < 8 && (x > 1.0f || x < -1.0f); folds++)
        x = x > 1.0f ? 2.0f - x : -2.0f - x;
    return clampf(x, -1.0f, 1.0f);
}

bool ts_recipe_validate(const ts_recipe *r)
{
    return r != NULL && r->name != NULL &&
        r->sample_rate >= 8000 && r->sample_rate <= 192000 &&
        r->requested_frames >= 160 && r->requested_frames <= 1920000 &&
        r->root_midi_note <= 127 && r->fine_tune_cent100 >= -10000 &&
        r->fine_tune_cent100 <= 10000 &&
        ((float)r->requested_frames / (float)r->sample_rate) >= 0.02f &&
        ((float)r->requested_frames / (float)r->sample_rate) <= 10.0f &&
        ts_recipe_derive_root_hz(r->root_midi_note, r->fine_tune_cent100) >= 1.0f &&
        ts_recipe_derive_root_hz(r->root_midi_note, r->fine_tune_cent100) <= 20000.0f &&
        r->source <= TS_SOURCE_CLICK && r->noise_type <= TS_NOISE_METALLIC &&
        r->filter_mode <= TS_FILTER_NOTCH && r->shaper <= TS_SHAPER_FOLD &&
        r->source_shape >= 0.05f && r->source_shape <= 0.95f &&
        r->harmonic_mix >= 0.0f && r->harmonic_mix <= 1.0f &&
        r->noise_amount >= 0.0f && r->noise_amount <= 1.0f &&
        r->attack_seconds >= 0.0f && r->decay_seconds > 0.0f &&
        r->sustain_level >= 0.0f && r->sustain_level <= 1.0f &&
        r->release_seconds >= 0.0f && r->pitch_env_seconds > 0.0f &&
        r->filter_cutoff_hz >= 20.0f &&
        r->filter_cutoff_hz <= (float)r->sample_rate * 0.45f &&
        r->filter_resonance >= 0.0f && r->filter_resonance <= 0.95f &&
        r->drive >= 0.1f && r->drive <= 8.0f &&
        r->delay_seconds >= 0.0f && r->delay_seconds <= 1.0f &&
        r->delay_feedback >= 0.0f && r->delay_feedback <= 0.85f &&
        r->delay_mix >= 0.0f && r->delay_mix <= 1.0f &&
        r->reverb_decay >= 0.0f && r->reverb_decay <= 0.9f &&
        r->reverb_mix >= 0.0f && r->reverb_mix <= 1.0f &&
        r->finishing_mode <= TS_FINISH_FIXED_HEADROOM &&
        r->target_peak >= 0.1f && r->target_peak <= 0.95f &&
        r->fixed_gain_centidb >= -9600 && r->fixed_gain_centidb <= 0;
}

float ts_recipe_derive_root_hz(const uint8_t note, const int32_t fine_cent100)
{
    const double semitones = ((double)note - 69.0) +
        ((double)fine_cent100 / 10000.0);
    const double hz = 440.0 * exp2(semitones / 12.0);
    /* Renderer v1 compatibility: the Phase 1A corpus used frequencies written
     * to hundredths of a hertz. Tuning has one authority, then this conversion
     * applies that renderer-version quantization. */
    return (float)(floor(hz * 100.0 + 0.5) / 100.0);
}

bool ts_render(const ts_recipe *r, ts_rendered_sample *out,
    ts_render_report *report)
{
    if (!ts_recipe_validate(r) || out == NULL || report == NULL)
        return false;
    memset(out, 0, sizeof(*out));
    const size_t frames = r->requested_frames;
    const float duration_seconds = (float)frames / (float)r->sample_rate;
    const float root_hz = ts_recipe_derive_root_hz(r->root_midi_note,
        r->fine_tune_cent100);
    float *samples = calloc(frames, sizeof(*samples));
    size_t delay_len = (size_t)llround((double)r->delay_seconds * r->sample_rate);
    if (delay_len == 0) delay_len = 1;
    const size_t rev1_len = (size_t)((r->sample_rate * 149U) / 10000U) + 1U;
    const size_t rev2_len = (size_t)((r->sample_rate * 211U) / 10000U) + 1U;
    float *delay = calloc(delay_len, sizeof(*delay));
    float *rev1 = calloc(rev1_len, sizeof(*rev1));
    float *rev2 = calloc(rev2_len, sizeof(*rev2));
    if (samples == NULL || delay == NULL || rev1 == NULL || rev2 == NULL)
    {
        free(samples); free(delay); free(rev1); free(rev2);
        return false;
    }

    ts_rng rng = { r->seed };
    double phase = 0.0, phase2 = 0.0;
    float pink = 0.0f, low = 0.0f, band = 0.0f;
    float peak = 0.0f;
    uint32_t non_finite_count = 0;
    size_t dp = 0, rp1 = 0, rp2 = 0;
    for (size_t i = 0; i < frames; i++)
    {
        const float time = (float)i / (float)r->sample_rate;
        const float pitch_t = expf(-time / r->pitch_env_seconds);
        const float frequency = root_hz *
            exp2f((r->pitch_env_semitones * pitch_t) / 12.0f);
        phase += (double)frequency / r->sample_rate;
        phase2 += (double)(frequency * (2.0f + r->harmonic_mix * 1.7f)) /
            r->sample_rate;
        float noise = rng_bipolar(&rng);
        if (r->noise_type == TS_NOISE_PINKISH)
        {
            pink += 0.075f * (noise - pink);
            noise = pink * 2.8f;
        }
        else if (r->noise_type == TS_NOISE_METALLIC)
            noise = noise * ((rng_next(&rng) & 31U) < 7U ? 1.0f : -0.32f);

        float x = oscillator(r, phase, phase2, time);
        x = (x * (1.0f - r->noise_amount)) + (noise * r->noise_amount);
        x *= amp_envelope(r, time, duration_seconds);

        if (r->filter_enabled)
        {
            const float env = expf(-time / fmaxf(r->decay_seconds, 0.01f));
            const float cutoff = clampf(r->filter_cutoff_hz *
                exp2f(r->filter_env_octaves * env), 20.0f,
                (float)r->sample_rate * 0.42f);
            const float f = 2.0f * sinf((float)TS_PI * cutoff / (float)r->sample_rate);
            const float damping = 2.0f - (1.82f * r->filter_resonance);
            low += f * band;
            const float high = x - low - damping * band;
            band += f * high;
            const float notch = low + high;
            if (r->filter_mode == TS_FILTER_LOW_PASS) x = low;
            else if (r->filter_mode == TS_FILTER_BAND_PASS) x = band;
            else if (r->filter_mode == TS_FILTER_HIGH_PASS) x = high;
            else x = notch;
        }

        x = shape_sample(r->shaper, x, r->drive);
        const float delayed = delay[dp];
        delay[dp] = clampf(x + delayed * r->delay_feedback, -2.0f, 2.0f);
        dp = (dp + 1U) % delay_len;
        x = x * (1.0f - r->delay_mix) + delayed * r->delay_mix;

        const float rv1 = rev1[rp1], rv2 = rev2[rp2];
        rev1[rp1] = clampf(x + rv1 * r->reverb_decay, -2.0f, 2.0f);
        rev2[rp2] = clampf(x - rv2 * r->reverb_decay * 0.83f, -2.0f, 2.0f);
        rp1 = (rp1 + 1U) % rev1_len;
        rp2 = (rp2 + 1U) % rev2_len;
        x = x * (1.0f - r->reverb_mix) + (rv1 + rv2) * 0.5f * r->reverb_mix;
        if (!isfinite(x)) { non_finite_count++; x = 0.0f; }
        samples[i] = x;
    }

    double mean = 0.0;
    for (size_t i = 0; i < frames; i++) mean += samples[i];
    mean /= (double)frames;
    for (size_t i = 0; i < frames; i++)
    {
        samples[i] -= (float)mean;
        peak = fmaxf(peak, fabsf(samples[i]));
    }
    if (peak <= FLT_MIN)
    {
        free(samples); free(delay); free(rev1); free(rev2);
        return false;
    }
    float gain;
    if (r->finishing_mode == TS_FINISH_TARGET_PEAK)
        gain = r->target_peak / peak;
    else
        gain = powf(10.0f, (float)r->fixed_gain_centidb / 2000.0f);
    for (size_t i = 0; i < frames; i++) samples[i] *= gain;

    free(delay); free(rev1); free(rev2);
    out->samples = samples;
    out->frame_count = frames;
    out->sample_rate = r->sample_rate;
    ts_analyze(out, report);
    if (non_finite_count != 0)
    {
        report->non_finite_count = non_finite_count;
        ts_rendered_sample_free(out);
        return false;
    }
    if (r->finishing_mode == TS_FINISH_FIXED_HEADROOM && report->peak > 1.0f)
    {
        ts_rendered_sample_free(out);
        return false;
    }
    return report->non_finite_count == 0;
}

void ts_analyze(const ts_rendered_sample *sample, ts_render_report *report)
{
    memset(report, 0, sizeof(*report));
    double sum = 0.0, sum_sq = 0.0, weighted = 0.0, magnitude_sum = 0.0;
    float peak = 0.0f;
    size_t crossings = 0, peak_index = 0;
    for (size_t i = 0; i < sample->frame_count; i++)
    {
        const float x = sample->samples[i];
        if (!isfinite(x)) { report->non_finite_count++; continue; }
        sum += x; sum_sq += (double)x * x;
        if (fabsf(x) > peak) { peak = fabsf(x); peak_index = i; }
        if (i > 0 && ((x < 0.0f) != (sample->samples[i - 1] < 0.0f))) crossings++;
    }
    report->peak = peak;
    report->rms = (float)sqrt(sum_sq / (double)sample->frame_count);
    report->crest_factor = peak / fmaxf(report->rms, FLT_MIN);
    report->dc_offset = (float)(sum / (double)sample->frame_count);
    report->zero_crossing_rate = (float)crossings / (float)sample->frame_count;
    report->attack_seconds = (float)peak_index / (float)sample->sample_rate;

    const size_t window = sample->frame_count < 4096U ? sample->frame_count : 4096U;
    for (size_t bin = 1; bin < 256U && bin < window / 2U; bin++)
    {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < window; i++)
        {
            const double angle = (2.0 * TS_PI * (double)bin * (double)i) / (double)window;
            re += sample->samples[i] * cos(angle);
            im -= sample->samples[i] * sin(angle);
        }
        const double magnitude = sqrt(re * re + im * im);
        const double frequency = (double)bin * sample->sample_rate / (double)window;
        magnitude_sum += magnitude;
        weighted += magnitude * frequency;
    }
    report->spectral_centroid_hz = magnitude_sum > DBL_MIN ?
        (float)(weighted / magnitude_sum) : 0.0f;
}

float ts_waveform_correlation(const ts_rendered_sample *a,
    const ts_rendered_sample *b)
{
    const size_t count = a->frame_count < b->frame_count ?
        a->frame_count : b->frame_count;
    double aa = 0.0, bb = 0.0, ab = 0.0;
    for (size_t i = 0; i < count; i++)
    {
        aa += (double)a->samples[i] * a->samples[i];
        bb += (double)b->samples[i] * b->samples[i];
        ab += (double)a->samples[i] * b->samples[i];
    }
    const double divisor = sqrt(aa * bb);
    return divisor > DBL_MIN ? (float)(ab / divisor) : 0.0f;
}

void ts_rendered_sample_free(ts_rendered_sample *sample)
{
    if (sample == NULL) return;
    free(sample->samples);
    memset(sample, 0, sizeof(*sample));
}
