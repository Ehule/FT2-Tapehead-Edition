#pragma once

#include <stdint.h>
#include <stdbool.h>

bool patternLauncherPanelIsShown(void);
void patternLauncherSetPanelShown(bool shown);
void patternLauncherDrawPanel(void);
void patternLauncherSetPage(uint8_t page);
bool patternLauncherHandlePanelClick(int16_t x, int16_t y);
bool patternLauncherHandlePanelMiddleClick(int16_t x, int16_t y, bool shiftPressed);
void handlePatternLauncherPanelRefresh(void);
void patternLauncherForceRedraw(void);
void patternLauncherNotifySongOrderChanged(void);
void patternLauncherNotifyPatternChanged(uint16_t patternNum);
