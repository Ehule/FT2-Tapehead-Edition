#pragma once

#include "tapesister/ts_render.h"

#include <stddef.h>
#include <stdint.h>

#define TS_RECIPE_MAX_BYTES 65536U
#define TS_RECIPE_NAME_MAX_BYTES 127U
#define TS_IO_NO_FAILURE SIZE_MAX

typedef enum ts_io_status
{
    TS_IO_OK,
    TS_IO_INVALID_ARGUMENT,
    TS_IO_EXISTS,
    TS_IO_OPEN_FAILED,
    TS_IO_WRITE_FAILED,
    TS_IO_RENAME_FAILED,
    TS_IO_ROLLBACK_FAILED,
    TS_IO_TOO_LARGE,
    TS_IO_PARSE_ERROR,
    TS_IO_DUPLICATE_FIELD,
    TS_IO_MISSING_FIELD,
    TS_IO_UNKNOWN_FIELD,
    TS_IO_UNSUPPORTED_VERSION,
    TS_IO_INVALID_VALUE
} ts_io_status;

typedef struct ts_io_error
{
    ts_io_status status;
    size_t offset;
    char message[160];
} ts_io_error;

ts_io_status ts_recipe_load_file(const char *path, ts_recipe *recipe,
    ts_io_error *error);
ts_io_status ts_recipe_parse(const char *json, size_t length,
    ts_recipe *recipe, ts_io_error *error);
ts_io_status ts_recipe_format(const ts_recipe *recipe, char **json,
    size_t *length, ts_io_error *error);
ts_io_status ts_recipe_save_file(const char *path, const ts_recipe *recipe,
    ts_io_error *error);
/* Releases the name owned by a recipe returned from load/parse. Do not call
 * this for compiled fixture recipes or caller-owned recipe names. */
void ts_recipe_loaded_dispose(ts_recipe *recipe);

bool ts_pcm16_encode_sample(float sample, int16_t *encoded);
ts_io_status ts_pcm16_encode(const ts_rendered_sample *sample,
    uint8_t **bytes, size_t *length, ts_io_error *error);
ts_io_status ts_wav_save_file(const char *path,
    const ts_rendered_sample *sample, ts_io_error *error);
ts_io_status ts_bake_pair_files(const char *recipe_path, const char *wav_path,
    const ts_recipe *recipe, const ts_rendered_sample *sample,
    ts_io_error *error);
ts_io_status ts_recipe_replace_file(const char *path, const ts_recipe *recipe,
    ts_io_error *error);
ts_io_status ts_recipe_replace_file_test(const char *path,
    const ts_recipe *recipe, bool fail_publish, bool fail_restore,
    ts_io_error *error);
ts_io_status ts_bake_pair_replace_files(const char *recipe_path,
    const char *wav_path, const ts_recipe *recipe,
    const ts_rendered_sample *sample, ts_io_error *error);
/* fail_phase 1..3 fails publication; 4 recipe restore, 5 WAV restore, 6 both. */
ts_io_status ts_bake_pair_replace_files_test(const char *recipe_path,
    const char *wav_path, const ts_recipe *recipe,
    const ts_rendered_sample *sample, unsigned int fail_phase,
    ts_io_error *error);

/* Deterministic failure injection used by native I/O cleanup tests. */
ts_io_status ts_recipe_save_file_test(const char *path,
    const ts_recipe *recipe, size_t fail_after, ts_io_error *error);
ts_io_status ts_wav_save_file_test(const char *path,
    const ts_rendered_sample *sample, size_t fail_after, ts_io_error *error);
ts_io_status ts_bake_pair_files_test(const char *recipe_path,
    const char *wav_path, const ts_recipe *recipe,
    const ts_rendered_sample *sample, size_t recipe_fail_after,
    size_t wav_fail_after, ts_io_error *error);
