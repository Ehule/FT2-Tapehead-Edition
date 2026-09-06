#ifndef TAPE_COMPANION_H
#define TAPE_COMPANION_H

#include <SDL2/SDL.h>
#include <stddef.h>
#include <stdint.h>

#define TAPE_COMPANION_TAPEHEAD_NAME "tapehead_companion_v1"
#define TAPE_COMPANION_TAPESISTER_NAME "tapesister_companion_v1"

typedef struct {
    void *mapping;
    void *shared;
    int descriptor;
    char name[128];
    uint32_t last_request_sequence;
    SDL_Window *fallback_window;
    SDL_Window *active_window;
} TapeCompanion;

void tapeCompanionInit(TapeCompanion *companion);
int tapeCompanionOpen(TapeCompanion *companion, const char *name,
                      SDL_Window *window, char *error, size_t error_size);
void tapeCompanionSetActiveWindow(TapeCompanion *companion,
                                  SDL_Window *window);
int tapeCompanionRequestFocus(const char *name);
int tapeCompanionPump(TapeCompanion *companion);
void tapeCompanionClose(TapeCompanion *companion);

#endif
