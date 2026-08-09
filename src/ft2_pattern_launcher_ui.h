#pragma once

#include <stdint.h>
#include <stdbool.h>

bool patternLauncherPanelIsShown(void);
bool patternLauncherPanelIsSampleDeck(void);
bool patternLauncherDeckIsSample(void);
bool patternLauncherStandaloneIsShown(void);
void patternLauncherSetStandaloneShown(bool shown);
void patternLauncherDrawStandalone(void);
bool patternLauncherHandleStandaloneClick(int16_t x, int16_t y,
	uint8_t mouseButton, bool shiftPressed);
bool patternLauncherHandleStandaloneWheel(int16_t x, int16_t y,
	bool directionUp);
bool patternLauncherHandleStandaloneSpace(void);
void patternLauncherToggleDeck(void);
void patternLauncherSetPanelShown(bool shown);
void patternLauncherDrawPanel(void);
void patternLauncherSetPage(uint8_t page);
uint8_t patternLauncherGetPage(void);
void patternLauncherSetDeckMode(bool sampleDeck);
bool patternLauncherPatternIsExposed(uint8_t patternNum);
bool patternLauncherHandlePanelClick(int16_t x, int16_t y);
bool patternLauncherHandlePanelMiddleClick(int16_t x, int16_t y, bool shiftPressed);
void handlePatternLauncherPanelRefresh(void);
void patternLauncherForceRedraw(void);
void patternLauncherNotifySongOrderChanged(void);
void patternLauncherNotifyPatternChanged(uint16_t patternNum);
void patternLauncherResetExposure(void);
