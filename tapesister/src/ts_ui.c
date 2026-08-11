#include "tapesister/ts_ui.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  C_BG,
  C_PANEL,
  C_LIGHT,
  C_DARK,
  C_TEXT,
  C_HILITE,
  C_WAVE,
  C_WHITE,
  C_BLACK,
  C_PRESSED,
  C_WARN
};

/* Original TapeSister 5x7 bitmap glyphs, stored as five vertical columns. */
static const uint8_t font[59][5] = {{0, 0, 0, 0, 0},
                                    {0, 0, 0x5f, 0, 0},
                                    {0x03, 0, 0x03, 0, 0},
                                    {0x14, 0x7f, 0x14, 0x7f, 0x14},
                                    {0x24, 0x2a, 0x7f, 0x2a, 0x12},
                                    {0x23, 0x13, 0x08, 0x64, 0x62},
                                    {0x36, 0x49, 0x55, 0x22, 0x50},
                                    {0, 0x03, 0, 0, 0},
                                    {0, 0x1c, 0x22, 0x41, 0},
                                    {0, 0x41, 0x22, 0x1c, 0},
                                    {0x14, 0x08, 0x3e, 0x08, 0x14},
                                    {0x08, 0x08, 0x3e, 0x08, 0x08},
                                    {0, 0x50, 0x30, 0, 0},
                                    {0x08, 0x08, 0x08, 0x08, 0x08},
                                    {0, 0x60, 0x60, 0, 0},
                                    {0x20, 0x10, 0x08, 0x04, 0x02},
                                    {0x3e, 0x51, 0x49, 0x45, 0x3e},
                                    {0, 0x42, 0x7f, 0x40, 0},
                                    {0x42, 0x61, 0x51, 0x49, 0x46},
                                    {0x21, 0x41, 0x45, 0x4b, 0x31},
                                    {0x18, 0x14, 0x12, 0x7f, 0x10},
                                    {0x27, 0x45, 0x45, 0x45, 0x39},
                                    {0x3c, 0x4a, 0x49, 0x49, 0x30},
                                    {0x01, 0x71, 0x09, 0x05, 0x03},
                                    {0x36, 0x49, 0x49, 0x49, 0x36},
                                    {0x06, 0x49, 0x49, 0x29, 0x1e},
                                    {0, 0x36, 0x36, 0, 0},
                                    {0, 0x56, 0x36, 0, 0},
                                    {0x08, 0x14, 0x22, 0x41, 0},
                                    {0x14, 0x14, 0x14, 0x14, 0x14},
                                    {0, 0x41, 0x22, 0x14, 0x08},
                                    {0x02, 0x01, 0x51, 0x09, 0x06},
                                    {0x32, 0x49, 0x79, 0x41, 0x3e},
                                    {0x7e, 0x11, 0x11, 0x11, 0x7e},
                                    {0x7f, 0x49, 0x49, 0x49, 0x36},
                                    {0x3e, 0x41, 0x41, 0x41, 0x22},
                                    {0x7f, 0x41, 0x41, 0x22, 0x1c},
                                    {0x7f, 0x49, 0x49, 0x49, 0x41},
                                    {0x7f, 0x09, 0x09, 0x09, 0x01},
                                    {0x3e, 0x41, 0x49, 0x49, 0x7a},
                                    {0x7f, 0x08, 0x08, 0x08, 0x7f},
                                    {0, 0x41, 0x7f, 0x41, 0},
                                    {0x20, 0x40, 0x41, 0x3f, 0x01},
                                    {0x7f, 0x08, 0x14, 0x22, 0x41},
                                    {0x7f, 0x40, 0x40, 0x40, 0x40},
                                    {0x7f, 0x02, 0x0c, 0x02, 0x7f},
                                    {0x7f, 0x04, 0x08, 0x10, 0x7f},
                                    {0x3e, 0x41, 0x41, 0x41, 0x3e},
                                    {0x7f, 0x09, 0x09, 0x09, 0x06},
                                    {0x3e, 0x41, 0x51, 0x21, 0x5e},
                                    {0x7f, 0x09, 0x19, 0x29, 0x46},
                                    {0x46, 0x49, 0x49, 0x49, 0x31},
                                    {0x01, 0x01, 0x7f, 0x01, 0x01},
                                    {0x3f, 0x40, 0x40, 0x40, 0x3f},
                                    {0x1f, 0x20, 0x40, 0x20, 0x1f},
                                    {0x3f, 0x40, 0x38, 0x40, 0x3f},
                                    {0x63, 0x14, 0x08, 0x14, 0x63},
                                    {0x07, 0x08, 0x70, 0x08, 0x07},
                                    {0x61, 0x51, 0x49, 0x45, 0x43}};

static void rect(ts_framebuffer *fb, int x, int y, int w, int h, uint8_t c) {
  for (int yy = 0; yy < h; yy++)
    for (int xx = 0; xx < w; xx++)
      ts_framebuffer_put(fb, x + xx, y + yy, c);
}
static void frame(ts_framebuffer *fb, int x, int y, int w, int h) {
  rect(fb, x, y, w, 1, C_LIGHT);
  rect(fb, x, y, 1, h, C_LIGHT);
  rect(fb, x, y + h - 1, w, 1, C_DARK);
  rect(fb, x + w - 1, y, 1, h, C_DARK);
}
static void text(ts_framebuffer *fb, int x, int y, const char *s, uint8_t c) {
  for (; *s; s++, x += 6) {
    unsigned char ch = (unsigned char)toupper((unsigned char)*s);
    if (ch == '_') {
      for (int xx = 0; xx < 5; xx++)
        ts_framebuffer_put(fb, x + xx, y + 6, c);
      continue;
    }
    if (ch < 32 || ch > 90)
      ch = '?';
    const uint8_t *g = font[ch - 32];
    for (int xx = 0; xx < 5; xx++)
      for (int yy = 0; yy < 7; yy++)
        if (g[xx] & (1U << yy))
          ts_framebuffer_put(fb, x + xx, y + yy, c);
  }
}

void ts_palette_builtin(ts_palette *p, const char *name) {
  static const uint32_t normal[TS_PALETTE_SIZE] = {
      0xff10141a, 0xff28313a, 0xff8b9aa5, 0xff080a0c, 0xffd8e0d0, 0xff527f72,
      0xff7fcea5, 0xffd8d8c8, 0xff151719, 0xffd19a52, 0xffd65252, 0xff526a80,
      0xff707b72, 0xff303b45, 0xffaab7a8, 0xffffffff};
  static const uint32_t dark[TS_PALETTE_SIZE] = {
      0xff08090b, 0xff171a1f, 0xff66717c, 0xff020304, 0xffc7d0c5, 0xff315a54,
      0xff69b58b, 0xffced0c5, 0xff0a0b0d, 0xffb37c3f, 0xffd34e4e, 0xff3d5266,
      0xff4b514c, 0xff22272d, 0xff909a92, 0xffffffff};
  memcpy(p->rgba, name != NULL && strcmp(name, "dark") == 0 ? dark : normal,
         sizeof(p->rgba));
}

static int hex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
static bool equal_icase(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
      return false;
    a++;
    b++;
  }
  return *a == *b;
}
bool ts_palette_load_file(const char *path, ts_palette *p, char *error,
                          size_t cap) {
  if (path == NULL || p == NULL)
    return false;
  FILE *f = fopen(path, "rb");
  if (!f) {
    snprintf(error, cap, "cannot open palette: %s", path);
    return false;
  }
  const char *keys[] = {"PatternText", "BlockMark", "TextOnBlock",
                        "Mouse",       "Desktop",   "Buttons"};
  uint32_t colors[6] = {0};
  bool found[6] = {0};
  char line[256];
  while (fgets(line, sizeof line, f)) {
    char *k = line;
    while (isspace((unsigned char)*k))
      k++;
    if (*k == ';' || *k == '#' || *k == '[' || *k == '\0')
      continue;
    char *eq = strchr(k, '=');
    if (!eq)
      continue;
    *eq++ = '\0';
    char *end = k + strlen(k);
    while (end > k && isspace((unsigned char)end[-1]))
      *--end = '\0';
    while (isspace((unsigned char)*eq))
      eq++;
    for (int i = 0; i < 6; i++)
      if (equal_icase(k, keys[i])) {
        if (*eq != '#' || strlen(eq) < 7) {
          fclose(f);
          snprintf(error, cap, "malformed color for %s", keys[i]);
          return false;
        }
        uint32_t rgb = 0;
        for (int n = 1; n <= 6; n++) {
          int d = hex(eq[n]);
          if (d < 0) {
            fclose(f);
            snprintf(error, cap, "malformed color for %s", keys[i]);
            return false;
          }
          rgb = (rgb << 4) | (uint32_t)d;
        }
        colors[i] = 0xff000000U | rgb;
        found[i] = true;
      }
  }
  fclose(f);
  for (int i = 0; i < 6; i++)
    if (!found[i]) {
      snprintf(error, cap, "palette missing %s", keys[i]);
      return false;
    }
  ts_palette candidate;
  ts_palette_builtin(&candidate, "default");
  candidate.rgba[C_TEXT] = colors[0];
  candidate.rgba[C_HILITE] = colors[1];
  candidate.rgba[C_PRESSED] = colors[2];
  candidate.rgba[C_WHITE] = colors[3];
  candidate.rgba[C_BG] = colors[4];
  candidate.rgba[C_PANEL] = colors[5];
  *p = candidate;
  return true;
}

void ts_framebuffer_clear(ts_framebuffer *fb, uint8_t c) {
  if (fb)
    memset(fb->pixels, c, sizeof(fb->pixels));
}
bool ts_framebuffer_put(ts_framebuffer *fb, int x, int y, uint8_t c) {
  if (!fb || x < 0 || y < 0 || x >= TS_SCREEN_WIDTH || y >= TS_SCREEN_HEIGHT ||
      c >= TS_PALETTE_SIZE)
    return false;
  fb->pixels[y * TS_SCREEN_WIDTH + x] = c;
  return true;
}

int ts_ui_recipe_hit(int x, int y, size_t count) {
  if (x < 8 || x >= 180 || y < 45)
    return -1;
  int row = (y - 45) / 24;
  if (row < 0 || (size_t)row >= count || y >= 45 + row * 24 + 20)
    return -1;
  return row;
}

int ts_ui_keyboard_hit(int x, int y, int octave) {
  const int kx = 22, ky = 333, ww = 42;
  if (x < kx || x >= kx + 14 * ww || y < ky || y >= 385)
    return -1;
  static const int black_note[10] = {1, 3, 6, 8, 10, 13, 15, 18, 20, 22};
  static const int boundary[10] = {1, 2, 4, 5, 6, 8, 9, 11, 12, 13};
  if (y < 365)
    for (int i = 0; i < 10; i++) {
      int bx = kx + boundary[i] * ww - 12;
      if (x >= bx && x < bx + 24) {
        int n = 12 * (octave + 1) + black_note[i];
        return n <= 127 ? n : -1;
      }
    }
  static const int white_note[14] = {0,  2,  4,  5,  7,  9,  11,
                                     12, 14, 16, 17, 19, 21, 23};
  int n = 12 * (octave + 1) + white_note[(x - kx) / ww];
  return n <= 127 ? n : -1;
}

static const char *source_name(ts_source_type v) {
  static const char *n[] = {"SINE", "TRIANGLE", "SAW", "PULSE", "CLICK"};
  return n[v];
}
static const char *filter_name(ts_filter_mode v) {
  static const char *n[] = {"LOW PASS", "BAND PASS", "HIGH PASS", "NOTCH"};
  return n[v];
}
static const char *shape_name(ts_shaper_type v) {
  static const char *n[] = {"SOFT", "HARD", "FOLD"};
  return n[v];
}
void ts_ui_draw(ts_framebuffer *fb, const ts_ui_model *m) {
  ts_framebuffer_clear(fb, C_BG);
  rect(fb, 0, 0, TS_SCREEN_WIDTH, 30, C_PANEL);
  text(fb, 10, 9, "TAPESISTER 0.1C", C_TEXT);
  text(fb, 470, 9, m->overload ? "OVERLOAD" : "AUDIO OK",
       m->overload ? C_WARN : C_WAVE);
  frame(fb, 6, 38, 178, 170);
  text(fb, 12, 34, "FACTORY RECIPES", C_TEXT);
  for (size_t i = 0; i < m->recipe_count; i++) {
    int y = 45 + (int)i * 24;
    if (i == m->selected_recipe)
      rect(fb, 9, y, 172, 20, C_HILITE);
    text(fb, 14, y + 6, m->recipes[i]->name, C_TEXT);
  }
  frame(fb, 190, 38, 436, 268);
  const ts_recipe *r = m->recipes[m->selected_recipe];
  const ts_rendered_sample *s = m->renders[m->selected_recipe];
  char line[128];
  snprintf(line, sizeof line, "NAME %s", r->name);
  text(fb, 200, 48, line, C_TEXT);
  snprintf(line, sizeof line, "SOURCE %s  FILTER %s  SHAPER %s",
           source_name(r->source), filter_name(r->filter_mode),
           shape_name(r->shaper));
  text(fb, 200, 62, line, C_TEXT);
  snprintf(line, sizeof line,
           "ROOT %u FINE %d/100 CENT DURATION %u MS RATE %u HZ",
           r->root_midi_note, r->fine_tune_cent100,
           (unsigned)((uint64_t)r->requested_frames * 1000U / r->sample_rate),
           r->sample_rate);
  text(fb, 200, 74, line, C_TEXT);
  snprintf(line, sizeof line, "FINISH %s  MODE %s  OCTAVE %d  VOICES %u",
           r->finishing_mode == TS_FINISH_TARGET_PEAK ? "TARGET PEAK"
                                                      : "FIXED HEADROOM",
           m->mode == TS_AUDITION_ONE_SHOT ? "ONE SHOT" : "GATED",
           m->base_octave, (unsigned)m->active_voices);
  text(fb, 200, 86, line, C_TEXT);
  rect(fb, 200, 104, 416, 184, C_DARK);
  if (s && s->samples && s->frame_count) {
    for (int x = 0; x < 416; x++) {
      size_t a = (size_t)x * s->frame_count / 416U,
             b = (size_t)(x + 1) * s->frame_count / 416U;
      if (b <= a)
        b = a + 1;
      float lo = 1, hi = -1;
      for (size_t i = a; i < b && i < s->frame_count; i++) {
        if (s->samples[i] < lo)
          lo = s->samples[i];
        if (s->samples[i] > hi)
          hi = s->samples[i];
      }
      int y1 = 196 - (int)(hi * 84), y2 = 196 - (int)(lo * 84);
      for (int y = y1; y <= y2; y++)
        ts_framebuffer_put(fb, 200 + x, y, C_WAVE);
    }
  }
  text(fb, 10, 218, m->audio_status ? m->audio_status : "AUDIO UNKNOWN",
       C_TEXT);
  if (m->message)
    text(fb, 10, 234, m->message, C_WARN);
  const int kx = 22, ky = 333, ww = 42;
  for (int i = 0; i < 14; i++) {
    rect(fb, kx + i * ww, ky, ww - 1, 52, C_WHITE);
    frame(fb, kx + i * ww, ky, ww, 52);
  }
  static const int bn[10] = {1, 2, 4, 5, 6, 8, 9, 11, 12, 13};
  for (int i = 0; i < 10; i++)
    rect(fb, kx + bn[i] * ww - 12, ky, 24, 32, C_BLACK);
  for (int note = 0; note < 128; note++)
    if (m->pressed[note]) {
      int hitbase = 12 * (m->base_octave + 1);
      int rel = note - hitbase;
      if (rel >= 0 && rel < 24) {
        for (int y = 333; y < 385; y++)
          for (int x = 22; x < 610; x++)
            if (ts_ui_keyboard_hit(x, y, m->base_octave) == note)
              ts_framebuffer_put(fb, x, y, C_PRESSED);
      }
    }
  text(fb, 10, 390, "ZSXDCVGBHNJM / Q2W3ER5T6Y7U  [ ] OCT  TAB GATE  SPACE STOP",
       C_TEXT);
}
