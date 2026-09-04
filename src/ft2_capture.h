#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ft2_replayer.h"
#include "ft2_tapesister_render.h"

typedef enum tapeheadPerformanceCaptureToggleResult_t
{
	TAPEHEAD_PERFORMANCE_CAPTURE_FAILED = 0,
	TAPEHEAD_PERFORMANCE_CAPTURE_ARMED,
	TAPEHEAD_PERFORMANCE_CAPTURE_DISARMED,
	TAPEHEAD_PERFORMANCE_CAPTURE_STOPPING,
	TAPEHEAD_PERFORMANCE_CAPTURE_ALREADY_STOPPING
} tapeheadPerformanceCaptureToggleResult_t;

bool tapeheadCaptureRender(const tapeheadRenderPlan_t *plan,
	const tapeheadBlockLoopSpec_t *blockSpec, bool resumeBlockLoop,
	bool quietSuccess);
bool tapeheadCaptureQuickBlock(void);
tapeheadPerformanceCaptureToggleResult_t tapeheadPerformanceCaptureToggle(void);
bool tapeheadPerformanceCaptureIsBusy(void);
void tapeheadPerformanceCaptureFeed(const float *left, const float *right,
	uint32_t offset, uint32_t frames, float normalizeMultiplier,
	bool blockSeam);
void tapeheadPerformanceCaptureAudioStopped(void);
void tapeheadCapturePoll(void);
void tapeheadCaptureShutdown(void);
