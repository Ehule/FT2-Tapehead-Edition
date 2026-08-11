#pragma once

#include "tapesister/ts_audition.h"

#define TS_SCREEN_WIDTH 632
#define TS_SCREEN_HEIGHT 400
#define TS_PALETTE_SIZE 16
#define TS_FACTORY_RECIPES 6

typedef struct ts_palette {
  uint32_t rgba[TS_PALETTE_SIZE];
} ts_palette;
typedef struct ts_framebuffer {
  uint8_t pixels[TS_SCREEN_WIDTH * TS_SCREEN_HEIGHT];
} ts_framebuffer;

typedef struct ts_ui_model {
  const ts_recipe *recipes[TS_FACTORY_RECIPES + 1];
  const ts_rendered_sample *renders[TS_FACTORY_RECIPES + 1];
  const char *audio_status;
  const char *message;
  size_t recipe_count, selected_recipe, active_voices;
  int base_octave;
  ts_audition_mode mode;
  bool overload;
  bool pressed[128];
} ts_ui_model;

void ts_palette_builtin(ts_palette *palette, const char *name);
bool ts_palette_load_file(const char *path, ts_palette *palette, char *error,
                          size_t error_capacity);
void ts_framebuffer_clear(ts_framebuffer *fb, uint8_t color);
bool ts_framebuffer_put(ts_framebuffer *fb, int x, int y, uint8_t color);
void ts_ui_draw(ts_framebuffer *fb, const ts_ui_model *model);
int ts_ui_recipe_hit(int x, int y, size_t recipe_count);
int ts_ui_keyboard_hit(int x, int y, int base_octave);
