#pragma once

#include <stdint.h>
#include <stdbool.h>

enum
{
	ASK_TYPE_QUIT = 0,
	ASK_TYPE_LOAD_SONG = 1,
};

#define SYSREQ_TYPE_FOLDER_IMPORT 9
#define SYSREQ_TYPE_BAKE_MODULE 10
#define SYSREQ_TYPE_EXS_EXPORT 11
#define SYSREQ_TYPE_FOLDER_SCOPE 12
#define SYSREQ_TYPE_BAKE_OUTPUT 13
#define SYSREQ_TYPE_TAPESISTER_MENU 14
#define SYSREQ_TYPE_TAPESISTER_PUBLISH 15
#define SYSREQ_TYPE_TAPESISTER_REPLACE 16
#define SYSREQ_TYPE_TAPESISTER_IMPORT 17

// for thread-safe version of okBox()
typedef struct okBoxData_t
{
	volatile bool active;
	int16_t type, returnData;
	const char *headline, *text;
	void (*checkBoxCallback)(void);
} okBoxData_t;

#define SYSREQ_MAX_MESSAGE_LINES 32
#define SYSREQ_MAX_MESSAGE_LENGTH 512

typedef struct systemRequestLayout_t
{
	char lines[SYSREQ_MAX_MESSAGE_LINES][SYSREQ_MAX_MESSAGE_LENGTH];
	uint16_t lineWidths[SYSREQ_MAX_MESSAGE_LINES];
	int16_t lineX[SYSREQ_MAX_MESSAGE_LINES];
	uint16_t lineCount, frameX, frameY, frameWidth, frameHeight;
	uint16_t headlineX, textY, buttonY;
} systemRequestLayout_t;

bool systemRequestCalculateLayout(const char *headline, const char *text,
	uint16_t buttonSpan, uint16_t baseY, systemRequestLayout_t *layout);

// If the checkBoxCallback argument is set, then you get a "Do not show again" checkbox.
int16_t okBoxThreadSafe(int16_t type, const char *headline, const char *text, void (*checkBoxCallback)(void));
int16_t okBox(int16_t type, const char *headline, const char *text, void (*checkBoxCallback)(void));
int16_t choiceBoxWithCheckBox(int16_t type, const char *headline, const char *text,
	const char *checkBoxText, bool *checkBoxState);
int16_t bakerChoiceBox(int16_t type, const char *headline, const char *text,
	const char *checkBoxText, bool *checkBoxState, uint16_t *patternRows, uint16_t bpm);
// -----------

int16_t quitBox(bool skipQuitMsg);
int16_t inputBox(int16_t type, const char *headline, char *edText, uint16_t maxStrLen);
bool askUnsavedChanges(uint8_t type);

void myLoaderMsgBoxThreadSafe(const char *fmt, ...);
void myLoaderMsgBox(const char *fmt, ...);

 // ft2_sysreqs.c
extern okBoxData_t okBoxData;
extern void (*loaderMsgBox)(const char *, ...);
extern int16_t (*loaderSysReq)(int16_t, const char *, const char *, void (*)(void));
// ---------------
