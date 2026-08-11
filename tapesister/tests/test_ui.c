#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "tapesister/ts_app.h"
#include "tapesister/ts_presentation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static char temporaryPaths[2][512];
static size_t temporaryPathCount;

static void cleanupTemporaryPaths(void)
{
  for (size_t i = 0; i < temporaryPathCount; i++) remove(temporaryPaths[i]);
}

static FILE *createPaletteTemporary(char *path, size_t capacity)
{
  if (temporaryPathCount >= 2) return NULL;
#ifdef _WIN32
  char directory[MAX_PATH];
  if (GetTempPathA(MAX_PATH, directory) == 0) return NULL;
  HANDLE handle = INVALID_HANDLE_VALUE;
  for (unsigned int attempt = 0; attempt < 64; attempt++)
  {
    const int count = snprintf(path, capacity, "%stapesister_palette_%lu_%u.tmp",
        directory, (unsigned long)GetCurrentProcessId(), attempt);
    if (count < 0 || (size_t)count >= capacity) return NULL;
    handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (handle != INVALID_HANDLE_VALUE) break;
  }
  if (handle == INVALID_HANDLE_VALUE) return NULL;
  const int descriptor = _open_osfhandle((intptr_t)handle, _O_WRONLY | _O_BINARY);
  if (descriptor < 0) { CloseHandle(handle); remove(path); return NULL; }
  FILE *file = _fdopen(descriptor, "wb");
  if (file == NULL) _close(descriptor);
#else
  const char pattern[] = "/tmp/tapesister_palette_XXXXXX";
  if (sizeof (pattern) > capacity) return NULL;
  memcpy(path, pattern, sizeof (pattern));
  const int descriptor = mkstemp(path);
  if (descriptor < 0) return NULL;
  FILE *file = fdopen(descriptor, "wb");
  if (file == NULL) close(descriptor);
#endif
  if (file == NULL) { remove(path); return NULL; }
  memcpy(temporaryPaths[temporaryPathCount], path, strlen(path) + 1);
  temporaryPathCount++;
  return file;
}

int main(void) {
  CHECK(atexit(cleanupTemporaryPaths) == 0);
  ts_present_rect present;
  CHECK(ts_present_fit(1264, 800, &present));
  CHECK(present.x == 0 && present.y == 0 && present.w == 1264 &&
        present.h == 800);
  int logical_x, logical_y;
  CHECK(ts_present_window_to_logical(&present, 1264, 800, 1264, 800, 0, 0,
                                     &logical_x, &logical_y));
  CHECK(logical_x == 0 && logical_y == 0);
  CHECK(ts_present_window_to_logical(&present, 1264, 800, 1264, 800, 1263, 799,
                                     &logical_x, &logical_y));
  CHECK(logical_x == 631 && logical_y == 399);
  CHECK(!ts_present_window_to_logical(&present, 1264, 800, 1264, 800, 1264, 799,
                                      &logical_x, &logical_y));
  CHECK(ts_present_fit(1000, 700, &present));
  CHECK(present.w == 1000 && present.h == 633 && present.y == 33);
  CHECK(!ts_present_window_to_logical(&present, 1000, 700, 1000, 700, 500, 32,
                                      &logical_x, &logical_y));
  CHECK(ts_present_window_to_logical(&present, 1000, 700, 1000, 700, 500, 33,
                                     &logical_x, &logical_y));
  CHECK(logical_y == 0);
  CHECK(ts_present_fit(1920, 1080, &present));
  CHECK(present.h == 1080 && present.w > 1700 && present.x > 0);
  CHECK(ts_present_window_to_logical(&present, 960, 540, 1920, 1080, 480, 270,
                                     &logical_x, &logical_y));
  CHECK(logical_x >= 315 && logical_x <= 316 && logical_y == 200);
  CHECK(ts_present_fit(316, 200, &present));
  CHECK(present.x == 0 && present.y == 0 && present.w == 316 &&
        present.h == 200);
  CHECK(ts_present_fit(1600, 500, &present));
  CHECK(present.h == 500 && present.w == 790 && present.x == 405);
  CHECK(ts_present_fit(500, 1000, &present));
  CHECK(present.w == 500 && present.h == 316 && present.y == 342);

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
  CHECK(ts_ui_tab_hit(190, 38) == TS_PAGE_SOURCE);
  CHECK(ts_ui_tab_hit(625, 53) == TS_PAGE_SAMPLE);
  CHECK(ts_ui_tab_hit(626, 53) == -1);
  CHECK(ts_ui_parameter_hit(194, 58, TS_PAGE_SOURCE) == TS_P_SOURCE);
  CHECK(ts_ui_parameter_hit(621, 72, TS_PAGE_SOURCE) == TS_P_SOURCE_SHAPE);
  CHECK(ts_ui_parameter_hit(622, 58, TS_PAGE_SOURCE) == -1);
  CHECK(ts_ui_slider_position(419) == 0.0);
  CHECK(ts_ui_slider_position(420) == 0.0);
  CHECK(ts_ui_slider_position(570) == 1.0);
  CHECK(ts_ui_slider_position(571) == 1.0);
  CHECK(ts_ui_action_hit(6, 309) == TS_UI_COMMIT_PARENT);
  CHECK(ts_ui_action_hit(625, 328) == TS_UI_MODE);
  CHECK(ts_ui_action_hit(626, 328) == TS_UI_ACTION_NONE);
  CHECK(ts_ui_keyboard_hit(22, 384, 3) == 48);
  CHECK(ts_ui_keyboard_hit(609, 384, 3) == 71);
  CHECK(ts_ui_keyboard_hit(52, 340, 3) == 49);
  CHECK(ts_ui_keyboard_hit(21, 350, 3) == -1);
  CHECK(ts_ui_keyboard_hit(610, 350, 3) == -1);
  bool piano_notes[24] = {0};
  for (int y = 333; y < 385; y++)
    for (int x = 22; x < 610; x++) {
      const int hit = ts_ui_keyboard_hit(x, y, 3);
      if (hit >= 48 && hit < 72)
        piano_notes[hit - 48] = true;
    }
  for (int i = 0; i < 24; i++)
    CHECK(piano_notes[i]);
  ts_palette fallback, candidate;
  ts_palette_builtin(&fallback, "default");
  candidate = fallback;
  char error[256];
  CHECK(!ts_palette_load_file("/no/such/palette", &candidate, error,
                              sizeof error));
  CHECK(memcmp(&candidate, &fallback, sizeof fallback) == 0);
  char validPalettePath[512], invalidPalettePath[512];
  FILE *f = createPaletteTemporary(validPalettePath, sizeof (validPalettePath));
  CHECK(f);
  fputs(
      "[TapeheadPalette]\nPatternText=#ffffff\nBlockMark=#112233\nTextOnBlock=#"
      "334455\nMouse=#abcdef\nDesktop=#010203\nButtons=#102030\n",
      f);
  fclose(f);
  CHECK(ts_palette_load_file(validPalettePath, &candidate, error, sizeof error));
  f = createPaletteTemporary(invalidPalettePath, sizeof (invalidPalettePath));
  CHECK(f);
  fputs("PatternText=#ffffff\n", f);
  fclose(f);
  candidate = fallback;
  CHECK(!ts_palette_load_file(invalidPalettePath, &candidate, error, sizeof error));
  CHECK(memcmp(&candidate, &fallback, sizeof fallback) == 0);
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
  CHECK(ts_app_key_note('G', 3) == 54);
  CHECK(ts_app_toggle_mode(&app, false));
  CHECK(app.mode == TS_AUDITION_GATED);
  CHECK(!ts_app_toggle_mode(&app, true));
  CHECK(app.mode == TS_AUDITION_GATED);

  ts_app_mouse_result mouse = ts_app_mouse_press(&app, 10, 45 + 2 * 24);
  CHECK(mouse.selected_recipe == 2 && app.selected == 2);
  mouse = ts_app_mouse_press(&app, 22, 384);
  CHECK(mouse.note_on == 48 && app.mouse_note == 48 && app.key_down[48]);
  mouse = ts_app_mouse_move(&app, 52, 340);
  CHECK(mouse.note_off == 48 && mouse.note_on == 49 && app.mouse_note == 49);
  mouse = ts_app_mouse_release(&app);
  CHECK(mouse.note_off == 49 && app.mouse_note == -1 && !app.key_down[49]);
  app.mode = TS_AUDITION_ONE_SHOT;
  mouse = ts_app_mouse_press(&app, 22, 384);
  CHECK(mouse.note_on == 48);
  mouse = ts_app_mouse_move(&app, 52, 340);
  CHECK(mouse.note_off == -1 && mouse.note_on == 49 && app.mouse_note == 49);
  mouse = ts_app_mouse_release(&app);
  CHECK(mouse.note_off == -1 && app.mouse_note == -1 && !app.key_down[49]);
  app.mode = TS_AUDITION_GATED;
  mouse = ts_app_mouse_press(&app, 22, 384);
  mouse = ts_app_focus_lost(&app);
  CHECK(mouse.note_off == 48 && app.mouse_note == -1);

  CHECK(!ts_app_update_overload(&app, 0, 1000));
  CHECK(ts_app_update_overload(&app, 1, 1100));
  CHECK(ts_app_update_overload(&app, 1, 1800));
  CHECK(!ts_app_update_overload(&app, 1, 1851));
  CHECK(ts_app_update_overload(&app, 2, 1900));
  CHECK(ts_app_update_overload(&app, 2, 2600));
  CHECK(!ts_app_update_overload(&app, 2, 2651));
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
