#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "ft2_header.h"

#define MIN_WAV_RENDER_FREQ 8000
#define MAX_WAV_RENDER_FREQ 384000

typedef void (*wavRenderCompletionCallback)(bool success,
	uint64_t renderedFrames, void *userdata);

void cbToggleWavRenderIndividualTracks(void);
void setWavRenderFrequency(int32_t freq);
void setWavRenderBitDepth(uint8_t bitDepth);
void updateWavRendererSettings(void);
void drawWavRenderer(void);
void showWavRenderer(void);
void hideWavRenderer(void);
void exitWavRenderer(void);
void pbWavRender(void);
void pbWavExit(void);
void pbWavFreqUp(void);
void pbWavFreqDown(void);
void pbWavAmpUp(void);
void pbWavAmpDown(void);
void pbWavSongStartUp(void);
void pbWavSongStartDown(void);
void pbWavSongEndUp(void);
void pbWavSongEndDown(void);
void resetWavRenderer(void);
void rbWavRenderBitDepth16(void);
void rbWavRenderBitDepth32(void);
uint32_t getWavRenderFrequency(void);
uint8_t getWavRenderBitDepth(void);
bool startWavRenderToFile(FILE *file, uint8_t startPosition,
	uint8_t stopPosition, int16_t soloChannel,
	wavRenderCompletionCallback callback, void *userdata);
bool startWavBlockRenderToFile(FILE *file,
	const tapeheadBlockLoopSpec_t *spec,
	wavRenderCompletionCallback callback, void *userdata);
