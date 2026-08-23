#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "ft2_replayer.h"
#include "ft2_pattern_ed.h"
#include "ft2_sample_ed.h"
#include "ft2_audio.h"
#include "ft2_structs.h"
#include "ft2_gui.h"
#include "ft2_sample_launcher.h"
#include "ft2_fasttracks.h"
#include "ft2_undo.h"

#define UNDO_MAX_STEPS 128
#define UNDO_DEFAULT_MB 32
#define UNDO_MIN_MB 4
#define UNDO_MAX_MB 1024
#define UNDO_DESCRIPTION_LEN 64

typedef struct sampleSnapshot_t
{
	bool exists;
	sample_t meta;
	uint32_t dataBytes;
	int8_t *data;
} sampleSnapshot_t;

typedef struct instrumentSnapshot_t
{
	bool exists;
	char name[23];
	instr_t meta;
	sampleSnapshot_t samples[MAX_SMP_PER_INST];
} instrumentSnapshot_t;

typedef struct patternSnapshot_t
{
	bool exists;
	uint16_t patternNum;
	int16_t numRows;
	fastTracksPatternMetadata_t fastTracksMetadata;
	note_t *data;
} patternSnapshot_t;

typedef struct orderSnapshot_t
{
	uint8_t orders[MAX_ORDERS];
	int16_t songPos, row;
	uint16_t songLength, songLoopStart;
} orderSnapshot_t;

typedef struct patternChange_t
{
	patternSnapshot_t before, after;
} patternChange_t;

typedef struct sampleChange_t
{
	uint8_t instrNum, sampleNum;
	sampleSnapshot_t before, after;
} sampleChange_t;

typedef struct instrumentChange_t
{
	uint8_t instrNum;
	bool afterPrepared;
	instrumentSnapshot_t before, after;
} instrumentChange_t;

typedef struct undoEntry_t
{
	char description[UNDO_DESCRIPTION_LEN];
	uint32_t bytes;
	uint64_t beforeStateId, afterStateId;

	patternChange_t *patterns;
	uint16_t patternCount;

	sampleChange_t *samples;
	uint16_t sampleCount;

	instrumentChange_t *instruments;
	uint16_t instrumentCount;

	bool hasOrder;
	orderSnapshot_t beforeOrder, afterOrder;

	bool hasSampleLauncher;
	sampleLauncherUndoState_t beforeSampleLauncher, afterSampleLauncher;
} undoEntry_t;

static undoEntry_t history[UNDO_MAX_STEPS];
static int32_t historyCount, historyPos;
static uint64_t currentStateId = 1, savedStateId = 1, nextStateId = 2;
static uint32_t historyBytes;
static uint32_t memoryLimitBytes = UNDO_DEFAULT_MB * 1024U * 1024U;
static undoEntry_t pending;
static bool initialized, applyingHistory;

static void freeSampleSnapshot(sampleSnapshot_t *s)
{
	free(s->data);
	memset(s, 0, sizeof (*s));
}

static void freeInstrumentSnapshot(instrumentSnapshot_t *ins)
{
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
		freeSampleSnapshot(&ins->samples[i]);
	memset(ins, 0, sizeof (*ins));
}

static void freePatternSnapshot(patternSnapshot_t *p)
{
	free(p->data);
	memset(p, 0, sizeof (*p));
}

static void freeEntry(undoEntry_t *e)
{
	for (uint16_t i = 0; i < e->patternCount; i++)
	{
		freePatternSnapshot(&e->patterns[i].before);
		freePatternSnapshot(&e->patterns[i].after);
	}
	for (uint16_t i = 0; i < e->sampleCount; i++)
	{
		freeSampleSnapshot(&e->samples[i].before);
		freeSampleSnapshot(&e->samples[i].after);
	}
	for (uint16_t i = 0; i < e->instrumentCount; i++)
	{
		freeInstrumentSnapshot(&e->instruments[i].before);
		freeInstrumentSnapshot(&e->instruments[i].after);
	}
	free(e->patterns);
	free(e->samples);
	free(e->instruments);
	memset(e, 0, sizeof (*e));
}

void undoClear(void)
{
	for (int32_t i = 0; i < historyCount; i++)
		freeEntry(&history[i]);
	historyCount = historyPos = 0;
	historyBytes = 0;
	freeEntry(&pending);
}

static bool captureSampleFrom(sample_t *src, sampleSnapshot_t *dst)
{
	memset(dst, 0, sizeof (*dst));
	if (src == NULL)
		return true;

	dst->exists = true;
	dst->meta = *src;
	dst->meta.dataPtr = dst->meta.origDataPtr = NULL;
	if (src->dataPtr == NULL || src->length <= 0)
		return true;

	dst->dataBytes = (uint32_t)src->length << !!(src->flags & SAMPLE_16BIT);
	dst->data = (int8_t *)malloc(dst->dataBytes);
	if (dst->data == NULL)
		return false;

	unfixSample(src);
	memcpy(dst->data, src->dataPtr, dst->dataBytes);
	fixSample(src);
	return true;
}

static bool captureSample(uint8_t instrNum, uint8_t sampleNum,
	sampleSnapshot_t *dst)
{
	if (instrNum == 0 || instrNum > MAX_INST || sampleNum >= MAX_SMP_PER_INST ||
		instr[instrNum] == NULL)
	{
		memset(dst, 0, sizeof (*dst));
		return true;
	}
	return captureSampleFrom(&instr[instrNum]->smp[sampleNum], dst);
}

static bool captureInstrumentFrom(const char name[23], instr_t *source,
	instrumentSnapshot_t *dst)
{
	memset(dst, 0, sizeof (*dst));
	if (source == NULL)
		return true;

	dst->exists = true;
	if (name != NULL)
		memcpy(dst->name, name, sizeof (dst->name));
	dst->meta = *source;
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
	{
		memset(&dst->meta.smp[i], 0, sizeof (sample_t));
		if (!captureSampleFrom(&source->smp[i], &dst->samples[i]))
		{
			freeInstrumentSnapshot(dst);
			return false;
		}
	}
	return true;
}

static bool captureInstrument(uint8_t instrNum, instrumentSnapshot_t *dst)
{
	if (instrNum == 0 || instrNum > MAX_INST)
	{
		memset(dst, 0, sizeof (*dst));
		return true;
	}
	return captureInstrumentFrom(song.instrName[instrNum], instr[instrNum], dst);
}

static bool capturePattern(uint16_t patternNum, patternSnapshot_t *dst)
{
	memset(dst, 0, sizeof (*dst));
	if (patternNum >= MAX_PATTERNS)
		return false;

	dst->patternNum = patternNum;
	dst->numRows = patternNumRows[patternNum];
	fastTracksPOCGetPatternMetadata(patternNum, &dst->fastTracksMetadata);
	if (pattern[patternNum] == NULL)
		return true;

	dst->exists = true;
	const uint32_t bytes = (uint32_t)dst->numRows * TRACK_WIDTH;
	dst->data = (note_t *)malloc(bytes);
	if (dst->data == NULL)
		return false;
	memcpy(dst->data, pattern[patternNum], bytes);
	return true;
}

static void captureOrder(orderSnapshot_t *dst)
{
	memcpy(dst->orders, song.orders, sizeof (dst->orders));
	dst->songPos = song.songPos;
	dst->row = song.row;
	dst->songLength = song.songLength;
	dst->songLoopStart = song.songLoopStart;
}

static uint32_t patternSnapshotBytes(const patternSnapshot_t *p)
{
	return sizeof (*p) + (p->exists ? (uint32_t)p->numRows * TRACK_WIDTH : 0);
}

static uint32_t sampleSnapshotBytes(const sampleSnapshot_t *s)
{
	return sizeof (*s) + s->dataBytes;
}

static uint32_t instrumentSnapshotBytes(const instrumentSnapshot_t *ins)
{
	uint32_t bytes = sizeof (*ins);
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
		bytes += ins->samples[i].dataBytes;
	return bytes;
}

static bool patternsEqual(const patternSnapshot_t *a, const patternSnapshot_t *b)
{
	if (a->exists != b->exists || a->numRows != b->numRows ||
		memcmp(&a->fastTracksMetadata, &b->fastTracksMetadata,
			sizeof (a->fastTracksMetadata)) != 0)
		return false;
	if (!a->exists)
		return true;
	return memcmp(a->data, b->data, (uint32_t)a->numRows * TRACK_WIDTH) == 0;
}

static bool samplesEqual(const sampleSnapshot_t *a, const sampleSnapshot_t *b)
{
	if (a->exists != b->exists || a->dataBytes != b->dataBytes)
		return false;
	if (!a->exists)
		return true;
	if (memcmp(&a->meta, &b->meta, sizeof (sample_t)) != 0)
		return false;
	return a->dataBytes == 0 || memcmp(a->data, b->data, a->dataBytes) == 0;
}

static bool instrumentsEqual(const instrumentSnapshot_t *a, const instrumentSnapshot_t *b)
{
	if (a->exists != b->exists)
		return false;
	if (!a->exists)
		return true;
	if (memcmp(a->name, b->name, sizeof (a->name)) != 0 ||
		memcmp(&a->meta, &b->meta, sizeof (instr_t)) != 0)
	{
		return false;
	}
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
		if (!samplesEqual(&a->samples[i], &b->samples[i])) return false;
	return true;
}

static bool ordersEqual(const orderSnapshot_t *a, const orderSnapshot_t *b)
{
	return a->songPos == b->songPos && a->row == b->row &&
		a->songLength == b->songLength && a->songLoopStart == b->songLoopStart &&
		memcmp(a->orders, b->orders, sizeof (a->orders)) == 0;
}

static void removeOldest(void)
{
	if (historyCount <= 0)
		return;
	historyBytes -= history[0].bytes;
	freeEntry(&history[0]);
	memmove(&history[0], &history[1], (historyCount - 1) * sizeof (undoEntry_t));
	historyCount--;
	if (historyPos > 0)
		historyPos--;
	memset(&history[historyCount], 0, sizeof (undoEntry_t));
}

static void commitPending(void)
{
	while (historyCount > historyPos)
	{
		historyCount--;
		historyBytes -= history[historyCount].bytes;
		freeEntry(&history[historyCount]);
	}

	pending.afterStateId = nextStateId++;
	currentStateId = pending.afterStateId;

	if (pending.bytes > memoryLimitBytes)
	{
		/* The edit already happened, but this transaction cannot be retained.
		** Drop older history too so Undo can never skip across this mutation. */
		undoClear();
		return;
	}

	while (historyCount >= UNDO_MAX_STEPS || historyBytes + pending.bytes > memoryLimitBytes)
		removeOldest();

	history[historyCount++] = pending;
	historyPos = historyCount;
	historyBytes += pending.bytes;
	memset(&pending, 0, sizeof (pending));
}

bool undoTransactionIsActive(void)
{
	return pending.description[0] != '\0';
}

bool undoTransactionBegin(const char *description)
{
	undoInit();
	freeEntry(&pending);
	if (description == NULL || description[0] == '\0')
		description = "Edit";
	strncpy(pending.description, description, sizeof (pending.description)-1);
	pending.beforeStateId = currentStateId;
	return true;
}

bool undoTransactionAddPattern(uint16_t patternNum)
{
	if (!undoTransactionIsActive() || patternNum >= MAX_PATTERNS)
		return false;
	for (uint16_t i = 0; i < pending.patternCount; i++)
		if (pending.patterns[i].before.patternNum == patternNum) return true;

	patternChange_t *newList = realloc(pending.patterns,
		(pending.patternCount + 1) * sizeof (*newList));
	if (newList == NULL)
		return false;
	pending.patterns = newList;
	patternChange_t *change = &pending.patterns[pending.patternCount];
	memset(change, 0, sizeof (*change));
	if (!capturePattern(patternNum, &change->before))
		return false;
	pending.patternCount++;
	return true;
}

bool undoTransactionAddOrder(void)
{
	if (!undoTransactionIsActive())
		return false;
	if (!pending.hasOrder)
	{
		captureOrder(&pending.beforeOrder);
		pending.hasOrder = true;
	}
	return true;
}

bool undoTransactionAddSample(uint8_t instrNum, uint8_t sampleNum)
{
	if (!undoTransactionIsActive() || instrNum == 0 || instrNum > MAX_INST ||
		sampleNum >= MAX_SMP_PER_INST)
	{
		return false;
	}
	for (uint16_t i = 0; i < pending.instrumentCount; i++)
		if (pending.instruments[i].instrNum == instrNum) return true;
	for (uint16_t i = 0; i < pending.sampleCount; i++)
		if (pending.samples[i].instrNum == instrNum && pending.samples[i].sampleNum == sampleNum) return true;

	sampleChange_t *newList = realloc(pending.samples,
		(pending.sampleCount + 1) * sizeof (*newList));
	if (newList == NULL)
		return false;
	pending.samples = newList;
	sampleChange_t *change = &pending.samples[pending.sampleCount];
	memset(change, 0, sizeof (*change));
	change->instrNum = instrNum;
	change->sampleNum = sampleNum;
	if (!captureSample(instrNum, sampleNum, &change->before))
		return false;
	pending.sampleCount++;
	return true;
}

bool undoTransactionAddInstrument(uint8_t instrNum)
{
	if (!undoTransactionIsActive() || instrNum == 0 || instrNum > MAX_INST)
		return false;
	for (uint16_t i = 0; i < pending.instrumentCount; i++)
		if (pending.instruments[i].instrNum == instrNum) return true;
	for (uint16_t i = 0; i < pending.sampleCount; i++)
		if (pending.samples[i].instrNum == instrNum) return false;

	instrumentChange_t *newList = realloc(pending.instruments,
		(pending.instrumentCount + 1) * sizeof (*newList));
	if (newList == NULL)
		return false;
	pending.instruments = newList;
	instrumentChange_t *change = &pending.instruments[pending.instrumentCount];
	memset(change, 0, sizeof (*change));
	change->instrNum = instrNum;
	if (!captureInstrument(instrNum, &change->before))
		return false;
	pending.instrumentCount++;
	return true;
}

bool undoTransactionPrepareInstrumentAfter(uint8_t instrNum,
	const char name[23], instr_t *instrument)
{
	if (!undoTransactionIsActive() || instrNum == 0 || instrNum > MAX_INST ||
		instrument == NULL)
	{
		return false;
	}
	for (uint16_t i = 0; i < pending.instrumentCount; i++)
	{
		instrumentChange_t *change = &pending.instruments[i];
		if (change->instrNum != instrNum)
			continue;
		freeInstrumentSnapshot(&change->after);
		change->afterPrepared = false;
		if (!captureInstrumentFrom(name, instrument, &change->after))
			return false;
		change->afterPrepared = true;
		return true;
	}
	return false;
}

bool undoTransactionPreparedInstrumentsFitMemoryLimit(void)
{
	if (!undoTransactionIsActive() || pending.instrumentCount == 0 ||
		pending.patternCount != 0 || pending.sampleCount != 0 ||
		pending.hasOrder || pending.hasSampleLauncher)
	{
		return false;
	}
	uint64_t bytes = sizeof (undoEntry_t);
	for (uint16_t i = 0; i < pending.instrumentCount; i++)
	{
		const instrumentChange_t *change = &pending.instruments[i];
		if (!change->afterPrepared)
			return false;
		bytes += instrumentSnapshotBytes(&change->before);
		bytes += instrumentSnapshotBytes(&change->after);
		if (bytes > memoryLimitBytes)
			return false;
	}
	return true;
}

bool undoTransactionAddSampleLauncher(void)
{
	if (!undoTransactionIsActive())
		return false;
	if (!pending.hasSampleLauncher)
	{
		sampleLauncherCaptureUndoState(&pending.beforeSampleLauncher);
		pending.hasSampleLauncher = true;
	}
	return true;
}

void undoTransactionCommit(void)
{
	if (!undoTransactionIsActive())
		return;

	bool changed = false;
	uint32_t bytes = sizeof (undoEntry_t);
	for (uint16_t i = 0; i < pending.patternCount; i++)
	{
		patternChange_t *change = &pending.patterns[i];
		if (!capturePattern(change->before.patternNum, &change->after))
		{
			currentStateId = nextStateId++;
			undoClear();
			return;
		}
		changed |= !patternsEqual(&change->before, &change->after);
		bytes += patternSnapshotBytes(&change->before) + patternSnapshotBytes(&change->after);
	}
	for (uint16_t i = 0; i < pending.sampleCount; i++)
	{
		sampleChange_t *change = &pending.samples[i];
		if (!captureSample(change->instrNum, change->sampleNum, &change->after))
		{
			currentStateId = nextStateId++;
			undoClear();
			return;
		}
		changed |= !samplesEqual(&change->before, &change->after);
		bytes += sampleSnapshotBytes(&change->before) + sampleSnapshotBytes(&change->after);
	}
	for (uint16_t i = 0; i < pending.instrumentCount; i++)
	{
		instrumentChange_t *change = &pending.instruments[i];
		if (!change->afterPrepared &&
			!captureInstrument(change->instrNum, &change->after))
		{
			currentStateId = nextStateId++;
			undoClear();
			return;
		}
		changed |= !instrumentsEqual(&change->before, &change->after);
		bytes += instrumentSnapshotBytes(&change->before) + instrumentSnapshotBytes(&change->after);
	}
	if (pending.hasOrder)
	{
		captureOrder(&pending.afterOrder);
		changed |= !ordersEqual(&pending.beforeOrder, &pending.afterOrder);
		bytes += sizeof (orderSnapshot_t) * 2;
	}
	if (pending.hasSampleLauncher)
	{
		sampleLauncherCaptureUndoState(&pending.afterSampleLauncher);
		changed |= memcmp(&pending.beforeSampleLauncher, &pending.afterSampleLauncher,
			sizeof (sampleLauncherUndoState_t)) != 0;
		bytes += sizeof (sampleLauncherUndoState_t) * 2;
	}

	if (!changed)
	{
		freeEntry(&pending);
		return;
	}

	pending.bytes = bytes;
	commitPending();
}

bool undoPatternBegin(uint16_t patternNum, const char *description)
{
	if (!undoTransactionBegin(description) || !undoTransactionAddPattern(patternNum))
	{
		undoCancelTransaction();
		return false;
	}
	return true;
}

void undoPatternCommit(void) { undoTransactionCommit(); }

bool undoPatternInsertBegin(uint16_t patternNum, const char *description)
{
	if (!undoTransactionBegin(description) || !undoTransactionAddOrder() ||
		!undoTransactionAddPattern(patternNum))
	{
		undoCancelTransaction();
		return false;
	}
	return true;
}

void undoPatternInsertCommit(void) { undoTransactionCommit(); }

bool undoSongBegin(const char *description)
{
	if (!undoTransactionBegin(description))
		return false;
	for (uint16_t i = 0; i < MAX_PATTERNS; i++)
	{
		if (!undoTransactionAddPattern(i))
		{
			undoCancelTransaction();
			return false;
		}
	}
	return true;
}

void undoSongCommit(void) { undoTransactionCommit(); }

bool undoSampleBegin(uint8_t instrNum, uint8_t sampleNum, const char *description)
{
	if (!undoTransactionBegin(description) || !undoTransactionAddSample(instrNum, sampleNum))
	{
		undoCancelTransaction();
		return false;
	}
	return true;
}

void undoSampleCommit(void) { undoTransactionCommit(); }

bool undoInstrumentBegin(uint8_t instrNum, const char *description)
{
	if (!undoTransactionBegin(description) || !undoTransactionAddInstrument(instrNum))
	{
		undoCancelTransaction();
		return false;
	}
	return true;
}

void undoInstrumentCommit(void) { undoTransactionCommit(); }

void undoCancelTransaction(void)
{
	freeEntry(&pending);
}

void undoNotifyProjectMutation(void)
{
	if (!initialized || applyingHistory || undoTransactionIsActive())
		return;

	/* A persistent edit happened without a transaction. Old history can no
	** longer be trusted to describe the immediately preceding project state. */
	currentStateId = nextStateId++;
	if (historyCount > 0)
		undoClear();
}

uint64_t undoGetCurrentStateId(void)
{
	return currentStateId;
}

void undoMarkSavedState(uint64_t stateId)
{
	savedStateId = stateId;
	song.isModified = currentStateId != savedStateId;
	editor.updateWindowTitle = true;
}

void undoResetForLoadedProject(void)
{
	undoClear();
	currentStateId = nextStateId++;
	savedStateId = currentStateId;
}

static void syncSongModifiedToSavepoint(void)
{
	song.isModified = currentStateId != savedStateId;
	editor.updateWindowTitle = true;
}

static bool restoreSample(uint8_t instrNum, uint8_t sampleNum, const sampleSnapshot_t *src)
{
	if (instrNum == 0 || instrNum > MAX_INST || sampleNum >= MAX_SMP_PER_INST)
		return false;
	if (instr[instrNum] == NULL && !allocateInstr(instrNum))
		return false;
	sample_t *dst = &instr[instrNum]->smp[sampleNum];
	freeSmpData(dst);
	memset(dst, 0, sizeof (*dst));
	if (!src->exists)
		return true;

	*dst = src->meta;
	dst->dataPtr = dst->origDataPtr = NULL;
	if (src->dataBytes > 0)
	{
		if (!allocateSmpData(dst, dst->length, !!(dst->flags & SAMPLE_16BIT)))
			return false;
		memcpy(dst->dataPtr, src->data, src->dataBytes);
		fixSample(dst);
	}
	return true;
}

static bool restoreInstrument(uint8_t instrNum, const instrumentSnapshot_t *src)
{
	freeInstr(instrNum);
	memset(song.instrName[instrNum], 0, sizeof (song.instrName[instrNum]));
	if (!src->exists)
		return true;
	if (!allocateInstr(instrNum))
		return false;
	memcpy(song.instrName[instrNum], src->name, sizeof (src->name));
	*instr[instrNum] = src->meta;
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
	{
		memset(&instr[instrNum]->smp[i], 0, sizeof (sample_t));
		if (!restoreSample(instrNum, (uint8_t)i, &src->samples[i]))
			return false;
	}
	return true;
}

static bool restorePattern(const patternSnapshot_t *src)
{
	setPatternLen(src->patternNum, src->numRows);
	fastTracksPOCSetPatternMetadata(src->patternNum, &src->fastTracksMetadata);
	if (!src->exists)
	{
		if (pattern[src->patternNum] != NULL)
		{
			memset(pattern[src->patternNum], 0, (uint32_t)src->numRows * TRACK_WIDTH);
			killPatternIfUnused(src->patternNum);
		}
		return true;
	}
	if (!allocatePattern(src->patternNum))
		return false;
	memcpy(pattern[src->patternNum], src->data, (uint32_t)src->numRows * TRACK_WIDTH);
	return true;
}

static void restoreOrderData(const orderSnapshot_t *src)
{
	memcpy(song.orders, src->orders, sizeof (src->orders));
	song.songLength = src->songLength;
	song.songLoopStart = src->songLoopStart;
}

static void restoreOrderPosition(const orderSnapshot_t *src)
{
	setSongPos(src->songPos, src->row, DONT_RESET_SONG_TICK);
}

static bool entryTouchesMappedInstrument(const undoEntry_t *e)
{
	for (uint16_t i = 0; i < e->sampleCount; i++)
		if (sampleLauncherInstrumentIsMapped(e->samples[i].instrNum)) return true;
	for (uint16_t i = 0; i < e->instrumentCount; i++)
		if (sampleLauncherInstrumentIsMapped(e->instruments[i].instrNum)) return true;
	return false;
}

static bool applyEntry(const undoEntry_t *e, bool after)
{
	bool ok = true;
	if (e->hasSampleLauncher || entryTouchesMappedInstrument(e))
		sampleLauncherReset();

	pauseAudio();
	if (e->hasOrder && !after)
		restoreOrderData(&e->beforeOrder);

	for (uint16_t i = 0; i < e->patternCount; i++)
	{
		const patternSnapshot_t *state = after ? &e->patterns[i].after : &e->patterns[i].before;
		if (!restorePattern(state)) { ok = false; break; }
	}
	if (ok)
	{
		for (uint16_t i = 0; i < e->instrumentCount; i++)
		{
			const instrumentSnapshot_t *state = after ? &e->instruments[i].after : &e->instruments[i].before;
			if (!restoreInstrument(e->instruments[i].instrNum, state)) { ok = false; break; }
		}
	}
	if (ok)
	{
		for (uint16_t i = 0; i < e->sampleCount; i++)
		{
			const sampleSnapshot_t *state = after ? &e->samples[i].after : &e->samples[i].before;
			if (!restoreSample(e->samples[i].instrNum, e->samples[i].sampleNum, state)) { ok = false; break; }
		}
	}
	if (ok && e->hasSampleLauncher)
		sampleLauncherRestoreUndoState(after ? &e->afterSampleLauncher : &e->beforeSampleLauncher);
	if (ok && e->hasOrder)
	{
		const orderSnapshot_t *order = after ? &e->afterOrder : &e->beforeOrder;
		if (after)
			restoreOrderData(order);
		restoreOrderPosition(order);
	}
	resumeAudio();

	if (ok)
	{
		ui.updatePatternEditor = true;
		if (e->hasOrder)
		{
			ui.updatePosSections = true;
			ui.updatePosEdScrollBar = true;
		}
		editor.updateCurInstr = true;
		editor.updateCurSmp = true;
		if (ui.sampleEditorShown)
			updateSampleEditorSample();
	}
	return ok;
}

void undoPerform(void)
{
	undoInit();
	if (undoTransactionIsActive())
		undoCancelTransaction();
	if (historyPos <= 0)
		return;
	applyingHistory = true;
	const bool ok = applyEntry(&history[historyPos-1], false);
	applyingHistory = false;
	if (ok)
	{
		historyPos--;
		currentStateId = history[historyPos].beforeStateId;
		syncSongModifiedToSavepoint();
	}
	else
	{
		currentStateId = nextStateId++;
		undoClear();
	}
}

void redoPerform(void)
{
	undoInit();
	if (undoTransactionIsActive())
		undoCancelTransaction();
	if (historyPos >= historyCount)
		return;
	applyingHistory = true;
	const bool ok = applyEntry(&history[historyPos], true);
	applyingHistory = false;
	if (ok)
	{
		currentStateId = history[historyPos].afterStateId;
		historyPos++;
		syncSongModifiedToSavepoint();
	}
	else
	{
		currentStateId = nextStateId++;
		undoClear();
	}
}

void undoLoadConfig(void)
{
	const uint32_t mb = CLAMP(tapeheadConfig.undoMemoryMB, UNDO_MIN_MB, UNDO_MAX_MB);
	memoryLimitBytes = mb * 1024U * 1024U;
	while (historyBytes > memoryLimitBytes)
		removeOldest();
}

void undoInit(void)
{
	if (initialized)
		return;
	initialized = true;
	undoLoadConfig();
}

void undoClose(void)
{
	undoClear();
	initialized = false;
}
