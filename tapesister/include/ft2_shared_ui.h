/* Shared FT2-style UI primitives. Geometry is adapted from ft2_scrollbars.c and
 * bevel rendering from ft2_pushbuttons.c (FT2 Clone, see LICENSE/NOTICE). */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FT2_UI_WIDTH 632
#define FT2_UI_HEIGHT 400
typedef struct ft2_ui_surface { uint8_t *pixels; int width, height, pitch; } ft2_ui_surface;
typedef struct ft2_ui_scrollbar {
  int x,y,w,h,arrows,thumb_y,thumb_h,drag_bias;
  size_t count,page,pos;
  bool dragging;
} ft2_ui_scrollbar;

void ft2_ui_fill(ft2_ui_surface *s,int x,int y,int w,int h,uint8_t color);
void ft2_ui_bevel(ft2_ui_surface *s,int x,int y,int w,int h,uint8_t face,uint8_t light,uint8_t dark,bool pressed);
void ft2_ui_text(ft2_ui_surface *s,int x,int y,const char *text,uint8_t color);
bool ft2_ui_text_supported(const char *text);
void ft2_ui_scrollbar_set(ft2_ui_scrollbar *bar,size_t count,size_t page,size_t pos);
bool ft2_ui_scrollbar_press(ft2_ui_scrollbar *bar,int x,int y);
bool ft2_ui_scrollbar_drag(ft2_ui_scrollbar *bar,int y);
void ft2_ui_scrollbar_release(ft2_ui_scrollbar *bar);
bool ft2_ui_scrollbar_step(ft2_ui_scrollbar *bar,int amount);
void ft2_ui_scrollbar_draw(ft2_ui_surface *s,const ft2_ui_scrollbar *bar,uint8_t face,uint8_t light,uint8_t dark,uint8_t thumb);
