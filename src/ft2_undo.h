#pragma once

#include <stdint.h>
#include <stdbool.h>

void undoInit(void);
void undoClose(void);
void undoClear(void);
void undoLoadConfig(void);

bool undoPatternBegin(uint16_t patternNum, const char *description);
void undoPatternCommit(void);
bool undoPatternInsertBegin(uint16_t patternNum, const char *description);
void undoPatternInsertCommit(void);
bool undoSongBegin(const char *description);
void undoSongCommit(void);
bool undoSampleBegin(uint8_t instrNum, uint8_t sampleNum, const char *description);
void undoSampleCommit(void);
bool undoInstrumentBegin(uint8_t instrNum, const char *description);
void undoInstrumentCommit(void);
void undoCancelTransaction(void);

void undoPerform(void);
void redoPerform(void);
