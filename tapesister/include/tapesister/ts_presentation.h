#pragma once

#include <stdbool.h>

#define TS_PRESENT_WIDTH 632
#define TS_PRESENT_HEIGHT 400

typedef struct ts_present_rect {
  int x, y, w, h;
} ts_present_rect;

bool ts_present_fit(int output_width, int output_height,
                    ts_present_rect *destination);
bool ts_present_window_to_logical(const ts_present_rect *destination,
                                  int window_width, int window_height,
                                  int output_width, int output_height,
                                  int window_x, int window_y, int *logical_x,
                                  int *logical_y);
