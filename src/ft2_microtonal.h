#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct pattNote_t note_t;

enum
{
	TAPEHEAD_EFX_MICROTUNE = 0x16, /* Mxx */
	TAPEHEAD_EFX_MICRODRIFT = 0x17 /* Nxx */
};

#define MICROTONAL_CENTS_FRAC_BITS 16
#define MICROTONAL_CENTS_ONE (1 << MICROTONAL_CENTS_FRAC_BITS)

typedef struct microtonalState_t
{
	int16_t tuneCents;
	uint8_t driftDepthCents;
	int32_t driftCents16, driftTargetCents16;
	uint32_t driftTimeLeft16, driftPrngState;
} microtonalState_t;

void microtonalReset(microtonalState_t *state, uint32_t voiceIndex);
void microtonalSetTune(microtonalState_t *state, uint8_t parameter);
void microtonalSetDriftDepth(microtonalState_t *state, uint8_t depthCents);
bool microtonalAdvance(microtonalState_t *state, uint16_t bpm);
bool microtonalEffectIsPitchExtension(uint8_t effect);
bool microtonalLaneTypeIsValid(uint8_t type);
bool microtonalPromoteLegacyEffect(note_t *event);
int32_t microtonalCurrentCents16(const microtonalState_t *state);
int64_t microtonalScaleDelta(int64_t baseDelta, int32_t cents16);
