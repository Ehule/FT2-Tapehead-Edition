#include "tapesister/ts_render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return EXIT_FAILURE; } } while (0)

int main(void)
{
    ts_rendered_sample rendered[TS_FIXTURE_COUNT] = { 0 };
    ts_render_report reports[TS_FIXTURE_COUNT];
    float min_rms = 1.0f, max_rms = 0.0f;
    float min_centroid = 1.0e9f, max_centroid = 0.0f;
    float min_zcr = 1.0f, max_zcr = 0.0f;

    CHECK(ts_fixture_recipe(TS_FIXTURE_COUNT) == NULL);
    for (size_t i = 0; i < TS_FIXTURE_COUNT; i++)
    {
        const ts_recipe *recipe = ts_fixture_recipe(i);
        ts_rendered_sample repeat = { 0 };
        ts_render_report repeat_report;
        CHECK(recipe != NULL && ts_recipe_validate(recipe));
        CHECK(ts_render(recipe, &rendered[i], &reports[i]));
        CHECK(ts_render(recipe, &repeat, &repeat_report));
        CHECK(rendered[i].frame_count == repeat.frame_count);
        CHECK(memcmp(rendered[i].samples, repeat.samples,
            repeat.frame_count * sizeof(float)) == 0);
        CHECK(reports[i].non_finite_count == 0);
        CHECK(reports[i].peak <= recipe->target_peak + 0.00001f);
        CHECK(reports[i].peak >= recipe->target_peak - 0.00001f);
        CHECK(reports[i].rms > 0.005f);
        CHECK(fabsf(reports[i].dc_offset) < 0.00001f);
        min_rms = fminf(min_rms, reports[i].rms);
        max_rms = fmaxf(max_rms, reports[i].rms);
        min_centroid = fminf(min_centroid, reports[i].spectral_centroid_hz);
        max_centroid = fmaxf(max_centroid, reports[i].spectral_centroid_hz);
        min_zcr = fminf(min_zcr, reports[i].zero_crossing_rate);
        max_zcr = fmaxf(max_zcr, reports[i].zero_crossing_rate);
        printf("%-18s peak %.3f rms %.3f crest %.2f zcr %.3f "
               "attack %.3f centroid %.0fHz dc %.8f\n", recipe->name,
            reports[i].peak, reports[i].rms, reports[i].crest_factor,
            reports[i].zero_crossing_rate, reports[i].attack_seconds,
            reports[i].spectral_centroid_hz, reports[i].dc_offset);
        ts_rendered_sample_free(&repeat);
    }

    CHECK(max_rms - min_rms > 0.08f);
    CHECK(max_centroid - min_centroid > 900.0f);
    CHECK(max_zcr - min_zcr > 0.08f);
    for (size_t a = 0; a < TS_FIXTURE_COUNT; a++)
        for (size_t b = a + 1; b < TS_FIXTURE_COUNT; b++)
            CHECK(fabsf(ts_waveform_correlation(&rendered[a], &rendered[b])) < 0.93f);

    ts_recipe invalid = *ts_fixture_recipe(0);
    invalid.delay_feedback = 1.1f;
    CHECK(!ts_recipe_validate(&invalid));
    for (size_t i = 0; i < TS_FIXTURE_COUNT; i++)
        ts_rendered_sample_free(&rendered[i]);
    puts("PASS: deterministic, finite, bounded, non-silent and distinct corpus");
    return EXIT_SUCCESS;
}
