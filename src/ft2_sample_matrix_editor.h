#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ft2_unicode.h"

#define SAMPLE_MATRIX_BROWSER_VISIBLE_ROWS 17

bool sampleMatrixBrowserOpen(void);
void sampleMatrixBrowserClose(void);
bool sampleMatrixBrowserRefresh(void);
bool sampleMatrixBrowserGoParent(void);
bool sampleMatrixBrowserOpenDirectory(uint32_t index);
uint32_t sampleMatrixBrowserGetCount(void);
uint32_t sampleMatrixBrowserGetScroll(void);
void sampleMatrixBrowserSetScroll(uint32_t scroll);
void sampleMatrixBrowserScroll(int32_t amount);
const char *sampleMatrixBrowserGetName(uint32_t index);
bool sampleMatrixBrowserEntryIsDirectory(uint32_t index);
bool sampleMatrixBrowserEntryIsSelected(uint32_t index);
void sampleMatrixBrowserSelect(uint32_t index, bool toggle, bool range);
void sampleMatrixBrowserClearSelection(void);
uint32_t sampleMatrixBrowserGetSelectionCount(void);
const UNICHAR *sampleMatrixBrowserGetSelectedName(uint32_t selectedIndex);
uint32_t sampleMatrixBrowserGetFileCount(void);
const UNICHAR *sampleMatrixBrowserGetFileName(uint32_t fileIndex);
const UNICHAR *sampleMatrixBrowserGetPath(void);
const char *sampleMatrixBrowserGetDisplayPath(void);
