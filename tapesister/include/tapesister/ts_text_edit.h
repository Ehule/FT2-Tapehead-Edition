#pragma once
#include <stdbool.h>
#include <stddef.h>
#define TS_PATH_MAX_BYTES 1023U
typedef struct ts_text_edit { char *text; size_t capacity, length, cursor, scroll; } ts_text_edit;
bool ts_text_edit_init(ts_text_edit *edit, size_t capacity, const char *initial);
void ts_text_edit_destroy(ts_text_edit *edit);
bool ts_text_edit_insert(ts_text_edit *edit, const char *utf8);
bool ts_text_edit_backspace(ts_text_edit *edit);
bool ts_text_edit_delete(ts_text_edit *edit);
void ts_text_edit_left(ts_text_edit *edit);
void ts_text_edit_right(ts_text_edit *edit);
void ts_text_edit_home(ts_text_edit *edit);
void ts_text_edit_end(ts_text_edit *edit);
bool ts_utf8_valid(const char *text, size_t length);
