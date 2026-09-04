#pragma once

#include <stdbool.h>
#include "ft2_replayer.h"
#include "ft2_tapesister_render.h"

bool tapeheadCaptureRender(const tapeheadRenderPlan_t *plan,
	const tapeheadBlockLoopSpec_t *blockSpec, bool resumeBlockLoop,
	bool quietSuccess);
bool tapeheadCaptureQuickBlock(void);
void tapeheadCapturePoll(void);
