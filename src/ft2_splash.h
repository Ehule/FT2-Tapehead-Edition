#pragma once

#include <stdbool.h>
#include <SDL2/SDL_events.h>

bool showTapeheadSplash(void);
bool tapeheadSplashConsumeDismissEvent(const SDL_Event *event);
