#include "tapesister/ts_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#define TEST_GETPID _getpid
#else
#include <unistd.h>
#define TEST_GETPID getpid
#endif

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return EXIT_FAILURE; } } while (0)

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint64_t hash_bytes(const void *data, size_t length)
{
    const uint8_t *bytes = data; uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < length; i++) { hash ^= bytes[i]; hash *= UINT64_C(1099511628211); }
    return hash;
}

static char *read_file(const char *path, size_t *length)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END); const long end = ftell(f); rewind(f);
    if (end < 0) { fclose(f); return NULL; }
    char *data = malloc((size_t)end + 1U);
    if (data == NULL) { fclose(f); return NULL; }
    *length = fread(data, 1, (size_t)end, f); fclose(f); data[*length] = '\0';
    return data;
}

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    fclose(f); return 1;
}

static int replace_once(char *text, const char *old, const char *replacement)
{
    char *at = strstr(text, old);
    if (at == NULL || strlen(old) != strlen(replacement)) return 0;
    memcpy(at, replacement, strlen(old)); return 1;
}

static char *replace_canonical_name(const char *canonical, size_t length,
    const char *replacement, size_t *newLength)
{
    const char marker[] = "\"name\": \"";
    const char *markerAt = strstr(canonical, marker);
    if (markerAt == NULL) return NULL;
    const char *nameStart = markerAt + sizeof (marker) - 1;
    const char *nameEnd = strchr(nameStart, '"');
    if (nameEnd == NULL) return NULL;
    const size_t prefix = (size_t)(nameStart - canonical);
    const size_t oldLength = (size_t)(nameEnd - nameStart);
    const size_t replacementLength = strlen(replacement);
    *newLength = length - oldLength + replacementLength;
    char *json = malloc(*newLength + 1);
    if (json == NULL) return NULL;
    memcpy(json, canonical, prefix);
    memcpy(json + prefix, replacement, replacementLength);
    memcpy(json + prefix + replacementLength, nameEnd,
        length - prefix - oldLength + 1);
    return json;
}

static int recipeNameBoundaryTests(const char *canonical, size_t length)
{
    ts_io_error error;
    char maximum[TS_RECIPE_NAME_MAX_BYTES + 1];
    memset(maximum, 'a', TS_RECIPE_NAME_MAX_BYTES);
    maximum[TS_RECIPE_NAME_MAX_BYTES] = '\0';
    size_t jsonLength;
    char *json = replace_canonical_name(canonical, length, maximum, &jsonLength);
    CHECK(json != NULL);
    ts_recipe loaded;
    CHECK(ts_recipe_parse(json, jsonLength, &loaded, &error) == TS_IO_OK);
    CHECK(strlen(loaded.name) == TS_RECIPE_NAME_MAX_BYTES);
    char *formatted1 = NULL, *formatted2 = NULL;
    size_t formattedLength1 = 0, formattedLength2 = 0;
    CHECK(ts_recipe_format(&loaded, &formatted1, &formattedLength1, &error) == TS_IO_OK);
    ts_recipe loadedAgain;
    CHECK(ts_recipe_parse(formatted1, formattedLength1, &loadedAgain, &error) == TS_IO_OK);
    CHECK(ts_recipe_format(&loadedAgain, &formatted2, &formattedLength2, &error) == TS_IO_OK);
    CHECK(formattedLength1 == formattedLength2 &&
        memcmp(formatted1, formatted2, formattedLength1) == 0);
    ts_recipe_loaded_dispose(&loadedAgain);
    ts_recipe_loaded_dispose(&loaded);
    free(formatted2); free(formatted1); free(json);

    char oversized[TS_RECIPE_NAME_MAX_BYTES + 2];
    memset(oversized, 'b', TS_RECIPE_NAME_MAX_BYTES + 1);
    oversized[TS_RECIPE_NAME_MAX_BYTES + 1] = '\0';
    json = replace_canonical_name(canonical, length, oversized, &jsonLength);
    CHECK(json != NULL);
    ts_recipe destination, original;
    memset(&destination, 0xA5, sizeof (destination));
    original = destination;
    CHECK(ts_recipe_parse(json, jsonLength, &destination, &error) == TS_IO_INVALID_VALUE);
    CHECK(strstr(error.message, "recipe name exceeds 127") != NULL);
    CHECK(memcmp(&destination, &original, sizeof (destination)) == 0);
    free(json);

    char longer[151];
    memset(longer, 'c', 150); longer[150] = '\0';
    json = replace_canonical_name(canonical, length, longer, &jsonLength);
    CHECK(json != NULL);
    CHECK(ts_recipe_parse(json, jsonLength, &destination, &error) == TS_IO_INVALID_VALUE);
    CHECK(strstr(error.message, "recipe name exceeds 127") != NULL);
    CHECK(memcmp(&destination, &original, sizeof (destination)) == 0);
    free(json);

    char beyondIntermediate[201];
    memset(beyondIntermediate, 'e', 200); beyondIntermediate[200] = '\0';
    json = replace_canonical_name(canonical, length, beyondIntermediate, &jsonLength);
    CHECK(json != NULL);
    CHECK(ts_recipe_parse(json, jsonLength, &destination, &error) == TS_IO_INVALID_VALUE);
    CHECK(strstr(error.message, "recipe name exceeds 127") != NULL);
    CHECK(memcmp(&destination, &original, sizeof (destination)) == 0);
    free(json);

    char escaped[137];
    memset(escaped, 'd', 124);
    memcpy(escaped + 124, "\\u00e9\\u00e9", 12);
    escaped[136] = '\0';
    json = replace_canonical_name(canonical, length, escaped, &jsonLength);
    CHECK(json != NULL);
    CHECK(ts_recipe_parse(json, jsonLength, &destination, &error) == TS_IO_INVALID_VALUE);
    CHECK(strstr(error.message, "recipe name exceeds 127") != NULL);
    CHECK(memcmp(&destination, &original, sizeof (destination)) == 0);
    free(json);
    return EXIT_SUCCESS;
}

static int rejection_tests(const char *canonical, size_t canonical_length)
{
    ts_recipe recipe; ts_io_error error;
    CHECK(ts_recipe_parse(canonical, canonical_length - 5U, &recipe, &error) == TS_IO_PARSE_ERROR);
    const char duplicate[] = "{\"schema\":\"tapesister.recipe\",\"schema\":\"tapesister.recipe\"}";
    CHECK(ts_recipe_parse(duplicate, strlen(duplicate), &recipe, &error) == TS_IO_DUPLICATE_FIELD);
    const char missing[] = "{\"schema\":\"tapesister.recipe\"}";
    CHECK(ts_recipe_parse(missing, strlen(missing), &recipe, &error) == TS_IO_MISSING_FIELD);
    const char unknown[] = "{\"bogus\":1}";
    CHECK(ts_recipe_parse(unknown, strlen(unknown), &recipe, &error) == TS_IO_UNKNOWN_FIELD);
    const char nested[] = "{\"schema\":{\"nested\":true}}";
    CHECK(ts_recipe_parse(nested, strlen(nested), &recipe, &error) == TS_IO_INVALID_VALUE);
    const char invalid_utf8[] = { '{', '"', (char)0xc0, (char)0x80, '"', ':', '1', '}' };
    CHECK(ts_recipe_parse(invalid_utf8, sizeof(invalid_utf8), &recipe, &error) == TS_IO_PARSE_ERROR);
    const char bad_surrogate[] = "{\"schema\":\"\\ud800x\"}";
    CHECK(ts_recipe_parse(bad_surrogate, strlen(bad_surrogate), &recipe, &error) == TS_IO_INVALID_VALUE);

    char *mutated = malloc(canonical_length + 1U); CHECK(mutated != NULL);
    memcpy(mutated, canonical, canonical_length + 1U);
    CHECK(replace_once(mutated, "\"sine\"", "\"xxxx\""));
    CHECK(ts_recipe_parse(mutated, canonical_length, &recipe, &error) == TS_IO_INVALID_VALUE);
    memcpy(mutated, canonical, canonical_length + 1U);
    CHECK(replace_once(mutated, "48000,", "00000,"));
    CHECK(ts_recipe_parse(mutated, canonical_length, &recipe, &error) == TS_IO_INVALID_VALUE);
    memcpy(mutated, canonical, canonical_length + 1U);
    CHECK(replace_once(mutated, "\"renderer_version\": 1", "\"renderer_version\": 2"));
    CHECK(ts_recipe_parse(mutated, canonical_length, &recipe, &error) == TS_IO_UNSUPPORTED_VERSION);
    free(mutated);

    char *large = malloc(TS_RECIPE_MAX_BYTES + 2U); CHECK(large != NULL);
    memset(large, ' ', TS_RECIPE_MAX_BYTES + 1U);
    CHECK(ts_recipe_parse(large, TS_RECIPE_MAX_BYTES + 1U, &recipe, &error) == TS_IO_TOO_LARGE);
    free(large); return EXIT_SUCCESS;
}

static int pcm_boundary_tests(void)
{
    struct { float input; int16_t expected; } cases[] =
    {
        { -2.0f, -32768 }, { -1.0f, -32768 },
        { -1.5f / 32768.0f, -2 }, { -0.5f / 32768.0f, -1 },
        { 0.0f, 0 }, { 0.5f / 32768.0f, 1 },
        { 1.5f / 32768.0f, 2 }, { 1.0f, 32767 }, { 2.0f, 32767 }
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int16_t got = 0; CHECK(ts_pcm16_encode_sample(cases[i].input, &got));
        CHECK(got == cases[i].expected);
    }
    int16_t ignored;
    CHECK(!ts_pcm16_encode_sample(NAN, &ignored));
    const float samples[] = { -1.0f, 0.5f / 32768.0f, 1.0f };
    ts_rendered_sample source = { (float *)samples, 3, 48000 };
    uint8_t *bytes = NULL; size_t length = 0; ts_io_error error;
    CHECK(ts_pcm16_encode(&source, &bytes, &length, &error) == TS_IO_OK);
    CHECK(length == 6 && bytes[0] == 0x00 && bytes[1] == 0x80 &&
        bytes[2] == 0x01 && bytes[3] == 0x00 && bytes[4] == 0xff && bytes[5] == 0x7f);
    free(bytes); return EXIT_SUCCESS;
}

int main(void)
{
    static const uint64_t phase1a_float_hash[TS_FIXTURE_COUNT] =
    {
        UINT64_C(0xd80d1f7174260eaf), UINT64_C(0x573ead53fbab09c2),
        UINT64_C(0x223576a324d39866), UINT64_C(0xa37c8d1785108a78),
        UINT64_C(0x3c6703aa16790769), UINT64_C(0x1e674968e7648e57)
    };
    static const uint64_t phase1a_pcm_hash[TS_FIXTURE_COUNT] =
    {
        UINT64_C(0x9c0de17ff14a0c0b), UINT64_C(0x7eb18d6cf66713b7),
        UINT64_C(0xdb61ae8f3f5e31e8), UINT64_C(0x949a5b9e1eb58314),
        UINT64_C(0xd9f2cafe77fa886a), UINT64_C(0xa31458dab1f90160)
    };
    ts_io_error error;
    char *first_canonical = NULL; size_t first_length = 0;
    CHECK(ts_recipe_format(ts_fixture_recipe(0), &first_canonical, &first_length, &error) == TS_IO_OK);
    CHECK(rejection_tests(first_canonical, first_length) == EXIT_SUCCESS);
    CHECK(recipeNameBoundaryTests(first_canonical, first_length) == EXIT_SUCCESS);
    CHECK(pcm_boundary_tests() == EXIT_SUCCESS);

    for (size_t i = 0; i < TS_FIXTURE_COUNT; i++)
    {
        const ts_recipe *baseline = ts_fixture_recipe(i);
        char path[512]; snprintf(path, sizeof(path), "%s/%s.tsr", TS_FIXTURE_DIR, baseline->name);
        size_t source_length = 0; char *source_text = read_file(path, &source_length);
        CHECK(source_text != NULL && source_length > 0 && source_text[source_length - 1] == '\n');
        ts_recipe loaded; CHECK(ts_recipe_load_file(path, &loaded, &error) == TS_IO_OK);
        CHECK(ts_recipe_derive_root_hz(loaded.root_midi_note, loaded.fine_tune_cent100) ==
            ts_recipe_derive_root_hz(baseline->root_midi_note, baseline->fine_tune_cent100));
        CHECK(loaded.requested_frames == baseline->requested_frames);
        char *formatted = NULL; size_t formatted_length = 0;
        CHECK(ts_recipe_format(&loaded, &formatted, &formatted_length, &error) == TS_IO_OK);
        CHECK(formatted_length == source_length && memcmp(formatted, source_text, source_length) == 0);

        ts_rendered_sample a = { 0 }, b = { 0 }; ts_render_report ar, br;
        CHECK(ts_render(baseline, &a, &ar) && ts_render(&loaded, &b, &br));
        CHECK(a.frame_count == b.frame_count);
        CHECK(memcmp(a.samples, b.samples, a.frame_count * sizeof(float)) == 0);
        uint8_t *apcm = NULL, *bpcm = NULL; size_t alen = 0, blen = 0;
        CHECK(ts_pcm16_encode(&a, &apcm, &alen, &error) == TS_IO_OK);
        CHECK(ts_pcm16_encode(&b, &bpcm, &blen, &error) == TS_IO_OK);
        CHECK(alen == blen && memcmp(apcm, bpcm, alen) == 0);
        CHECK(hash_bytes(a.samples, a.frame_count * sizeof(float)) == phase1a_float_hash[i]);
        CHECK(hash_bytes(apcm, alen) == phase1a_pcm_hash[i]);
        printf("PARITY %-18s float=%016llx pcm16=%016llx frames=%zu\n", baseline->name,
            (unsigned long long)hash_bytes(a.samples, a.frame_count * sizeof(float)),
            (unsigned long long)hash_bytes(apcm, alen), a.frame_count);
        free(apcm); free(bpcm); free(formatted); free(source_text);
        ts_rendered_sample_free(&a); ts_rendered_sample_free(&b);
        ts_recipe_loaded_dispose(&loaded);
    }

    ts_recipe max_seed = *ts_fixture_recipe(0); max_seed.seed = UINT64_MAX;
    char *seed_json = NULL; size_t seed_length = 0;
    CHECK(ts_recipe_format(&max_seed, &seed_json, &seed_length, &error) == TS_IO_OK);
    CHECK(strstr(seed_json, "\"ffffffffffffffff\"") != NULL);
    ts_recipe seed_loaded;
    CHECK(ts_recipe_parse(seed_json, seed_length, &seed_loaded, &error) == TS_IO_OK);
    CHECK(seed_loaded.seed == UINT64_MAX); ts_recipe_loaded_dispose(&seed_loaded); free(seed_json);
    ts_recipe zero_seed = *ts_fixture_recipe(0); zero_seed.seed = 0;
    CHECK(ts_recipe_format(&zero_seed, &seed_json, &seed_length, &error) == TS_IO_OK);
    CHECK(strstr(seed_json, "\"0000000000000000\"") != NULL);
    CHECK(ts_recipe_parse(seed_json, seed_length, &seed_loaded, &error) == TS_IO_OK);
    CHECK(seed_loaded.seed == 0); ts_recipe_loaded_dispose(&seed_loaded); free(seed_json);
    char *escaped_name = NULL; size_t escaped_length = 0;
    CHECK(ts_recipe_format(ts_fixture_recipe(0), &escaped_name, &escaped_length, &error) == TS_IO_OK);
    char *name_at = strstr(escaped_name, "clean_sustain"); CHECK(name_at != NULL);
    const char escaped_value[] = "clean_\\u0073ustain";
    const size_t extra = strlen(escaped_value) - strlen("clean_sustain");
    char *escaped_json = malloc(escaped_length + extra + 1U); CHECK(escaped_json != NULL);
    const size_t prefix = (size_t)(name_at - escaped_name);
    memcpy(escaped_json, escaped_name, prefix);
    memcpy(escaped_json + prefix, escaped_value, strlen(escaped_value));
    memcpy(escaped_json + prefix + strlen(escaped_value), name_at + strlen("clean_sustain"),
        escaped_length - prefix - strlen("clean_sustain") + 1U);
    CHECK(ts_recipe_parse(escaped_json, escaped_length + extra, &seed_loaded, &error) == TS_IO_OK);
    CHECK(strcmp(seed_loaded.name, "clean_sustain") == 0);
    ts_recipe_loaded_dispose(&seed_loaded); free(escaped_json); free(escaped_name);
    ts_recipe tuning_edge = *ts_fixture_recipe(0); tuning_edge.root_midi_note = 0;
    tuning_edge.fine_tune_cent100 = -10000; CHECK(ts_recipe_validate(&tuning_edge));
    tuning_edge.root_midi_note = 127; tuning_edge.fine_tune_cent100 = 10000;
    CHECK(ts_recipe_validate(&tuning_edge));

    ts_recipe fixed = *ts_fixture_recipe(0); fixed.finishing_mode = TS_FINISH_FIXED_HEADROOM;
    fixed.fixed_gain_centidb = -600;
    ts_rendered_sample fixed_sample = { 0 }; ts_render_report fixed_report;
    CHECK(ts_render(&fixed, &fixed_sample, &fixed_report));
    CHECK(fixed_report.peak < 1.0f); ts_rendered_sample_free(&fixed_sample);
    ts_recipe clipping = *ts_fixture_recipe(2);
    clipping.finishing_mode = TS_FINISH_FIXED_HEADROOM; clipping.fixed_gain_centidb = 0;
    clipping.delay_feedback = 0.85f; clipping.delay_mix = 1.0f;
    clipping.reverb_decay = 0.9f; clipping.reverb_mix = 1.0f;
    ts_rendered_sample clipped = { 0 }; ts_render_report clipped_report;
    CHECK(!ts_render(&clipping, &clipped, &clipped_report));

    const char *wav_path = "/tmp/tapesister_phase1b.wav";
    const char *recipe_path = "/tmp/tapesister_phase1b.tsr";
    remove(wav_path); remove(recipe_path);
    ts_rendered_sample rendered = { 0 }; ts_render_report report;
    CHECK(ts_render(ts_fixture_recipe(1), &rendered, &report));
    CHECK(ts_wav_save_file(wav_path, &rendered, &error) == TS_IO_OK);
    size_t wav_length = 0; char *wav = read_file(wav_path, &wav_length); CHECK(wav != NULL);
    CHECK(wav_length == 44U + rendered.frame_count * 2U);
    CHECK(memcmp(wav, "RIFF", 4) == 0 && get_u32((uint8_t *)wav + 4) == wav_length - 8U);
    CHECK(memcmp(wav + 8, "WAVEfmt ", 8) == 0 && get_u32((uint8_t *)wav + 16) == 16U);
    CHECK(get_u16((uint8_t *)wav + 20) == 1 && get_u16((uint8_t *)wav + 22) == 1 &&
        get_u32((uint8_t *)wav + 24) == 48000U && get_u32((uint8_t *)wav + 28) == 96000U);
    CHECK(get_u16((uint8_t *)wav + 32) == 2 && get_u16((uint8_t *)wav + 34) == 16 &&
        memcmp(wav + 36, "data", 4) == 0);
    uint8_t *expected = NULL; size_t expected_length = 0;
    CHECK(ts_pcm16_encode(&rendered, &expected, &expected_length, &error) == TS_IO_OK);
    CHECK(get_u32((uint8_t *)wav + 40) == expected_length &&
        memcmp(wav + 44, expected, expected_length) == 0);
    printf("WAV frames=%zu rate=%u data=%zu RIFF=%zu\n", rendered.frame_count,
        rendered.sample_rate, expected_length, wav_length);

    CHECK(ts_wav_save_file(wav_path, &rendered, &error) == TS_IO_EXISTS);
    size_t collision_length = 0; char *collision = read_file(wav_path, &collision_length);
    CHECK(collision_length == wav_length && memcmp(collision, wav, wav_length) == 0);
    CHECK(ts_recipe_save_file_test(recipe_path, ts_fixture_recipe(0), 10, &error) == TS_IO_WRITE_FAILED);
    FILE *absent = fopen(recipe_path, "rb"); CHECK(absent == NULL);
    const char *failed_wav = "/tmp/tapesister_phase1b_failed.wav"; remove(failed_wav);
    CHECK(ts_wav_save_file_test(failed_wav, &rendered, 20, &error) == TS_IO_WRITE_FAILED);
    absent = fopen(failed_wav, "rb"); CHECK(absent == NULL);
    CHECK(ts_recipe_save_file(recipe_path, ts_fixture_recipe(0), &error) == TS_IO_OK);
    size_t preserved_length = 0; char *preserved = read_file(recipe_path, &preserved_length);
    CHECK(ts_recipe_save_file(recipe_path, ts_fixture_recipe(1), &error) == TS_IO_EXISTS);
    size_t after_length = 0; char *after = read_file(recipe_path, &after_length);
    CHECK(after_length == preserved_length && memcmp(after, preserved, after_length) == 0);

    const char *pair_recipe = "/tmp/tapesister_phase1b_pair.tsr";
    const char *pair_wav = "/tmp/tapesister_phase1b_pair.wav";
    remove(pair_recipe); remove(pair_wav);
    CHECK(ts_bake_pair_files_test(pair_recipe, pair_wav, ts_fixture_recipe(1),
        &rendered, TS_IO_NO_FAILURE, 12, &error) == TS_IO_WRITE_FAILED);
    absent = fopen(pair_recipe, "rb"); CHECK(absent == NULL);
    absent = fopen(pair_wav, "rb"); CHECK(absent == NULL);
    CHECK(ts_bake_pair_files(pair_recipe, pair_wav, ts_fixture_recipe(1),
        &rendered, &error) == TS_IO_OK);
    CHECK(ts_bake_pair_files(pair_recipe, pair_wav, ts_fixture_recipe(1),
        &rendered, &error) == TS_IO_EXISTS);
    size_t old_pair_recipe_length = 0, old_pair_wav_length = 0;
    char *old_pair_recipe = read_file(pair_recipe, &old_pair_recipe_length);
    char *old_pair_wav = read_file(pair_wav, &old_pair_wav_length);
    CHECK(old_pair_recipe != NULL && old_pair_wav != NULL);
    for (unsigned int phase = 1; phase <= 3; phase++)
    {
        CHECK(ts_bake_pair_replace_files_test(pair_recipe, pair_wav,
            ts_fixture_recipe(2), &rendered, phase, &error) == TS_IO_RENAME_FAILED);
        size_t check_recipe_length = 0, check_wav_length = 0;
        char *check_recipe = read_file(pair_recipe, &check_recipe_length);
        char *check_wav = read_file(pair_wav, &check_wav_length);
        CHECK(check_recipe != NULL && check_wav != NULL);
        CHECK(check_recipe_length == old_pair_recipe_length &&
            memcmp(check_recipe, old_pair_recipe, check_recipe_length) == 0);
        CHECK(check_wav_length == old_pair_wav_length &&
            memcmp(check_wav, old_pair_wav, check_wav_length) == 0);
        free(check_recipe); free(check_wav);
    }
    CHECK(ts_bake_pair_replace_files(pair_recipe, pair_wav,
        ts_fixture_recipe(2), &rendered, &error) == TS_IO_OK);
    ts_recipe replaced_recipe;
    CHECK(ts_recipe_load_file(pair_recipe, &replaced_recipe, &error) == TS_IO_OK);
    CHECK(strcmp(replaced_recipe.name, ts_fixture_recipe(2)->name) == 0);
    ts_recipe_loaded_dispose(&replaced_recipe);
    CHECK(remove(pair_wav) == 0);
    CHECK(ts_bake_pair_replace_files(pair_recipe, pair_wav,
        ts_fixture_recipe(3), &rendered, &error) == TS_IO_OK);
    CHECK(remove(pair_recipe) == 0);
    CHECK(ts_bake_pair_replace_files(pair_recipe, pair_wav,
        ts_fixture_recipe(4), &rendered, &error) == TS_IO_OK);
    CHECK(ts_recipe_replace_file(recipe_path, ts_fixture_recipe(2), &error) == TS_IO_OK);
    CHECK(ts_recipe_load_file(recipe_path, &replaced_recipe, &error) == TS_IO_OK);
    CHECK(strcmp(replaced_recipe.name, ts_fixture_recipe(2)->name) == 0);
    ts_recipe_loaded_dispose(&replaced_recipe);

    /* Replacement failures distinguish complete rollback from retained-backup
     * recovery. The injected restoration failures use the first unique sibling. */
    CHECK(ts_recipe_replace_file_test(recipe_path, ts_fixture_recipe(3),
        true, false, &error) == TS_IO_RENAME_FAILED);
    char recipe_backup[1024];
    snprintf(recipe_backup, sizeof recipe_backup, "%s.backup.%ld.0",
        recipe_path, (long)TEST_GETPID());
    CHECK(ts_recipe_replace_file_test(recipe_path, ts_fixture_recipe(3),
        true, true, &error) == TS_IO_ROLLBACK_FAILED);
    CHECK(strstr(error.message, recipe_backup) != NULL);
    CHECK(file_exists(recipe_backup));
    remove(recipe_backup);
    CHECK(ts_recipe_save_file(recipe_path, ts_fixture_recipe(2), &error) == TS_IO_OK);

    for (unsigned int phase = 4; phase <= 6; ++phase) {
        char rb[1024], wb[1024];
        snprintf(rb, sizeof rb, "%s.backup.%ld.0", pair_recipe,
            (long)TEST_GETPID());
        snprintf(wb, sizeof wb, "%s.backup.%ld.0", pair_wav,
            (long)TEST_GETPID());
        CHECK(ts_bake_pair_replace_files_test(pair_recipe, pair_wav,
            ts_fixture_recipe(5), &rendered, phase, &error) ==
            TS_IO_ROLLBACK_FAILED);
        if (phase == 4 || phase == 6) CHECK(file_exists(rb));
        if (phase == 5 || phase == 6) CHECK(file_exists(wb));
        /* The restoration not selected for failure was still attempted. */
        if (phase == 4) CHECK(file_exists(pair_wav));
        if (phase == 5) CHECK(file_exists(pair_recipe));
        remove(pair_recipe); remove(pair_wav); remove(rb); remove(wb);
        CHECK(ts_bake_pair_files(pair_recipe, pair_wav, ts_fixture_recipe(2),
            &rendered, &error) == TS_IO_OK);
    }

    free(old_pair_recipe); free(old_pair_wav); free(after); free(preserved);
    free(collision); free(expected); free(wav);
    free(first_canonical); ts_rendered_sample_free(&rendered);
    remove(wav_path); remove(recipe_path); remove(failed_wav);
    remove(pair_recipe); remove(pair_wav);
    puts("PASS: canonical recipes, PCM16, atomic files and WAV boundary");
    return EXIT_SUCCESS;
}
