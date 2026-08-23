#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct instr_t instr_t;

void undoInit(void);
void undoClose(void);
void undoClear(void);
void undoLoadConfig(void);

/* Generic application-wide transaction API. Add every persistent object that
** a logical command can mutate before changing any of them, then commit once. */
bool undoTransactionBegin(const char *description);
bool undoTransactionAddPattern(uint16_t patternNum);
bool undoTransactionAddOrder(void);
bool undoTransactionAddSample(uint8_t instrNum, uint8_t sampleNum);
bool undoTransactionAddInstrument(uint8_t instrNum);
/* Stage an instrument transaction's post-edit snapshot before mutating the
** live song. This lets atomic batch imports prove that all Undo allocations
** and the configured memory limit are satisfied first. */
bool undoTransactionPrepareInstrumentAfter(uint8_t instrNum,
	const char name[23], instr_t *instrument);
bool undoTransactionPreparedInstrumentsFitMemoryLimit(void);
bool undoTransactionAddSampleLauncher(void);
void undoTransactionCommit(void);
bool undoTransactionIsActive(void);

/* Compatibility wrappers used by existing single-object edit paths. */
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

/* Called by setSongModifiedFlag(). If a persistent mutation happens outside an
** undo transaction, old history is invalidated instead of silently skipping
** across the unrecorded edit. */
void undoNotifyProjectMutation(void);
/* Mark the current undo position as the exact state represented by the most
** recently loaded/saved module. Undo/redo uses this checkpoint to keep the
** window modified marker accurate when returning to the saved state. */
uint64_t undoGetCurrentStateId(void);
void undoMarkSavedState(uint64_t stateId);
void undoResetForLoadedProject(void);

void undoPerform(void);
void redoPerform(void);
