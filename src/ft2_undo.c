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
#include "ft2_undo.h"

#define UNDO_MAX_STEPS 128
#define UNDO_DEFAULT_MB 32
#define UNDO_MIN_MB 4
#define UNDO_MAX_MB 1024

typedef enum undoType_t
{
	UNDO_NONE = 0,
	UNDO_PATTERN,
	UNDO_PATTERN_INSERT,
	UNDO_SONG,
	UNDO_SAMPLE,
	UNDO_INSTRUMENT
} undoType_t;

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
	note_t *data;
} patternSnapshot_t;

typedef struct orderSnapshot_t
{
	uint8_t orders[MAX_ORDERS];
	int16_t songPos, row;
	uint16_t songLength, songLoopStart;
} orderSnapshot_t;

typedef struct songSnapshot_t
{
	patternSnapshot_t patterns[MAX_PATTERNS];
} songSnapshot_t;

typedef struct undoEntry_t
{
	undoType_t type;
	char description[32];
	uint32_t bytes;
	union
	{
		struct { patternSnapshot_t before, after; } pattern;
		struct
		{
			orderSnapshot_t beforeOrder, afterOrder;
			patternSnapshot_t beforePattern, afterPattern;
		} patternInsert;
		struct { songSnapshot_t before, after; } song;
		struct { uint8_t instrNum, sampleNum; sampleSnapshot_t before, after; } sample;
		struct { uint8_t instrNum; instrumentSnapshot_t before, after; } instrument;
	} state;
} undoEntry_t;

static undoEntry_t history[UNDO_MAX_STEPS];
static int32_t historyCount, historyPos;
static uint32_t historyBytes;
static uint32_t memoryLimitBytes = UNDO_DEFAULT_MB * 1024U * 1024U;
static undoEntry_t pending;
static bool initialized;

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

static void freeSongSnapshot(songSnapshot_t *s)
{
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
		freePatternSnapshot(&s->patterns[i]);
}

static void freeEntry(undoEntry_t *e)
{
	if (e->type == UNDO_PATTERN)
	{
		freePatternSnapshot(&e->state.pattern.before);
		freePatternSnapshot(&e->state.pattern.after);
	}
	else if (e->type == UNDO_PATTERN_INSERT)
	{
		freePatternSnapshot(&e->state.patternInsert.beforePattern);
		freePatternSnapshot(&e->state.patternInsert.afterPattern);
	}
	else if (e->type == UNDO_SONG)
	{
		freeSongSnapshot(&e->state.song.before);
		freeSongSnapshot(&e->state.song.after);
	}
	else if (e->type == UNDO_SAMPLE)
	{
		freeSampleSnapshot(&e->state.sample.before);
		freeSampleSnapshot(&e->state.sample.after);
	}
	else if (e->type == UNDO_INSTRUMENT)
	{
		freeInstrumentSnapshot(&e->state.instrument.before);
		freeInstrumentSnapshot(&e->state.instrument.after);
	}
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

static bool captureSample(uint8_t instrNum, uint8_t sampleNum, sampleSnapshot_t *dst)
{
	memset(dst, 0, sizeof (*dst));
	if (instrNum == 0 || instr[instrNum] == NULL)
		return true;

	sample_t *src = &instr[instrNum]->smp[sampleNum];
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

static bool captureInstrument(uint8_t instrNum, instrumentSnapshot_t *dst)
{
	memset(dst, 0, sizeof (*dst));
	if (instrNum == 0 || instr[instrNum] == NULL)
		return true;

	dst->exists = true;
	memcpy(dst->name, song.instrName[instrNum], sizeof (dst->name));
	dst->meta = *instr[instrNum];
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
	{
		memset(&dst->meta.smp[i], 0, sizeof (sample_t));
		if (!captureSample(instrNum, (uint8_t)i, &dst->samples[i]))
		{
			freeInstrumentSnapshot(dst);
			return false;
		}
	}
	return true;
}

static bool capturePattern(uint16_t patternNum, patternSnapshot_t *dst)
{
	memset(dst, 0, sizeof (*dst));
	dst->patternNum = patternNum;
	dst->numRows = patternNumRows[patternNum];
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

static bool captureSong(songSnapshot_t *dst)
{
	memset(dst, 0, sizeof (*dst));
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		if (!capturePattern((uint16_t)i, &dst->patterns[i]))
		{
			freeSongSnapshot(dst);
			return false;
		}
	}
	return true;
}

static uint32_t patternSnapshotBytes(const patternSnapshot_t *p);

static uint32_t songSnapshotBytes(const songSnapshot_t *s)
{
	uint32_t bytes = 0;
	for (int32_t i = 0; i < MAX_PATTERNS; i++) bytes += patternSnapshotBytes(&s->patterns[i]);
	return bytes;
}

static uint32_t sampleSnapshotBytes(const sampleSnapshot_t *s) { return s->dataBytes; }
static uint32_t instrumentSnapshotBytes(const instrumentSnapshot_t *ins)
{
	uint32_t bytes = sizeof (instr_t);
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++) bytes += sampleSnapshotBytes(&ins->samples[i]);
	return bytes;
}
static uint32_t patternSnapshotBytes(const patternSnapshot_t *p)
{
	return p->exists ? (uint32_t)p->numRows * TRACK_WIDTH : 0;
}

static bool patternsEqual(const patternSnapshot_t *a, const patternSnapshot_t *b);

static bool ordersEqual(const orderSnapshot_t *a, const orderSnapshot_t *b)
{
	return a->songPos == b->songPos &&
		a->row == b->row &&
		a->songLength == b->songLength &&
		a->songLoopStart == b->songLoopStart &&
		memcmp(a->orders, b->orders, sizeof (a->orders)) == 0;
}

static bool songsEqual(const songSnapshot_t *a, const songSnapshot_t *b)
{
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
		if (!patternsEqual(&a->patterns[i], &b->patterns[i])) return false;
	return true;
}

static bool samplesEqual(const sampleSnapshot_t *a, const sampleSnapshot_t *b)
{
	if (a->exists != b->exists || a->dataBytes != b->dataBytes) return false;
	if (!a->exists) return true;
	if (memcmp(&a->meta, &b->meta, sizeof (sample_t)) != 0) return false;
	return a->dataBytes == 0 || memcmp(a->data, b->data, a->dataBytes) == 0;
}

static bool instrumentsEqual(const instrumentSnapshot_t *a, const instrumentSnapshot_t *b)
{
	if (a->exists != b->exists) return false;
	if (!a->exists) return true;
	if (memcmp(a->name, b->name, sizeof (a->name)) != 0) return false;
	if (memcmp(&a->meta, &b->meta, sizeof (instr_t)) != 0) return false;
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++) if (!samplesEqual(&a->samples[i], &b->samples[i])) return false;
	return true;
}

static bool patternsEqual(const patternSnapshot_t *a, const patternSnapshot_t *b)
{
	if (a->exists != b->exists || a->numRows != b->numRows) return false;
	if (!a->exists) return true;
	return memcmp(a->data, b->data, (uint32_t)a->numRows * TRACK_WIDTH) == 0;
}

static void removeOldest(void)
{
	if (historyCount <= 0) return;
	historyBytes -= history[0].bytes;
	freeEntry(&history[0]);
	memmove(&history[0], &history[1], (historyCount - 1) * sizeof (undoEntry_t));
	historyCount--;
	if (historyPos > 0) historyPos--;
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

	if (pending.bytes > memoryLimitBytes)
	{
		freeEntry(&pending);
		return;
	}

	while (historyCount >= UNDO_MAX_STEPS || historyBytes + pending.bytes > memoryLimitBytes)
		removeOldest();

	history[historyCount++] = pending;
	historyPos = historyCount;
	historyBytes += pending.bytes;
	memset(&pending, 0, sizeof (pending));
}

bool undoPatternBegin(uint16_t patternNum, const char *description)
{
	undoInit();
	freeEntry(&pending);
	pending.type = UNDO_PATTERN;
	strncpy(pending.description, description, sizeof (pending.description)-1);
	if (!capturePattern(patternNum, &pending.state.pattern.before))
	{
		freeEntry(&pending);
		return false;
	}
	return true;
}

void undoPatternCommit(void)
{
	if (pending.type != UNDO_PATTERN) return;
	if (!capturePattern(pending.state.pattern.before.patternNum, &pending.state.pattern.after) ||
		patternsEqual(&pending.state.pattern.before, &pending.state.pattern.after))
	{
		freeEntry(&pending);
		return;
	}
	pending.bytes = patternSnapshotBytes(&pending.state.pattern.before) + patternSnapshotBytes(&pending.state.pattern.after);
	commitPending();
}

bool undoPatternInsertBegin(uint16_t patternNum, const char *description)
{
	undoInit();
	freeEntry(&pending);
	pending.type = UNDO_PATTERN_INSERT;
	strncpy(pending.description, description, sizeof (pending.description)-1);
	captureOrder(&pending.state.patternInsert.beforeOrder);
	if (!capturePattern(patternNum, &pending.state.patternInsert.beforePattern))
	{
		freeEntry(&pending);
		return false;
	}
	return true;
}

void undoPatternInsertCommit(void)
{
	if (pending.type != UNDO_PATTERN_INSERT)
		return;

	captureOrder(&pending.state.patternInsert.afterOrder);
	const uint16_t patternNum = pending.state.patternInsert.beforePattern.patternNum;
	if (!capturePattern(patternNum, &pending.state.patternInsert.afterPattern) ||
		(ordersEqual(&pending.state.patternInsert.beforeOrder, &pending.state.patternInsert.afterOrder) &&
		 patternsEqual(&pending.state.patternInsert.beforePattern, &pending.state.patternInsert.afterPattern)))
	{
		freeEntry(&pending);
		return;
	}

	pending.bytes =
		(uint32_t)(sizeof (orderSnapshot_t) * 2) +
		patternSnapshotBytes(&pending.state.patternInsert.beforePattern) +
		patternSnapshotBytes(&pending.state.patternInsert.afterPattern);
	commitPending();
}


bool undoSongBegin(const char *description)
{
	undoInit();
	freeEntry(&pending);
	pending.type = UNDO_SONG;
	strncpy(pending.description, description, sizeof (pending.description)-1);
	if (!captureSong(&pending.state.song.before))
	{
		freeEntry(&pending);
		return false;
	}
	return true;
}

void undoSongCommit(void)
{
	if (pending.type != UNDO_SONG) return;
	if (!captureSong(&pending.state.song.after) || songsEqual(&pending.state.song.before, &pending.state.song.after))
	{
		freeEntry(&pending);
		return;
	}
	pending.bytes = songSnapshotBytes(&pending.state.song.before) + songSnapshotBytes(&pending.state.song.after);
	commitPending();
}

bool undoSampleBegin(uint8_t instrNum, uint8_t sampleNum, const char *description)
{
	undoInit();
	freeEntry(&pending);
	pending.type = UNDO_SAMPLE;
	pending.state.sample.instrNum = instrNum;
	pending.state.sample.sampleNum = sampleNum;
	strncpy(pending.description, description, sizeof (pending.description)-1);
	if (!captureSample(instrNum, sampleNum, &pending.state.sample.before))
	{
		freeEntry(&pending);
		return false;
	}
	return true;
}

void undoSampleCommit(void)
{
	if (pending.type != UNDO_SAMPLE) return;
	if (!captureSample(pending.state.sample.instrNum, pending.state.sample.sampleNum, &pending.state.sample.after) ||
		samplesEqual(&pending.state.sample.before, &pending.state.sample.after))
	{
		freeEntry(&pending);
		return;
	}
	pending.bytes = sampleSnapshotBytes(&pending.state.sample.before) + sampleSnapshotBytes(&pending.state.sample.after);
	commitPending();
}

bool undoInstrumentBegin(uint8_t instrNum, const char *description)
{
	undoInit();
	freeEntry(&pending);
	pending.type = UNDO_INSTRUMENT;
	pending.state.instrument.instrNum = instrNum;
	strncpy(pending.description, description, sizeof (pending.description)-1);
	if (!captureInstrument(instrNum, &pending.state.instrument.before))
	{
		freeEntry(&pending);
		return false;
	}
	return true;
}

void undoInstrumentCommit(void)
{
	if (pending.type != UNDO_INSTRUMENT) return;
	if (!captureInstrument(pending.state.instrument.instrNum, &pending.state.instrument.after) ||
		instrumentsEqual(&pending.state.instrument.before, &pending.state.instrument.after))
	{
		freeEntry(&pending);
		return;
	}
	pending.bytes = instrumentSnapshotBytes(&pending.state.instrument.before) + instrumentSnapshotBytes(&pending.state.instrument.after);
	commitPending();
}

void undoCancelTransaction(void) { freeEntry(&pending); }

static bool restoreSample(uint8_t instrNum, uint8_t sampleNum, const sampleSnapshot_t *src)
{
	if (instrNum == 0) return false;
	if (instr[instrNum] == NULL && !allocateInstr(instrNum)) return false;
	sample_t *dst = &instr[instrNum]->smp[sampleNum];
	freeSmpData(dst);
	memset(dst, 0, sizeof (*dst));
	if (!src->exists) return true;

	*dst = src->meta;
	dst->dataPtr = dst->origDataPtr = NULL;
	if (src->dataBytes > 0)
	{
		if (!allocateSmpData(dst, dst->length, !!(dst->flags & SAMPLE_16BIT))) return false;
		memcpy(dst->dataPtr, src->data, src->dataBytes);
		fixSample(dst);
	}
	return true;
}

static bool restoreInstrument(uint8_t instrNum, const instrumentSnapshot_t *src)
{
	freeInstr(instrNum);
	memset(song.instrName[instrNum], 0, sizeof (song.instrName[instrNum]));
	if (!src->exists) return true;
	if (!allocateInstr(instrNum)) return false;
	memcpy(song.instrName[instrNum], src->name, sizeof (src->name));
	*instr[instrNum] = src->meta;
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++)
	{
		memset(&instr[instrNum]->smp[i], 0, sizeof (sample_t));
		if (!restoreSample(instrNum, (uint8_t)i, &src->samples[i])) return false;
	}
	return true;
}

static bool restorePattern(const patternSnapshot_t *src)
{
	setPatternLen(src->patternNum, src->numRows);
	if (!src->exists)
	{
		if (pattern[src->patternNum] != NULL)
		{
			memset(pattern[src->patternNum], 0, (uint32_t)src->numRows * TRACK_WIDTH);
			killPatternIfUnused(src->patternNum);
		}
		return true;
	}
	if (!allocatePattern(src->patternNum)) return false;
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

static void restoreOrder(const orderSnapshot_t *src)
{
	restoreOrderData(src);
	restoreOrderPosition(src);
}

static bool applyEntry(const undoEntry_t *e, bool after)
{
	bool ok = false;
	/* Direct Deck placement intentionally leaves the source sample selected.
	** Undo/Redo therefore cannot rely on pauseAudio()'s current-instrument
	** check to protect a different mapped destination. Pull Deck voices by the
	** transaction target before restoring its native sample memory. */
	if (e->type == UNDO_SAMPLE &&
		sampleLauncherInstrumentIsMapped(e->state.sample.instrNum))
	{
		sampleLauncherReset();
	}
	else if (e->type == UNDO_INSTRUMENT &&
		sampleLauncherInstrumentIsMapped(e->state.instrument.instrNum))
	{
		sampleLauncherReset();
	}
	pauseAudio();
	if (e->type == UNDO_PATTERN)
		ok = restorePattern(after ? &e->state.pattern.after : &e->state.pattern.before);
	else if (e->type == UNDO_PATTERN_INSERT)
	{
		const orderSnapshot_t *order = after
			? &e->state.patternInsert.afterOrder
			: &e->state.patternInsert.beforeOrder;
		const patternSnapshot_t *patternState = after
			? &e->state.patternInsert.afterPattern
			: &e->state.patternInsert.beforePattern;

		/*
		** Redo restores the pattern before referencing it from the order list.
		** Undo removes the order reference first so an originally unused
		** pattern can be released by restorePattern().
		*/
		if (after)
		{
			ok = restorePattern(patternState);
			if (ok)
				restoreOrder(order);
		}
		else
		{
			restoreOrderData(order);
			ok = restorePattern(patternState);
			restoreOrderPosition(order);
		}
	}
	else if (e->type == UNDO_SONG)
	{
		ok = true;
		const songSnapshot_t *ss = after ? &e->state.song.after : &e->state.song.before;
		for (int32_t i = 0; i < MAX_PATTERNS; i++)
			if (!restorePattern(&ss->patterns[i])) { ok = false; break; }
	}
	else if (e->type == UNDO_SAMPLE)
		ok = restoreSample(e->state.sample.instrNum, e->state.sample.sampleNum, after ? &e->state.sample.after : &e->state.sample.before);
	else if (e->type == UNDO_INSTRUMENT)
		ok = restoreInstrument(e->state.instrument.instrNum, after ? &e->state.instrument.after : &e->state.instrument.before);
	resumeAudio();

	if (ok)
	{
		setSongModifiedFlag();
		ui.updatePatternEditor = true;
		if (e->type == UNDO_PATTERN_INSERT)
		{
			ui.updatePosSections = true;
			ui.updatePosEdScrollBar = true;
		}
		editor.updateCurInstr = true;
		editor.updateCurSmp = true;
		if (ui.sampleEditorShown) updateSampleEditorSample();
	}
	return ok;
}

void undoPerform(void)
{
	undoInit();
	if (pending.type != UNDO_NONE) undoCancelTransaction();
	if (historyPos <= 0) return;
	if (applyEntry(&history[historyPos-1], false)) historyPos--;
}

void redoPerform(void)
{
	undoInit();
	if (pending.type != UNDO_NONE) undoCancelTransaction();
	if (historyPos >= historyCount) return;
	if (applyEntry(&history[historyPos], true)) historyPos++;
}

void undoLoadConfig(void)
{
	const uint32_t mb = CLAMP(tapeheadConfig.undoMemoryMB, UNDO_MIN_MB, UNDO_MAX_MB);
	memoryLimitBytes = mb * 1024U * 1024U;
	while (historyBytes > memoryLimitBytes) removeOldest();
}

void undoInit(void)
{
	if (initialized) return;
	initialized = true;
	undoLoadConfig();
}

void undoClose(void)
{
	undoClear();
	initialized = false;
}
