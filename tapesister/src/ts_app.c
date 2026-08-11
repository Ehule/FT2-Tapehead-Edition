#include "tapesister/ts_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *factory_names[TS_FACTORY_RECIPES] = {
    "clean_sustain.tsr",  "percussive_pluck.tsr", "noisy_metal.tsr",
    "unstable_drone.tsr", "digital_bass.tsr",     "spacious_decay.tsr"};

bool ts_cli_parse(int argc, char **argv, ts_cli_options *o, char *error,
                  size_t cap) {
  if (!o)
    return false;
  memset(o, 0, sizeof(*o));
  o->palette_name = "default";
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "--smoke-test") == 0)
      o->smoke_test = true;
    else if (strcmp(a, "--help") == 0)
      o->help = true;
    else if (strcmp(a, "--recipe") == 0 || strcmp(a, "--palette-file") == 0 ||
             strcmp(a, "--palette") == 0 || strcmp(a, "--resource-dir") == 0) {
      if (++i >= argc) {
        snprintf(error, cap, "missing value after %s", a);
        return false;
      }
      if (strcmp(a, "--recipe") == 0)
        o->recipe_path = argv[i];
      else if (strcmp(a, "--palette-file") == 0)
        o->palette_file = argv[i];
      else if (strcmp(a, "--palette") == 0)
        o->palette_name = argv[i];
      else
        o->resource_dir = argv[i];
    } else {
      snprintf(error, cap, "unknown option: %s", a);
      return false;
    }
  }
  if (strcmp(o->palette_name, "default") != 0 &&
      strcmp(o->palette_name, "dark") != 0) {
    snprintf(error, cap, "unknown built-in palette: %s", o->palette_name);
    return false;
  }
  return true;
}

static bool has_factory(const char *d) {
  char p[1024];
  for (int i = 0; i < TS_FACTORY_RECIPES; i++) {
    snprintf(p, sizeof p, "%s/%s", d, factory_names[i]);
    FILE *f = fopen(p, "rb");
    if (!f)
      return false;
    fclose(f);
  }
  return true;
}
bool ts_app_find_factory(const char *override, const char *exe, char *out,
                         size_t cap, char *error, size_t ecap) {
  const char *candidates[3] = {override, NULL, TS_FACTORY_SOURCE_DIR};
  char beside[1024] = {0};
  if (exe) {
    snprintf(beside, sizeof beside, "%s", exe);
    char *s1 = strrchr(beside, '/');
    char *s2 = strrchr(beside, '\\');
    char *s = s1;
    if (s2 != NULL && (s1 == NULL || s2 > s1))
      s = s2;
    if (s)
      *s = '\0';
    else
      strcpy(beside, ".");
    strncat(beside, "/resources/recipes", sizeof(beside) - strlen(beside) - 1);
    candidates[1] = beside;
  }
  for (int i = 0; i < 3; i++)
    if (candidates[i] && has_factory(candidates[i])) {
      if (strlen(candidates[i]) + 1 > cap)
        break;
      strcpy(out, candidates[i]);
      return true;
    }
  snprintf(error, ecap, "factory recipes not found; use --resource-dir PATH");
  return false;
}

bool ts_app_load_bank(ts_app_state *a, const char *d, const char *extra,
                      char *error, size_t cap) {
  if (!a || !d)
    return false;
  memset(a, 0, sizeof(*a));
  a->base_octave = 3;
  a->mode = TS_AUDITION_ONE_SHOT;
  for (int i = 0; i < TS_FACTORY_RECIPES; i++) {
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", d, factory_names[i]);
    ts_io_error e;
    if (ts_recipe_load_file(p, &a->bank[i].recipe, &e) != TS_IO_OK) {
      snprintf(error, cap, "%s: %s", factory_names[i], e.message);
      ts_app_dispose(a);
      return false;
    }
    a->bank[i].loaded = true;
    a->bank_count++;
  }
  if (extra) {
    ts_io_error e;
    if (ts_recipe_load_file(extra, &a->bank[a->bank_count].recipe, &e) !=
        TS_IO_OK)
      snprintf(a->status, sizeof a->status, "EXTERNAL RECIPE ERROR: %s",
               e.message);
    else {
      a->bank[a->bank_count].loaded = true;
      a->bank_count++;
    }
  }
  return true;
}
bool ts_app_ensure_rendered(ts_app_state *a, size_t i, char *error,
                            size_t cap) {
  if (!a || i >= a->bank_count)
    return false;
  ts_app_bank_entry *e = &a->bank[i];
  if (e->rendered)
    return true;
  ts_render_report report;
  if (!ts_render(&e->recipe, &e->render, &report)) {
    snprintf(error, cap, "render failed: %s", e->recipe.name);
    return false;
  }
  e->source.samples = e->render.samples;
  e->source.frame_count = e->render.frame_count;
  e->source.sample_rate = e->render.sample_rate;
  e->source.root_midi_note = e->recipe.root_midi_note;
  e->rendered = true;
  return true;
}

int ts_app_key_note(int key, int octave) {
  const char *low = "ZSXDCVGBHNJM", *high = "Q2W3ER5T6Y7U";
  key = (key >= 'a' && key <= 'z') ? key - 32 : key;
  const char *p = strchr(low, key);
  int n;
  if (p)
    n = 12 * (octave + 1) + (int)(p - low);
  else if ((p = strchr(high, key)))
    n = 12 * (octave + 2) + (int)(p - high);
  else
    return -1;
  return n >= 0 && n <= 127 ? n : -1;
}
bool ts_app_key_press(ts_app_state *a, int key, bool repeat, int *note) {
  if (note)
    *note = -1;
  if (!a || repeat)
    return false;
  int n = ts_app_key_note(key, a->base_octave);
  if (n < 0 || a->key_down[n])
    return false;
  a->key_down[n] = true;
  if (note)
    *note = n;
  return true;
}
void ts_app_key_release(ts_app_state *a, int key, int *note) {
  if (note)
    *note = -1;
  if (!a)
    return;
  int n = ts_app_key_note(key, a->base_octave);
  if (n >= 0) {
    a->key_down[n] = false;
    if (note)
      *note = n;
  }
}
void ts_app_dispose(ts_app_state *a) {
  if (!a)
    return;
  for (size_t i = 0; i < a->bank_count; i++) {
    ts_rendered_sample_free(&a->bank[i].render);
    if (a->bank[i].loaded)
      ts_recipe_loaded_dispose(&a->bank[i].recipe);
  }
  memset(a, 0, sizeof(*a));
}
