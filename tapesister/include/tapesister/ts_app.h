#pragma once

#include "tapesister/ts_io.h"
#include "tapesister/ts_ui.h"
#include "tapesister/ts_preview.h"
#include "tapesister/ts_text_edit.h"

typedef struct ts_cli_options {
  const char *recipe_path, *palette_file, *palette_name, *resource_dir;
  bool smoke_test, help;
} ts_cli_options;

typedef struct ts_app_bank_entry {
  ts_recipe recipe;
  ts_rendered_sample render;
  ts_audition_source source;
  bool loaded, rendered;
} ts_app_bank_entry;

typedef struct ts_app_state {
  ts_app_bank_entry bank[TS_FACTORY_RECIPES + 1];
  size_t bank_count, selected;
  int base_octave;
  ts_audition_mode mode;
  bool key_down[128];
  int mouse_note;
  uint32_t overload_generation;
  uint64_t overload_last_ms;
  bool overload_visible;
  ts_parameter_page page;
  int focused_parameter;
  ts_recipe_history history, accepted_history;
  ts_owned_recipe saved, parent, baseline, accepted;
  bool has_saved, has_parent, has_baseline, has_accepted;
  uint64_t accepted_session_identity;
  bool rejected_edit;
  char saved_path[TS_PATH_MAX_BYTES + 1U];
  bool has_baked;
  uint64_t baked_recipe_identity, baked_pcm_identity;
  char baked_recipe_path[TS_PATH_MAX_BYTES + 1U];
  char baked_wav_path[TS_PATH_MAX_BYTES + 1U];
  uint64_t working_generation, published_generation;
  uint64_t session_identity, preview_session_identity;
  ts_render_worker *render_worker;
  ts_preview_pool previews;
  uint64_t failed_generation;
  char render_error[128];
  char status[192];
} ts_app_state;

typedef struct ts_app_mouse_result {
  int note_on, note_off, selected_recipe;
} ts_app_mouse_result;

bool ts_cli_parse(int argc, char **argv, ts_cli_options *options, char *error,
                  size_t error_capacity);
bool ts_app_find_factory(const char *resource_override, const char *executable,
                         char *directory, size_t capacity, char *error,
                         size_t error_capacity);
bool ts_app_load_bank(ts_app_state *app, const char *factory_directory,
                      const char *extra_recipe, char *error,
                      size_t error_capacity);
bool ts_app_ensure_rendered(ts_app_state *app, size_t index, char *error,
                            size_t error_capacity);
int ts_app_key_note(int key, int base_octave);
bool ts_app_key_press(ts_app_state *app, int key, bool repeat, int *note);
void ts_app_key_release(ts_app_state *app, int key, int *note);
bool ts_app_toggle_mode(ts_app_state *app, bool repeat);
ts_app_mouse_result ts_app_mouse_press(ts_app_state *app, int logical_x,
                                       int logical_y);
ts_app_mouse_result ts_app_mouse_move(ts_app_state *app, int logical_x,
                                      int logical_y);
ts_app_mouse_result ts_app_mouse_release(ts_app_state *app);
ts_app_mouse_result ts_app_focus_lost(ts_app_state *app);
bool ts_app_update_overload(ts_app_state *app, uint32_t generation,
                            uint64_t now_ms);
void ts_app_dispose(ts_app_state *app);
bool ts_app_set_page(ts_app_state *app, ts_parameter_page page);
bool ts_app_page_move(ts_app_state *app, int direction);
bool ts_app_focus_move(ts_app_state *app, int direction);
bool ts_app_adjust_parameter(ts_app_state *app, ts_parameter_id id,
                             double steps, bool commit_history);
bool ts_app_undo(ts_app_state *app);
bool ts_app_redo(ts_app_state *app);
bool ts_app_commit_parent(ts_app_state *app);
bool ts_app_update_parent(ts_app_state *app, bool confirmed);
bool ts_app_dirty(const ts_app_state *app);
bool ts_app_request_render(ts_app_state *app);
bool ts_app_poll_render(ts_app_state *app);
bool ts_app_rendering(ts_app_state *app);
bool ts_app_render_failed(ts_app_state *app);
bool ts_app_render_matched(const ts_app_state *app);
const ts_audition_source *ts_app_preview_source(const ts_app_state *app);
bool ts_app_set_parameter_text(ts_app_state *app, ts_parameter_id id,
                               const char *text, char *error, size_t capacity);
bool ts_app_save_recipe(ts_app_state *app, const char *path, ts_io_error *error);
bool ts_app_save_recipe_confirmed(ts_app_state *app, const char *path,
                                  bool replace, ts_io_error *error);
bool ts_app_load_recipe(ts_app_state *app, const char *path, ts_io_error *error);
bool ts_app_bake(ts_app_state *app, const char *recipe_path,
                 const char *wav_path, ts_io_error *error);
bool ts_app_bake_confirmed(ts_app_state *app, const char *recipe_path,
                           const char *wav_path, bool replace,
                           ts_io_error *error);
bool ts_app_baked(const ts_app_state *app);
bool ts_app_select_recipe(ts_app_state *app, size_t index);
bool ts_app_session_requires_discard(const ts_app_state *app);
bool ts_app_select_recipe_confirmed(ts_app_state *app, size_t index,
                                    bool confirmed);
