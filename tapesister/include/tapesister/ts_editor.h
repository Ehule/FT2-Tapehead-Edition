#pragma once

#include "tapesister/ts_io.h"

#define TS_EDITOR_HISTORY_CAPACITY 128U
#define TS_EDITOR_PAGE_COUNT 6U

typedef enum ts_parameter_page { TS_PAGE_SOURCE, TS_PAGE_CONTOUR, TS_PAGE_FILTER,
  TS_PAGE_COLOR, TS_PAGE_SPACE, TS_PAGE_SAMPLE } ts_parameter_page;
typedef enum ts_parameter_type { TS_PARAM_CONTINUOUS, TS_PARAM_INTEGER,
  TS_PARAM_ENUM, TS_PARAM_BOOLEAN, TS_PARAM_SEED, TS_PARAM_NAME,
  TS_PARAM_SAMPLE_RATE } ts_parameter_type;
typedef enum ts_parameter_mapping { TS_MAP_LINEAR, TS_MAP_LOG, TS_MAP_CATEGORY } ts_parameter_mapping;

typedef enum ts_parameter_id {
  TS_P_SOURCE, TS_P_SOURCE_SHAPE, TS_P_HARMONIC_MIX, TS_P_NOISE_TYPE,
  TS_P_NOISE_AMOUNT, TS_P_SEED, TS_P_ATTACK, TS_P_DECAY, TS_P_SUSTAIN,
  TS_P_RELEASE, TS_P_PITCH_ENV_AMOUNT, TS_P_PITCH_ENV_DECAY,
  TS_P_FILTER_ENABLED, TS_P_FILTER_MODE, TS_P_FILTER_CUTOFF,
  TS_P_FILTER_RESONANCE, TS_P_FILTER_ENV_AMOUNT, TS_P_SHAPER, TS_P_DRIVE,
  TS_P_DELAY_TIME, TS_P_DELAY_FEEDBACK, TS_P_DELAY_MIX, TS_P_REVERB_DECAY,
  TS_P_REVERB_MIX, TS_P_NAME, TS_P_ROOT_NOTE, TS_P_FINE_TUNE,
  TS_P_SAMPLE_RATE, TS_P_RENDER_FRAMES, TS_P_FINISHING_MODE,
  TS_P_TARGET_PEAK, TS_P_FIXED_GAIN, TS_PARAMETER_COUNT
} ts_parameter_id;

typedef struct ts_parameter_desc {
  ts_parameter_id id; ts_parameter_page page; unsigned order;
  const char *label, *unit; ts_parameter_type type; ts_parameter_mapping mapping;
  double minimum, maximum, fine_step, coarse_step;
  const char *const *enum_labels; size_t enum_count;
} ts_parameter_desc;

typedef struct ts_owned_recipe { ts_recipe value; char *name; } ts_owned_recipe;
typedef struct ts_recipe_history {
  ts_owned_recipe undo[TS_EDITOR_HISTORY_CAPACITY], redo[TS_EDITOR_HISTORY_CAPACITY];
  size_t undo_count, redo_count;
} ts_recipe_history;

const ts_parameter_desc *ts_parameter_descriptors(size_t *count);
const ts_parameter_desc *ts_parameter_by_id(ts_parameter_id id);
bool ts_parameter_enabled(ts_parameter_id id, const ts_recipe *recipe);
bool ts_parameter_get_number(ts_parameter_id id, const ts_recipe *recipe, double *value);
bool ts_parameter_set_number(ts_parameter_id id, ts_recipe *recipe, double value);
bool ts_parameter_format(ts_parameter_id id, const ts_recipe *recipe, char *text, size_t capacity);
bool ts_parameter_parse(ts_parameter_id id, ts_recipe *recipe, const char *text);
double ts_parameter_to_position(const ts_parameter_desc *desc, double value);
double ts_parameter_from_position(const ts_parameter_desc *desc, double position);

bool ts_owned_recipe_copy(ts_owned_recipe *destination, const ts_recipe *source);
void ts_owned_recipe_destroy(ts_owned_recipe *recipe);
bool ts_recipe_fields_equal(const ts_recipe *a, const ts_recipe *b);
uint64_t ts_recipe_identity(const ts_recipe *recipe);
void ts_recipe_history_destroy(ts_recipe_history *history);
bool ts_recipe_history_commit(ts_recipe_history *history, const ts_recipe *before);
bool ts_recipe_history_undo(ts_recipe_history *history, ts_owned_recipe *working);
bool ts_recipe_history_redo(ts_recipe_history *history, ts_owned_recipe *working);
