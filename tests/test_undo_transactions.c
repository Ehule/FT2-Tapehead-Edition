#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_config.h"
#include "ft2_replayer.h"
#include "ft2_sample_launcher.h"
#include "ft2_structs.h"
#include "ft2_undo.h"

song_t song;
editor_t editor;
ui_t ui;
tapeheadConfig_t tapeheadConfig;
instr_t *instr[MAX_INST + 4];
note_t *pattern[MAX_PATTERNS];
int16_t patternNumRows[MAX_PATTERNS];

static sampleLauncherUndoState_t launcherState;

bool allocatePattern(uint16_t patternNum)
{
    if (pattern[patternNum] != NULL)
        return true;
    pattern[patternNum] = calloc(MAX_PATT_LEN * MAX_CHANNELS, sizeof (note_t));
    return pattern[patternNum] != NULL;
}

void setPatternLen(uint16_t patternNum, int16_t numRows)
{
    patternNumRows[patternNum] = numRows;
}

void killPatternIfUnused(uint16_t patternNum)
{
    free(pattern[patternNum]);
    pattern[patternNum] = NULL;
}

bool allocateInstr(int16_t instrNum)
{
    if (instr[instrNum] != NULL)
        return true;
    instr[instrNum] = calloc(1, sizeof (instr_t));
    return instr[instrNum] != NULL;
}

void freeSmpData(sample_t *s)
{
    free(s->origDataPtr);
    s->origDataPtr = s->dataPtr = NULL;
}

void freeInstr(int32_t instrNum)
{
    if (instr[instrNum] != NULL)
    {
        for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
            freeSmpData(&instr[instrNum]->smp[i]);
        free(instr[instrNum]);
        instr[instrNum] = NULL;
    }
}

bool allocateSmpData(sample_t *s, int32_t length, bool sample16Bit)
{
    const size_t bytes = (size_t)length << sample16Bit;
    s->origDataPtr = malloc(bytes == 0 ? 1 : bytes);
    if (s->origDataPtr == NULL)
    {
        s->dataPtr = NULL;
        return false;
    }
    s->dataPtr = s->origDataPtr;
    return true;
}

void fixSample(sample_t *s) { (void)s; }
void unfixSample(sample_t *s) { (void)s; }
void pauseAudio(void) { }
void resumeAudio(void) { }
void updateSampleEditorSample(void) { }

void setSongPos(int16_t songPos, int16_t row, bool resetTick)
{
    (void)resetTick;
    if (songPos >= 0)
    {
        song.songPos = songPos;
        song.pattNum = song.orders[songPos];
    }
    if (row >= 0)
        song.row = row;
}

void setSongModifiedFlag(void)
{
    undoNotifyProjectMutation();
    song.isModified = true;
}

void sampleLauncherCaptureUndoState(sampleLauncherUndoState_t *state)
{
    *state = launcherState;
}

void sampleLauncherRestoreUndoState(const sampleLauncherUndoState_t *state)
{
    launcherState = *state;
}

bool sampleLauncherInstrumentIsMapped(uint8_t instrument)
{
    (void)instrument;
    return false;
}

void sampleLauncherReset(void) { }

static void resetFixture(void)
{
    undoClear();
    for (int32_t i = 0; i < MAX_PATTERNS; i++)
    {
        free(pattern[i]);
        pattern[i] = NULL;
        patternNumRows[i] = 64;
    }
    for (int32_t i = 1; i <= MAX_INST; i++)
        freeInstr(i);
    memset(&song, 0, sizeof (song));
    memset(&editor, 0, sizeof (editor));
    memset(&ui, 0, sizeof (ui));
    memset(&launcherState, 0, sizeof (launcherState));
    song.songLength = 1;
    song.orders[0] = 0;
    song.numChannels = 8;
    song.currNumRows = 64;
    assert(allocatePattern(0));
    undoResetForLoadedProject();
    song.isModified = false;
}

static void test_pattern_undo_redo_and_no_skip(void)
{
    resetFixture();
    assert(undoPatternBegin(0, "Enter note"));
    pattern[0][0].note = 24;
    setSongModifiedFlag();
    undoPatternCommit();

    undoPerform();
    assert(pattern[0][0].note == 0);
    redoPerform();
    assert(pattern[0][0].note == 24);

    assert(undoPatternBegin(0, "Enter note"));
    pattern[0][0].note = 36;
    setSongModifiedFlag();
    undoPatternCommit();

    /* Unsupported project mutation: this must invalidate older history so
    ** Undo cannot jump backward over the newer change. */
    pattern[0][0].note = 48;
    setSongModifiedFlag();
    undoPerform();
    assert(pattern[0][0].note == 48);
}

static void test_composite_transaction(void)
{
    resetFixture();
    launcherState.tileState[0] = 1;

    assert(undoTransactionBegin("Composite edit"));
    assert(undoTransactionAddPattern(0));
    assert(undoTransactionAddOrder());
    assert(undoTransactionAddInstrument(1));
    assert(undoTransactionAddSampleLauncher());

    pattern[0][0].note = 60;
    song.orders[0] = 7;
    song.songLength = 2;
    assert(allocateInstr(1));
    strcpy(song.instrName[1], "Created");
    instr[1]->smp[0].length = 4;
    instr[1]->smp[0].volume = 64;
    assert(allocateSmpData(&instr[1]->smp[0], 4, false));
    memcpy(instr[1]->smp[0].dataPtr, "ABCD", 4);
    launcherState.tileState[0] = 2;

    setSongModifiedFlag();
    undoTransactionCommit();

    undoPerform();
    assert(pattern[0][0].note == 0);
    assert(song.orders[0] == 0);
    assert(song.songLength == 1);
    assert(instr[1] == NULL);
    assert(launcherState.tileState[0] == 1);

    redoPerform();
    assert(pattern[0][0].note == 60);
    assert(song.orders[0] == 7);
    assert(song.songLength == 2);
    assert(instr[1] != NULL);
    assert(strcmp(song.instrName[1], "Created") == 0);
    assert(instr[1]->smp[0].length == 4);
    assert(memcmp(instr[1]->smp[0].dataPtr, "ABCD", 4) == 0);
    assert(launcherState.tileState[0] == 2);
}

static void test_saved_state_checkpoint(void)
{
    resetFixture();

    assert(undoPatternBegin(0, "Edit before save"));
    pattern[0][0].note = 24;
    setSongModifiedFlag();
    undoPatternCommit();
    assert(song.isModified);

    const uint64_t savedState = undoGetCurrentStateId();
    song.isModified = false; /* mirrors successful module saver */
    undoMarkSavedState(savedState);
    assert(!song.isModified);

    undoPerform();
    assert(pattern[0][0].note == 0);
    assert(song.isModified);

    redoPerform();
    assert(pattern[0][0].note == 24);
    assert(!song.isModified);

    /* Simulate an asynchronous save finishing after another edit occurred.
    ** The saved token must describe the state that save started from, not the
    ** newer current history position. */
    const uint64_t asyncSaveState = undoGetCurrentStateId();
    assert(undoPatternBegin(0, "Edit during save"));
    pattern[0][0].note = 36;
    setSongModifiedFlag();
    undoPatternCommit();
    undoMarkSavedState(asyncSaveState);
    assert(song.isModified);

    undoPerform();
    assert(pattern[0][0].note == 24);
    assert(!song.isModified);

    /* A non-undoable project mutation invalidates history, but it must also
    ** remain distinguishable from the saved checkpoint after later undo. */
    pattern[0][0].note = 48;
    setSongModifiedFlag();
    assert(song.isModified);

    assert(undoPatternBegin(0, "Edit after barrier"));
    pattern[0][0].note = 60;
    setSongModifiedFlag();
    undoPatternCommit();
    undoPerform();
    assert(pattern[0][0].note == 48);
    assert(song.isModified);
}

static void test_oversized_transaction_becomes_barrier(void)
{
    resetFixture();
    tapeheadConfig.undoMemoryMB = 4;
    undoLoadConfig();

    assert(undoPatternBegin(0, "Earlier edit"));
    pattern[0][0].note = 12;
    setSongModifiedFlag();
    undoPatternCommit();

    assert(allocateInstr(2));
    sample_t *s = &instr[2]->smp[0];
    s->length = 3 * 1024 * 1024;
    assert(allocateSmpData(s, s->length, false));
    memset(s->dataPtr, 1, (size_t)s->length);

    assert(undoSampleBegin(2, 0, "Large sample edit"));
    s->dataPtr[0] = 2;
    setSongModifiedFlag();
    undoSampleCommit();

    /* before+after PCM exceeds the 4MB history budget. The current edit must
    ** remain intact and older history must become unreachable. */
    undoPerform();
    assert(s->dataPtr[0] == 2);
    assert(pattern[0][0].note == 12);

    tapeheadConfig.undoMemoryMB = 32;
    undoLoadConfig();
}

int main(void)
{
    tapeheadConfig.undoMemoryMB = 32;
    undoInit();
    test_pattern_undo_redo_and_no_skip();
    test_composite_transaction();
    test_saved_state_checkpoint();
    test_oversized_transaction_becomes_barrier();
    resetFixture();
    undoClose();
    puts("Undo transaction tests passed.");
    return 0;
}
