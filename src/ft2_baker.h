#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ft2_unicode.h"
#include "ft2_replayer.h"

typedef enum bakerOutputTarget_t
{
	BAKER_OUTPUT_STANDARD_XM = 0,
	BAKER_OUTPUT_TAPEHEAD_XM,
	BAKER_OUTPUT_ADAPTIVE_XM
} bakerOutputTarget_t;

bool bakerIsRunning(void);
bool bakerIsOfflineRunning(void);
bool bakerLiveIsArmed(void);
bool bakerLiveIsCapturing(void);
bool bakerAllowPlaybackStart(int8_t mode);
void bakerPlaybackStarted(int8_t mode);
void bakerFinishOrCancelLive(void);
void bakerBeginTick(void);
void bakerCaptureEvent(int32_t channelIndex, const note_t *event);
void bakerCaptureResolvedEvent(int32_t channelIndex, const note_t *event,
	uint8_t resolvedInstrument, uint8_t resolvedSample);
void bakerCaptureSampleLauncherAction(uint16_t tile, uint8_t voice, bool start);
void bakerBeginManualRow(void);
void bakerCaptureManualEvent(int32_t channelIndex, const note_t *event);
void bakeComposition(UNICHAR *filenameU, bool mergeExactDuplicates,
	bakerOutputTarget_t outputTarget, uint16_t patternRows);
void armLiveCompositionBake(UNICHAR *filenameU, bool mergeExactDuplicates,
	bakerOutputTarget_t outputTarget, uint16_t patternRows);
