#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

enum
{
	INTERPOLATE_NOTES = 0,
	INTERPOLATE_VOLUME,
	INTERPOLATE_EFFECT,
	INTERPOLATE_TUNING
};

bool interpolationBegin(uint8_t type);
bool interpolationHandlePreviewKey(SDL_Scancode scancode, SDL_Keycode keycode, bool keyWasRepeated);
bool interpolationPreviewActive(void);
uint8_t interpolationGetDisplayedStep(void);
