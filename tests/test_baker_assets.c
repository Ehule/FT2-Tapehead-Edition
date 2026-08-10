#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_replayer.h"
#include "ft2_baker_assets.h"

#define TEST_SMP_DATA_OFFSET ((MAX_LEFT_TAPS * 2) + 1)
#define TEST_SAMPLE_PAD_LENGTH (TEST_SMP_DATA_OFFSET + (MAX_RIGHT_TAPS * 2))

instr_t *instr[128+4];

static instr_t *makeInstrument(uint8_t mappedSample, int8_t pcm0)
{
	instr_t *ins = (instr_t *)calloc(1, sizeof (*ins));
	assert(ins != NULL);
	memset(ins->note2SampleLUT, mappedSample, sizeof (ins->note2SampleLUT));
	ins->volEnvLength = 2;
	ins->volEnvFlags = 7;
	ins->volEnvSustain = 1;
	ins->volEnvLoopEnd = 1;
	ins->volEnvPoints[0][1] = 64;
	ins->volEnvPoints[1][0] = 12;
	ins->volEnvPoints[1][1] = 32;
	ins->panEnvLength = 1;
	ins->panEnvFlags = 1;
	ins->fadeout = 733;
	ins->autoVibType = 2;
	ins->autoVibSweep = 3;
	ins->autoVibDepth = 4;
	ins->autoVibRate = 5;
	for (int32_t i = 0; i < 4; i++)
	{
		sample_t *s = &ins->smp[i];
		s->length = 4;
		s->loopStart = 1;
		s->loopLength = 2;
		s->flags = i == 1 ? (SAMPLE_16BIT | LOOP_PINGPONG) : LOOP_FORWARD;
		s->volume = 40 + i;
		s->panning = 90 + i;
		s->relativeNote = -3 + i;
		s->finetune = 17 + i;
		const size_t bytes = SAMPLE_LENGTH_BYTES(s);
		s->origDataPtr = (int8_t *)calloc(1, bytes + TEST_SAMPLE_PAD_LENGTH);
		assert(s->origDataPtr != NULL);
		s->dataPtr = s->origDataPtr + TEST_SMP_DATA_OFFSET;
		memset(s->dataPtr, pcm0 + i, bytes);
	}
	return ins;
}

static void destroyInstrument(instr_t *ins)
{
	if (ins == NULL) return;
	for (int32_t i = 0; i < MAX_SMP_PER_INST; i++) free(ins->smp[i].origDataPtr);
	free(ins);
}

int main(void)
{
	instr[1] = makeInstrument(0, 10);
	int8_t sourcePcm[8];
	memcpy(sourcePcm, instr[1]->smp[1].dataPtr, sizeof (sourcePcm));

	/* Reproduce the four-slot D-4 performance through the same event resolver
	** used by Baker capture: 0, 1, 2, 3, then 1 again. */
	const uint8_t playedNote = NOTE_C4 + 3; /* D-4 in the stored 1..96 format. */
	for (uint8_t sample = 0; sample < 4; sample++)
		instr[1]->note2SampleLUT[playedNote - 1 + sample] = sample;
	const instr_t originalInstrument = *instr[1];
	const uint8_t selections[] = { 0, 1, 2, 3, 1 };
	uint8_t capturedInstruments[5];
	for (size_t i = 0; i < sizeof (selections); i++)
	{
		note_t event = { playedNote, 1, 0, 0, 0 };
		assert(bakerAssetsResolveEvent(&event, 1, selections[i]));
		assert(event.note == playedNote);
		capturedInstruments[i] = event.instr;
	}
	assert(capturedInstruments[0] == 1);
	assert(capturedInstruments[1] != 1);
	assert(capturedInstruments[2] != 1);
	assert(capturedInstruments[3] != 1);
	assert(capturedInstruments[1] == capturedInstruments[4]);
	assert(bakerAssetsGetPrivateCount() == 3);
	assert(memcmp(&originalInstrument, instr[1], sizeof (originalInstrument)) == 0);
	bakerAssetsFree();

	/* A populated sample not present anywhere in the source note map also gets
	** a private representation without changing D-4. */
	memset(instr[1]->note2SampleLUT, 0, sizeof (instr[1]->note2SampleLUT));
	note_t unmapped = { playedNote, 1, 0, 0, 0 };
	assert(bakerAssetsResolveEvent(&unmapped, 1, 3));
	assert(unmapped.note == playedNote && unmapped.instr != 1);
	bakerAssetsFree();

	/* Exact actual-note mapping uses the original instrument. */
	assert(bakerAssetsResolveInstrument(48, 1, 0) == 1);
	assert(bakerAssetsGetPrivateCount() == 0);

	/* Mapping the sample only at another note must not transpose the event. */
	instr[1]->note2SampleLUT[59] = 1;
	const instr_t sourceBefore = *instr[1];
	const uint8_t privateInstrument = bakerAssetsResolveInstrument(48, 1, 1);
	assert(privateInstrument > 1);
	assert(bakerAssetsGetPrivateCount() == 1);
	assert(bakerAssetsResolveInstrument(48, 1, 1) == privateInstrument);
	assert(bakerAssetsGetPrivateCount() == 1);
	/* Audible changes are not merged; restoring the compatible configuration
	** finds the earlier representation again. */
	instr[1]->smp[1].volume++;
	const uint8_t changedInstrument = bakerAssetsResolveInstrument(48, 1, 1);
	assert(changedInstrument != privateInstrument);
	assert(bakerAssetsGetPrivateCount() == 2);
	instr[1]->smp[1].volume--;
	assert(bakerAssetsResolveInstrument(48, 1, 1) == privateInstrument);
	assert(bakerAssetsGetPrivateCount() == 2);
	assert(bakerAssetsInstall());

	const instr_t *copy = instr[privateInstrument];
	assert(copy != NULL);
	assert(copy->note2SampleLUT[47] == 0);
	assert(copy->fadeout == instr[1]->fadeout);
	assert(copy->volEnvFlags == instr[1]->volEnvFlags);
	assert(copy->volEnvPoints[1][1] == instr[1]->volEnvPoints[1][1]);
	assert(copy->autoVibDepth == instr[1]->autoVibDepth);
	assert(copy->smp[0].length == instr[1]->smp[1].length);
	assert(copy->smp[0].loopStart == instr[1]->smp[1].loopStart);
	assert(copy->smp[0].loopLength == instr[1]->smp[1].loopLength);
	assert(copy->smp[0].flags == instr[1]->smp[1].flags);
	assert(copy->smp[0].volume == instr[1]->smp[1].volume);
	assert(copy->smp[0].panning == instr[1]->smp[1].panning);
	assert(copy->smp[0].relativeNote == instr[1]->smp[1].relativeNote);
	assert(copy->smp[0].finetune == instr[1]->smp[1].finetune);
	assert(copy->smp[0].dataPtr != instr[1]->smp[1].dataPtr);
	assert(memcmp(copy->smp[0].dataPtr, instr[1]->smp[1].dataPtr,
		SAMPLE_LENGTH_BYTES((&copy->smp[0]))) == 0);

	bakerAssetsUninstall();
	assert(instr[privateInstrument] == NULL);
	assert(memcmp(&sourceBefore, instr[1], offsetof(instr_t, smp)) == 0);
	assert(memcmp(sourcePcm, instr[1]->smp[1].dataPtr, sizeof (sourcePcm)) == 0);
	bakerAssetsFree();

	/* All destination slots occupied is a clear hard failure, not fallback. */
	for (int32_t i = 2; i <= MAX_INST; i++) instr[i] = (instr_t *)(uintptr_t)1;
	assert(bakerAssetsResolveInstrument(48, 1, 1) == 0);
	assert(bakerAssetsGetError() == BAKER_ASSET_INSTRUMENT_LIMIT);
	for (int32_t i = 2; i <= MAX_INST; i++) instr[i] = NULL;
	bakerAssetsFree();
	destroyInstrument(instr[1]);
	instr[1] = NULL;
	puts("Baker Sample Morph private-asset tests passed.");
	return 0;
}
