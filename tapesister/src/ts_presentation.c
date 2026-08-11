#include "tapesister/ts_presentation.h"

#include <math.h>
#include <stddef.h>

bool ts_present_fit(const int output_width, const int output_height,
                    ts_present_rect *destination) {
  if (destination == NULL || output_width <= 0 || output_height <= 0)
    return false;

  const double scale_x = (double)output_width / TS_PRESENT_WIDTH;
  const double scale_y = (double)output_height / TS_PRESENT_HEIGHT;
  const double scale = scale_x < scale_y ? scale_x : scale_y;
  int width = (int)floor(TS_PRESENT_WIDTH * scale + 0.5);
  int height = (int)floor(TS_PRESENT_HEIGHT * scale + 0.5);
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;
  if (width > output_width)
    width = output_width;
  if (height > output_height)
    height = output_height;
  destination->x = (output_width - width) / 2;
  destination->y = (output_height - height) / 2;
  destination->w = width;
  destination->h = height;
  return true;
}

bool ts_present_window_to_logical(
    const ts_present_rect *destination, const int window_width,
    const int window_height, const int output_width, const int output_height,
    const int window_x, const int window_y, int *logical_x, int *logical_y) {
  if (destination == NULL || logical_x == NULL || logical_y == NULL ||
      window_width <= 0 || window_height <= 0 || output_width <= 0 ||
      output_height <= 0 || destination->w <= 0 || destination->h <= 0)
    return false;

  const double drawable_x = (double)window_x * output_width / window_width;
  const double drawable_y = (double)window_y * output_height / window_height;
  if (drawable_x < destination->x || drawable_y < destination->y ||
      drawable_x >= destination->x + destination->w ||
      drawable_y >= destination->y + destination->h)
    return false;

  int x = (int)floor((drawable_x - destination->x) * TS_PRESENT_WIDTH /
                     destination->w);
  int y = (int)floor((drawable_y - destination->y) * TS_PRESENT_HEIGHT /
                     destination->h);
  if (x < 0 || y < 0 || x >= TS_PRESENT_WIDTH || y >= TS_PRESENT_HEIGHT)
    return false;
  *logical_x = x;
  *logical_y = y;
  return true;
}
