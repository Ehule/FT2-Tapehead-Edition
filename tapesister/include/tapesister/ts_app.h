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
  char status[192];
} ts_app_state;

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
void ts_app_dispose(ts_app_state *app);
