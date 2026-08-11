#include "tapesister/ts_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(void) {
  ts_framebuffer *fb = calloc(1, sizeof(*fb));
  CHECK(fb);
  ts_framebuffer_clear(fb, 0);
  CHECK(ts_framebuffer_put(fb, 0, 0, 15));
  CHECK(ts_framebuffer_put(fb, 631, 399, 15));
  CHECK(!ts_framebuffer_put(fb, -1, 0, 1));
  CHECK(!ts_framebuffer_put(fb, 632, 0, 1));
  CHECK(!ts_framebuffer_put(fb, 0, 400, 1));
  CHECK(ts_ui_recipe_hit(8, 45, 6) == 0);
  CHECK(ts_ui_recipe_hit(179, 64, 6) == 0);
  CHECK(ts_ui_recipe_hit(180, 45, 6) == -1);
  CHECK(ts_ui_recipe_hit(8, 65, 6) == -1);
  CHECK(ts_ui_keyboard_hit(22, 384, 3) == 48);
  CHECK(ts_ui_keyboard_hit(609, 384, 3) == 71);
  CHECK(ts_ui_keyboard_hit(52, 340, 3) == 49);
  CHECK(ts_ui_keyboard_hit(21, 350, 3) == -1);
  CHECK(ts_ui_keyboard_hit(610, 350, 3) == -1);
  ts_palette fallback, candidate;
  ts_palette_builtin(&fallback, "default");
  candidate = fallback;
  char error[256];
  CHECK(!ts_palette_load_file("/no/such/palette", &candidate, error,
                              sizeof error));
  CHECK(memcmp(&candidate, &fallback, sizeof fallback) == 0);
  const char *pfile = "/tmp/tapesister_palette_test.pal";
  FILE *f = fopen(pfile, "wb");
  CHECK(f);
  fputs(
      "[TapeheadPalette]\nPatternText=#ffffff\nBlockMark=#112233\nTextOnBlock=#"
      "334455\nMouse=#abcdef\nDesktop=#010203\nButtons=#102030\n",
      f);
  fclose(f);
  CHECK(ts_palette_load_file(pfile, &candidate, error, sizeof error));
  remove(pfile);
  f = fopen(pfile, "wb");
  CHECK(f);
  fputs("PatternText=#ffffff\n", f);
  fclose(f);
  candidate = fallback;
  CHECK(!ts_palette_load_file(pfile, &candidate, error, sizeof error));
  CHECK(memcmp(&candidate, &fallback, sizeof fallback) == 0);
  remove(pfile);
  char *a0[] = {(char *)"tapesister", (char *)"--recipe",
                (char *)"x.tsr",      (char *)"--palette",
                (char *)"dark",       (char *)"--smoke-test"};
  ts_cli_options o;
  CHECK(ts_cli_parse(6, a0, &o, error, sizeof error) && o.smoke_test &&
        strcmp(o.recipe_path, "x.tsr") == 0);
  char *a1[] = {(char *)"tapesister", (char *)"--bad"};
  CHECK(!ts_cli_parse(2, a1, &o, error, sizeof error));
  char dir[1024];
  CHECK(ts_app_find_factory(NULL, "/not/installed/tapesister", dir, sizeof dir,
                            error, sizeof error));
  CHECK(strcmp(dir, TS_FACTORY_SOURCE_DIR) == 0);
  CHECK(!ts_app_find_factory("/missing", NULL, dir, 2, error, sizeof error));
  ts_app_state app;
  CHECK(ts_app_load_bank(&app, TS_FACTORY_SOURCE_DIR, "/missing/external.tsr",
                         error, sizeof error));
  CHECK(app.bank_count == 6 &&
        strstr(app.status, "EXTERNAL RECIPE ERROR") != NULL);
  for (size_t i = 0; i < 6; i++)
    CHECK(ts_app_ensure_rendered(&app, i, error, sizeof error));
  int note;
  CHECK(ts_app_key_press(&app, 'Z', false, &note) && note == 48);
  CHECK(!ts_app_key_press(&app, 'Z', true, &note));
  ts_app_key_release(&app, 'Z', &note);
  CHECK(note == 48);
  CHECK(ts_app_key_note('Q', 3) == 60);
  CHECK(ts_app_key_note('Z', -1) == 0);
  CHECK(ts_app_key_note('U', 9) == -1);
  ts_ui_model model = {0};
  model.recipe_count = 6;
  model.selected_recipe = 0;
  model.base_octave = 3;
  model.audio_status = "AUDIO TEST";
  for (size_t i = 0; i < 6; i++) {
    model.recipes[i] = &app.bank[i].recipe;
    model.renders[i] = &app.bank[i].render;
  }
  for (size_t i = 0; i < 6; i++) {
    model.selected_recipe = i;
    ts_ui_draw(fb, &model);
    size_t nonzero = 0, wave = 0, keyboard = 0;
    for (size_t p = 0; p < TS_SCREEN_WIDTH * TS_SCREEN_HEIGHT; p++)
      if (fb->pixels[p])
        nonzero++;
    for (int y = 104; y < 288; y++)
      for (int x = 200; x < 616; x++)
        if (fb->pixels[y * TS_SCREEN_WIDTH + x])
          wave++;
    for (int y = 333; y < 385; y++)
      for (int x = 22; x < 610; x++)
        if (fb->pixels[y * TS_SCREEN_WIDTH + x])
          keyboard++;
    CHECK(nonzero > 20000 && wave > 1000 && keyboard > 10000);
    for (size_t p = 0; p < TS_SCREEN_WIDTH * TS_SCREEN_HEIGHT; p++)
      CHECK(fb->pixels[p] < TS_PALETTE_SIZE);
  }
  ts_app_dispose(&app);
  free(fb);
  puts("PASS UI framebuffer, palette, hit tests, CLI and discovery");
  return 0;
}
