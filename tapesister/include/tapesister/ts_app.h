#pragma once

#include "tapesister/ts_io.h"
#include "tapesister/ts_ui.h"

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
