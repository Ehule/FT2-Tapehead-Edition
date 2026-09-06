#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <process.h>
#define test_pid _getpid
#else
#include <unistd.h>
#define test_pid getpid
#endif

#include "tape_companion.h"

/* The protocol test never creates a window. These stubs keep the transport
   test independent of an installed SDL runtime while preserving SDL's types. */
static SDL_Window *last_raised_window;

Uint32 tapeCompanionTestGetWindowFlags(SDL_Window *window)
{
    (void)window;
    return 0u;
}

void tapeCompanionTestRestoreWindow(SDL_Window *window) { (void)window; }
void tapeCompanionTestShowWindow(SDL_Window *window) { (void)window; }
void tapeCompanionTestRaiseWindow(SDL_Window *window)
{
    last_raised_window = window;
}
int tapeCompanionTestSetWindowInputFocus(SDL_Window *window)
{
    (void)window;
    return 0;
}

int main(void)
{
    TapeCompanion tapehead;
    TapeCompanion tapesister;
    SDL_Window *tapehead_window = (SDL_Window *)(uintptr_t)1u;
    SDL_Window *tapesister_main = (SDL_Window *)(uintptr_t)2u;
    SDL_Window *sister_machine = (SDL_Window *)(uintptr_t)3u;
    char tapehead_name[96];
    char tapesister_name[96];
    char error[128];

    snprintf(tapehead_name, sizeof(tapehead_name),
             "tapehead_companion_test_%ld", (long)test_pid());
    snprintf(tapesister_name, sizeof(tapesister_name),
             "tapesister_companion_test_%ld", (long)test_pid());
    tapeCompanionInit(&tapehead);
    tapeCompanionInit(&tapesister);
    assert(tapeCompanionOpen(&tapehead, tapehead_name, tapehead_window,
                             error, sizeof(error)));
    assert(tapeCompanionOpen(&tapesister, tapesister_name, tapesister_main,
                             error, sizeof(error)));

    tapeCompanionSetActiveWindow(&tapesister, sister_machine);
    assert(tapeCompanionRequestFocus(tapesister_name));
    assert(tapeCompanionPump(&tapesister));
    assert(last_raised_window == sister_machine);
    assert(!tapeCompanionPump(&tapesister));

    assert(tapeCompanionRequestFocus(tapehead_name));
    assert(tapeCompanionRequestFocus(tapehead_name));
    assert(tapeCompanionPump(&tapehead));
    assert(last_raised_window == tapehead_window);
    assert(!tapeCompanionPump(&tapehead));

    tapeCompanionClose(&tapesister);
    assert(!tapeCompanionRequestFocus(tapesister_name));
    tapeCompanionClose(&tapehead);
    assert(!tapeCompanionRequestFocus(tapehead_name));
    puts("Companion focus protocol tests passed");
    return 0;
}
