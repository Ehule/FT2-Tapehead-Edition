#pragma once
#include "tapesister/ts_text_edit.h"
#include <stdbool.h>
#include <stddef.h>
#include "ft2_shared_ui.h"
#define TS_BROWSER_MAX_ENTRIES 256U
#define TS_BROWSER_NAME_BYTES 255U
typedef struct ts_browser_entry { char name[TS_BROWSER_NAME_BYTES+1U]; bool directory; } ts_browser_entry;
typedef enum ts_browser_mode { TS_BROWSER_SAVE, TS_BROWSER_LOAD, TS_BROWSER_BAKE } ts_browser_mode;
typedef enum ts_browser_focus { TS_BROWSER_FOCUS_LIST, TS_BROWSER_FOCUS_FILENAME } ts_browser_focus;
#define TS_BROWSER_VISIBLE_ROWS 10U
typedef enum ts_browser_key { TS_BROWSER_KEY_UP,TS_BROWSER_KEY_DOWN,TS_BROWSER_KEY_PAGE_UP,TS_BROWSER_KEY_PAGE_DOWN,TS_BROWSER_KEY_HOME,TS_BROWSER_KEY_END } ts_browser_key;
typedef struct ts_file_browser { char directory[TS_PATH_MAX_BYTES+1U];ts_browser_entry entries[TS_BROWSER_MAX_ENTRIES];size_t count,selected,scroll;ts_text_edit filename;ts_browser_mode mode;ts_browser_focus focus;char error[128];ft2_ui_scrollbar scrollbar; } ts_file_browser;
bool ts_file_browser_open(ts_file_browser *browser,ts_browser_mode mode,const char *directory,const char *name);
void ts_file_browser_close(ts_file_browser *browser);
bool ts_file_browser_home(ts_file_browser *browser);
bool ts_file_browser_root(ts_file_browser *browser);
bool ts_file_browser_parent(ts_file_browser *browser);
bool ts_file_browser_enter(ts_file_browser *browser,size_t index);
bool ts_file_browser_mkdir(ts_file_browser *browser,const char *name);
bool ts_file_browser_result(const ts_file_browser *browser,char *first,size_t first_capacity,char *second,size_t second_capacity);
bool ts_file_browser_move(ts_file_browser *browser,ts_browser_key key);
void ts_file_browser_ensure_visible(ts_file_browser *browser);
void ts_file_browser_toggle_focus(ts_file_browser *browser);
bool ts_file_browser_scroll_to(ts_file_browser *browser,size_t scroll);
bool ts_file_browser_mouse_press(ts_file_browser *browser,int x,int y,unsigned clicks);
bool ts_file_browser_mouse_motion(ts_file_browser *browser,int y);
void ts_file_browser_mouse_release(ts_file_browser *browser);
bool ts_file_browser_wheel(ts_file_browser *browser,int rows);
