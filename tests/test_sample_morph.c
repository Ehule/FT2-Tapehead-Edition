#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ft2_replayer.h"
#include "ft2_sample_morph.h"

channel_t channel[MAX_CHANNELS];
song_t song;
instr_t *instr[128+4];

static instr_t testInstrument;
static int8_t sampleData[MAX_SMP_PER_INST];

static void resetFixture(void)
{
	memset(channel, 0, sizeof (channel));
	memset(&song, 0, sizeof (song));
	memset(instr, 0, sizeof (instr));
	memset(&testInstrument, 0, sizeof (testInstrument));
	song.numChannels = 8;
	instr[1] = &testInstrument;
	for (int32_t i = 0; i < 8; i++) channel[i].instrNum = 1;
	/* Deliberate gaps prove that empty sample slots are skipped. */
	testInstrument.smp[1].dataPtr = &sampleData[1];
	testInstrument.smp[1].length = 1;
	testInstrument.smp[5].dataPtr = &sampleData[5];
	testInstrument.smp[5].length = 1;
	testInstrument.smp[12].dataPtr = &sampleData[12];
	testInstrument.smp[12].length = 1;
	sampleMorphResetForLoadedModule();
}

static void testSafetyArmAndFutureTriggerResolution(void)
{
	resetFixture();
	assert(!sampleMorphIsArmed());
	assert(!sampleMorphSetFromController(0, 127));
	assert(sampleMorphResolve(0, 1, 5) == 5);

	assert(sampleMorphToggleArmed());
	assert(sampleMorphIsArmed());
	assert(sampleMorphSetFromController(0, 127));
	assert(sampleMorphResolve(0, 1, 5) == 12);
	assert(sampleMorphSetFromController(0, 64));
	assert(sampleMorphResolve(0, 1, 1) == 5);

	assert(sampleMorphToggleArmed());
	assert(sampleMorphResolve(0, 1, 1) == 1);
}

static void testCollectiveStepClampsAndSkipsEmptySlots(void)
{
	resetFixture();
	sampleMorphSetArmed(true);
	assert(sampleMorphStepAll(1));
	for (int32_t i = 0; i < 8; i++)
		assert(sampleMorphResolve((uint8_t)i, 1, 0) == 5);
	assert(sampleMorphStepAll(1));
	assert(sampleMorphResolve(7, 1, 0) == 12);
	assert(!sampleMorphStepAll(1));
	assert(sampleMorphStepAll(-1));
	assert(sampleMorphResolve(3, 1, 0) == 5);
}

static void testTrackAndModuleBoundsAreSafe(void)
{
	resetFixture();
	sampleMorphSetArmed(true);
	assert(!sampleMorphSetFromController(8, 127));
	assert(sampleMorphResolve(8, 1, 7) == 7);
	assert(sampleMorphResolve(0, 99, 7) == 7);
	sampleMorphResetForLoadedModule();
	assert(!sampleMorphIsArmed());
	assert(sampleMorphGetSelectedSample(0) == 1);
}

int main(void)
{
	testSafetyArmAndFutureTriggerResolution();
	testCollectiveStepClampsAndSkipsEmptySlots();
	testTrackAndModuleBoundsAreSafe();
	puts("3 Sample Morph native groups passed.");
	return 0;
}
