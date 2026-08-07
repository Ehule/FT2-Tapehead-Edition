#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_audio.h"
#include "ft2_sample_launcher.h"
#include "ft2_structs.h"

bool songPlaying;
song_t song;
editor_t editor;
audio_t audio;
instr_t *instr[MAX_INST + 4];

static const sample_t *lastTriggeredSample;
static uint8_t lastTriggeredVoice, lastTriggeredBus;

bool allocateInstr(int16_t instrument)
{
	if (instr[instrument] != NULL)
		return false;
	instr[instrument] = calloc(1, sizeof (*instr[instrument]));
	return instr[instrument] != NULL;
}

void fixInstrAndSampleNames(int16_t instrument) { (void)instrument; }
void sanitizeSample(sample_t *sample) { (void)sample; }
void fixSample(sample_t *sample) { (void)sample; }
void setSongModifiedFlag(void) { song.isModified = true; }
void lockAudio(void) { audio.locked = true; }
void unlockAudio(void) { audio.locked = false; }
bool undoSampleBegin(uint8_t instrument, uint8_t sample,
	const char *description)
{
	(void)instrument;
	(void)sample;
	(void)description;
	return true;
}
void undoSampleCommit(void) { }
bool undoTransactionIsActive(void) { return false; }
bool undoTransactionBegin(const char *description) { (void)description; return true; }
bool undoTransactionAddSample(uint8_t instrument, uint8_t sample)
{
	(void)instrument;
	(void)sample;
	return true;
}
bool undoTransactionAddInstrument(uint8_t instrument) { (void)instrument; return true; }
bool undoTransactionAddSampleLauncher(void) { return true; }
void undoTransactionCommit(void) { }
void undoCancelTransaction(void) { }

void freeSmpData(sample_t *sample)
{
	free(sample->origDataPtr);
	sample->origDataPtr = sample->dataPtr = NULL;
}

void freeSample(int16_t instrument, int16_t sample)
{
	if (instr[instrument] == NULL)
		return;
	freeSmpData(&instr[instrument]->smp[sample]);
	memset(&instr[instrument]->smp[sample], 0,
		sizeof (instr[instrument]->smp[sample]));
}

bool cloneSample(sample_t *source, sample_t *destination)
{
	*destination = *source;
	destination->origDataPtr = destination->dataPtr = NULL;
	const size_t bytes = (size_t)source->length <<
		!!(source->flags & SAMPLE_16BIT);
	destination->origDataPtr = malloc(bytes);
	if (destination->origDataPtr == NULL)
		return false;
	destination->dataPtr = destination->origDataPtr;
	memcpy(destination->dataPtr, source->dataPtr, bytes);
	return true;
}

void audioSampleLauncherStop(uint8_t voiceIndex) { (void)voiceIndex; }
void audioSampleLauncherStopAll(void) { lastTriggeredSample = NULL; }
void audioSampleLauncherSetOutputBus(uint8_t voiceIndex, uint8_t outputBus)
{
	lastTriggeredVoice = voiceIndex;
	lastTriggeredBus = outputBus;
}
void audioSampleLauncherTrigger(uint8_t voiceIndex, const sample_t *sample,
	uint8_t outputBus)
{
	lastTriggeredVoice = voiceIndex;
	lastTriggeredSample = sample;
	lastTriggeredBus = outputBus;
}

void freeInstr(int32_t instrument)
{
	if (instr[instrument] != NULL)
	{
		for (uint8_t sample = 0; sample < MAX_SMP_PER_INST; sample++)
			free(instr[instrument]->smp[sample].origDataPtr);
	}
	free(instr[instrument]);
	instr[instrument] = NULL;
}

static instr_t *makeInstrument(const char *sampleName)
{
	instr_t *instrument = calloc(1, sizeof (*instrument));
	assert(instrument != NULL);
	instrument->smp[0].origDataPtr = malloc(8);
	assert(instrument->smp[0].origDataPtr != NULL);
	instrument->smp[0].dataPtr = instrument->smp[0].origDataPtr;
	instrument->smp[0].length = 8;
	instrument->smp[0].volume = 64;
	instrument->smp[0].panning = 128;
	strncpy(instrument->smp[0].name, sampleName, 22);
	return instrument;
}

static void resetFixture(void)
{
	for (uint16_t i = 1; i <= MAX_INST; i++)
		freeInstr(i);
	memset(&song, 0, sizeof (song));
	memset(&editor, 0, sizeof (editor));
	songPlaying = false;
	sampleLauncherForgetBanks();
}

static void test_tagged_instruments_restore_native_bank(void)
{
	resetFixture();
	instr[10] = makeInstrument("KICK");
	instr[11] = calloc(1, sizeof (*instr[11]));
	assert(instr[11] != NULL);
	strcpy(song.instrName[10], "SB00A 00-0F DRUMS");
	strcpy(song.instrName[11], "SB00B 10-1F DRUMS");

	assert(sampleLauncherTileIsLoaded(0));
	assert(strcmp(sampleLauncherGetTileName(0), "KICK") == 0);
	assert(!sampleLauncherTileIsLoaded(16));
	instr[11]->smp[0].dataPtr = (int8_t *)instr[11];
	instr[11]->smp[0].length = 8;
	strcpy(instr[11]->smp[0].name, "SNARE");
	assert(sampleLauncherTileIsLoaded(16));

	assert(sampleLauncherSelectTileInEditor(16));
	assert(editor.curInstr == 11);
	assert(editor.curSmp == 0);
}

static void test_pages_keep_transport_identity(void)
{
	resetFixture();
	instr[12] = makeInstrument("BANK2");
	strcpy(song.instrName[12], "SB20A 20-2F KIT");
	assert(sampleLauncherTileIsLoaded(32));
	assert(sampleLauncherRequestQ(32));
	assert(lastTriggeredSample == &instr[12]->smp[0]);
	assert(lastTriggeredVoice == 0);

	sampleLauncherSetBank(0);
	assert(sampleLauncherGetQCurrent() == 32);
	sampleLauncherSetBank(1);
	assert(sampleLauncherGetQCurrent() == 32);
}

static void test_import_pair_allocation_and_clear(void)
{
	resetFixture();
	uint8_t instrumentA = 0, instrumentB = 0;
	assert(sampleLauncherPrepareBankImport(3, &instrumentA, &instrumentB));
	assert(instrumentA == 1);
	assert(instrumentB == 2);
	instr[1] = makeInstrument("ONE");
	instr[2] = makeInstrument("TWO");
	sampleLauncherAttachBank(3, 1, 2);
	assert(sampleLauncherFindBankForInstrument(1) == 3);
	assert(sampleLauncherFindBankForInstrument(2) == 3);
	assert(sampleLauncherClearBank(3));
	assert(instr[1] == NULL && instr[2] == NULL);
}

static void test_unassign_bank_detaches_tags_and_keeps_native_samples(void)
{
	resetFixture();
	instr[10] = makeInstrument("KICK");
	instr[11] = makeInstrument("SNARE");
	strcpy(song.instrName[10], "SB00A 00-0F DRUMS");
	strcpy(song.instrName[11], "SB00B 10-1F DRUMS");
	assert(sampleLauncherTileIsLoaded(0));
	assert(sampleLauncherTileIsLoaded(16));

	assert(sampleLauncherUnassignBank(0));
	assert(instr[10] != NULL && instr[11] != NULL);
	assert(instr[10]->smp[0].dataPtr != NULL);
	assert(instr[11]->smp[0].dataPtr != NULL);
	assert(strcmp(song.instrName[10], "00-0F DRUMS") == 0);
	assert(strcmp(song.instrName[11], "10-1F DRUMS") == 0);
	assert(!sampleLauncherTileIsLoaded(0));
	assert(!sampleLauncherTileNaturalStorageIsLoaded(0));

	uint8_t instrumentA = 0, instrumentB = 0;
	assert(sampleLauncherPrepareBankImport(0, &instrumentA, &instrumentB));
	assert(instrumentA != 10 && instrumentA != 11);
	assert(instrumentB != 10 && instrumentB != 11);
}

static void test_stopped_song_uses_sample_deck_boundary_clock(void)
{
	resetFixture();
	instr[12] = makeInstrument("ONE");
	for (uint8_t sample = 1; sample <= 5; sample++)
	{
		instr[12]->smp[sample].origDataPtr = malloc(8);
		assert(instr[12]->smp[sample].origDataPtr != NULL);
		instr[12]->smp[sample].dataPtr = instr[12]->smp[sample].origDataPtr;
		instr[12]->smp[sample].length = 8;
		snprintf(instr[12]->smp[sample].name,
			sizeof (instr[12]->smp[sample].name), "SAMPLE %u", sample);
	}
	strcpy(song.instrName[12], "SB00A 00-0F CLOCK");
	song.speed = 2;
	song.currNumRows = 2;

	assert(sampleLauncherRequestQ(0));
	assert(sampleLauncherGetQCurrent() == 0);
	assert(lastTriggeredSample == &instr[12]->smp[0]);
	assert(sampleLauncherRequestQ(1));
	assert(sampleLauncherGetQCurrent() == 0);
	assert(sampleLauncherGetQQueuePos(1) == 0);
	assert(sampleLauncherRequestQ(2));
	assert(sampleLauncherRequestQ(3));
	assert(sampleLauncherRequestQ(4));
	assert(sampleLauncherGetQQueuePos(4) == 3);
	assert(!sampleLauncherRequestQ(5));

	for (uint8_t tick = 0; tick < 3; tick++)
	{
		sampleLauncherTick();
		assert(sampleLauncherGetQCurrent() == 0);
	}
	sampleLauncherTick();
	assert(sampleLauncherGetQCurrent() == 1);
	assert(sampleLauncherGetQQueuePos(1) == -1);
	assert(sampleLauncherGetQQueuePos(2) == 0);
	assert(sampleLauncherGetQQueuePos(4) == 2);
	assert(lastTriggeredSample == &instr[12]->smp[1]);
}

static void test_running_song_keeps_real_pattern_boundary(void)
{
	resetFixture();
	instr[12] = makeInstrument("ONE");
	instr[12]->smp[1].origDataPtr = malloc(8);
	assert(instr[12]->smp[1].origDataPtr != NULL);
	instr[12]->smp[1].dataPtr = instr[12]->smp[1].origDataPtr;
	instr[12]->smp[1].length = 8;
	strcpy(song.instrName[12], "SB00A 00-0F CLOCK");
	songPlaying = true;

	assert(sampleLauncherRequestQ(0));
	assert(sampleLauncherGetQCurrent() == -1);
	assert(lastTriggeredSample == NULL);
	sampleLauncherHandleBoundary();
	assert(sampleLauncherGetQCurrent() == 0);
	assert(lastTriggeredSample == &instr[12]->smp[0]);

	assert(sampleLauncherRequestQ(1));
	for (uint8_t tick = 0; tick < 32; tick++)
		sampleLauncherTick();
	assert(sampleLauncherGetQCurrent() == 0);
	sampleLauncherHandleBoundary();
	assert(sampleLauncherGetQCurrent() == 1);
	assert(lastTriggeredSample == &instr[12]->smp[1]);
}

static void test_range_import_allocates_one_instrument_per_sixteen_samples(void)
{
	resetFixture();
	instr[10] = makeInstrument("OLD-A");
	instr[11] = makeInstrument("OLD-B");
	strcpy(song.instrName[10], "SB00A 00-0F OLD");
	strcpy(song.instrName[11], "SB00B 10-1F OLD");

	uint8_t destinations[SAMPLE_LAUNCHER_BANK_COUNT][2];
	uint8_t bankCount = 0;
	assert(sampleLauncherPrepareRangeImport(0, 47, destinations, &bankCount));
	assert(bankCount == 2);
	assert(destinations[0][0] == 10);
	assert(destinations[0][1] == 11);
	assert(destinations[1][0] == 1);
	assert(destinations[1][1] == 0);

	assert(!sampleLauncherPrepareRangeImport(7, 33, destinations, &bankCount));
}

static void test_range_import_reserves_distinct_empty_instruments(void)
{
	resetFixture();
	uint8_t destinations[SAMPLE_LAUNCHER_BANK_COUNT][2];
	uint8_t bankCount = 0;
	assert(sampleLauncherPrepareRangeImport(2, 80, destinations, &bankCount));
	assert(bankCount == 3);
	assert(destinations[0][0] == 1);
	assert(destinations[0][1] == 2);
	assert(destinations[1][0] == 3);
	assert(destinations[1][1] == 4);
	assert(destinations[2][0] == 5);
	assert(destinations[2][1] == 0);
}

static void test_selected_sample_copies_to_exact_tile_and_creates_bank_half(void)
{
	resetFixture();
	instr[20] = makeInstrument("DEEP KICK");
	sample_t *source = &instr[20]->smp[0];
	source->finetune = -12;
	source->relativeNote = 3;
	source->loopStart = 2;
	source->loopLength = 4;
	source->flags = LOOP_FORWARD;
	source->volume = 51;
	source->panning = 77;
	memset(source->dataPtr, 0x35, 8);

	assert(sampleLauncherCopySampleToTile(19, 20, 0) ==
		SAMPLE_LAUNCHER_PLACE_OK);
	assert(sampleLauncherTileIsLoaded(19));
	assert(strcmp(sampleLauncherGetTileName(19), "DEEP KICK") == 0);
	assert(strcmp(song.instrName[1], "SB00B 10-1F DEEP KI") == 0);
	assert(instr[1] != NULL);
	const sample_t *destination = &instr[1]->smp[3];
	assert(destination->dataPtr != source->dataPtr);
	assert(destination->finetune == source->finetune);
	assert(destination->relativeNote == source->relativeNote);
	assert(destination->loopStart == source->loopStart);
	assert(destination->loopLength == source->loopLength);
	assert(destination->flags == source->flags);
	assert(destination->volume == source->volume);
	assert(destination->panning == source->panning);
	assert(memcmp(destination->dataPtr, source->dataPtr, 8) == 0);
	assert(song.isModified);
	assert(sampleLauncherCopySampleToTile(19, 1, 3) ==
		SAMPLE_LAUNCHER_PLACE_SAME_SAMPLE);
}

static void test_selected_sample_replaces_only_target_slot(void)
{
	resetFixture();
	instr[10] = makeInstrument("OLD");
	strcpy(song.instrName[10], "SB20A 20-2F KIT");
	instr[11] = makeInstrument("NEW");
	assert(sampleLauncherTileIsLoaded(32));
	sampleLauncherCycleTileBus(32, 4);
	assert(sampleLauncherGetTileBus(32) == 1);

	assert(sampleLauncherCopySampleToTile(32, 11, 0) ==
		SAMPLE_LAUNCHER_PLACE_OK);
	assert(strcmp(sampleLauncherGetTileName(32), "NEW") == 0);
	assert(sampleLauncherGetTileBus(32) == 1);
	assert(instr[10]->smp[1].dataPtr == NULL);
}

static void test_selected_sample_requires_source_and_free_instrument(void)
{
	resetFixture();
	assert(sampleLauncherCopySampleToTile(0, 1, 0) ==
		SAMPLE_LAUNCHER_PLACE_EMPTY_SOURCE);

	for (uint16_t instrument = 1; instrument <= MAX_INST; instrument++)
		instr[instrument] = makeInstrument("FULL");
	assert(sampleLauncherCopySampleToTile(224, 1, 0) ==
		SAMPLE_LAUNCHER_PLACE_NO_INSTRUMENT);
}

static void test_module_reference_assignment_does_not_copy_audio(void)
{
	resetFixture();
	instr[20] = makeInstrument("MODULE KICK");
	assert(sampleLauncherAssignTile(77, 20, 0));
	assert(sampleLauncherTileIsLoaded(77));
	assert(strcmp(sampleLauncherGetTileName(77), "MODULE KICK") == 0);
	uint8_t instrument = 0, sample = 0;
	assert(sampleLauncherGetTileReference(77, &instrument, &sample));
	assert(instrument == 20 && sample == 0);
	assert(lastTriggeredSample == NULL);
	assert(sampleLauncherUnassignTile(77));
	assert(!sampleLauncherTileIsLoaded(77));
	assert(instr[20]->smp[0].dataPtr != NULL);
}

static void test_reference_metadata_round_trip(void)
{
	resetFixture();
	instr[20] = makeInstrument("ROUND TRIP");
	assert(sampleLauncherAssignTile(145, 20, 0));
	assert(sampleLauncherUnassignTile(4));

	FILE *file = tmpfile();
	assert(file != NULL);
	assert(sampleLauncherWriteXMMetadata(file));
	const long fileSize = ftell(file);
	assert(fileSize > 0);
	rewind(file);
	sampleLauncherBeginModuleLoad();
	sampleLauncherReadXMMetadata(file, (uint32_t)fileSize);
	sampleLauncherForgetBanks();
	sampleLauncherCommitXMMetadata();
	assert(sampleLauncherTileIsLoaded(145));
	assert(!sampleLauncherTileIsLoaded(4));
	fclose(file);
}

int main(void)
{
	test_tagged_instruments_restore_native_bank();
	test_pages_keep_transport_identity();
	test_import_pair_allocation_and_clear();
	test_unassign_bank_detaches_tags_and_keeps_native_samples();
	test_stopped_song_uses_sample_deck_boundary_clock();
	test_running_song_keeps_real_pattern_boundary();
	test_range_import_allocates_one_instrument_per_sixteen_samples();
	test_range_import_reserves_distinct_empty_instruments();
	test_selected_sample_copies_to_exact_tile_and_creates_bank_half();
	test_selected_sample_replaces_only_target_slot();
	test_selected_sample_requires_source_and_free_instrument();
	test_module_reference_assignment_does_not_copy_audio();
	test_reference_metadata_round_trip();
	resetFixture();
	puts("13 native Sample Bank integration tests passed.");
	return 0;
}
