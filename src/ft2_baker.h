#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ft2_unicode.h"
#include "ft2_replayer.h"

bool bakerIsRunning(void);
bool bakerIsOfflineRunning(void);
bool bakerLiveIsArmed(void);
bool bakerLiveIsCapturing(void);
bool bakerAllowPlaybackStart(int8_t mode);
void bakerPlaybackStarted(int8_t mode);
void bakerFinishOrCancelLive(void);
void bakerBeginTick(void);
void bakerCaptureEvent(int32_t channelIndex, const note_t *event);
void bakeComposition(UNICHAR *filenameU, bool mergeExactDuplicates);
void armLiveCompositionBake(UNICHAR *filenameU, bool mergeExactDuplicates);
