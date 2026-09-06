#include "ft2_live_link.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tape_link.h"
#include "ft2_replayer.h"

static TapeLinkWriter liveLinkWriter = { NULL, NULL, -1, {0}, 0 };
static SDL_Thread *liveLinkThread;
static SDL_mutex *liveLinkMutex;
static SDL_atomic_t liveLinkStop;
static SDL_atomic_t liveLinkPaused;
static tapeheadLiveLinkRenderCallback liveLinkRenderCallback;
static void *liveLinkRenderUserdata;
static float *liveLinkBuffer;
static uint32_t liveLinkSampleRate;
static uint32_t liveLinkBufferFrames;
static char liveLinkLastError[256];

static int liveLinkThreadMain(void *unused)
{
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    const Uint64 period = liveLinkSampleRate > 0u ?
        ((Uint64)liveLinkBufferFrames * frequency) / liveLinkSampleRate : 1u;
    Uint64 deadline = SDL_GetPerformanceCounter();
    (void)unused;

    while (!SDL_AtomicGet(&liveLinkStop)) {
        if (SDL_AtomicGet(&liveLinkPaused)) {
            memset(liveLinkBuffer, 0,
                   liveLinkBufferFrames * 2u * sizeof(float));
        } else {
            SDL_LockMutex(liveLinkMutex);
            if (SDL_AtomicGet(&liveLinkPaused)) {
                memset(liveLinkBuffer, 0,
                       liveLinkBufferFrames * 2u * sizeof(float));
            } else {
                liveLinkRenderCallback(liveLinkBuffer, liveLinkBufferFrames,
                                       liveLinkRenderUserdata);
            }
            SDL_UnlockMutex(liveLinkMutex);
        }
        (void)tapeLinkWriterWrite(&liveLinkWriter, liveLinkBuffer,
                                  liveLinkBufferFrames);

        deadline += period > 0u ? period : 1u;
        for (;;) {
            Uint64 now = SDL_GetPerformanceCounter();
            if (now >= deadline || SDL_AtomicGet(&liveLinkStop)) break;
            Uint64 remaining = deadline - now;
            uint32_t milliseconds = frequency > 0u ?
                (uint32_t)((remaining * 1000u) / frequency) : 0u;
            if (milliseconds > 1u)
                SDL_Delay(milliseconds - 1u);
            else
                SDL_Delay(0u);
        }
        if (SDL_GetPerformanceCounter() > deadline + period * 4u)
            deadline = SDL_GetPerformanceCounter();
    }
    return 0;
}

bool tapeheadLiveLinkDeviceSelected(const char *deviceName)
{
    return deviceName != NULL &&
        strcmp(deviceName, TAPEHEAD_LIVE_LINK_DEVICE_NAME) == 0;
}

const char *tapeheadLiveLinkGetLastError(void)
{
    return liveLinkLastError[0] != '\0' ? liveLinkLastError :
        "Unknown Live Link error";
}

bool tapeheadLiveLinkOpen(uint32_t sampleRate, uint32_t bufferFrames,
    tapeheadLiveLinkRenderCallback renderCallback, void *userdata)
{
    char transportError[192];
    tapeheadLiveLinkClose();
    liveLinkLastError[0] = '\0';
    if (sampleRate < 8000u || bufferFrames == 0u || renderCallback == NULL) {
        snprintf(liveLinkLastError, sizeof(liveLinkLastError),
                 "Invalid Live Link audio configuration");
        return false;
    }
    tapeLinkWriterInit(&liveLinkWriter);
    if (!tapeLinkWriterOpen(&liveLinkWriter, sampleRate, transportError,
                            sizeof(transportError))) {
        snprintf(liveLinkLastError, sizeof(liveLinkLastError), "%s",
                 transportError);
        return false;
    }
    liveLinkMutex = SDL_CreateMutex();
    liveLinkBuffer = (float *)calloc((size_t)bufferFrames * 2u, sizeof(float));
    if (liveLinkMutex == NULL || liveLinkBuffer == NULL) {
        snprintf(liveLinkLastError, sizeof(liveLinkLastError),
                 "Not enough memory for the Live Link audio clock");
        tapeheadLiveLinkClose();
        return false;
    }
    liveLinkSampleRate = sampleRate;
    liveLinkBufferFrames = bufferFrames;
    liveLinkRenderCallback = renderCallback;
    liveLinkRenderUserdata = userdata;
    SDL_AtomicSet(&liveLinkStop, 0);
    SDL_AtomicSet(&liveLinkPaused, 1);
    liveLinkThread = SDL_CreateThread(liveLinkThreadMain,
                                      "Tapehead Live Link", NULL);
    if (liveLinkThread == NULL) {
        snprintf(liveLinkLastError, sizeof(liveLinkLastError), "%s",
                 SDL_GetError());
        tapeheadLiveLinkClose();
        return false;
    }
    return true;
}

void tapeheadLiveLinkClose(void)
{
    SDL_AtomicSet(&liveLinkStop, 1);
    if (liveLinkThread != NULL) {
        SDL_WaitThread(liveLinkThread, NULL);
        liveLinkThread = NULL;
    }
    tapeLinkWriterClose(&liveLinkWriter);
    if (liveLinkMutex != NULL) {
        SDL_DestroyMutex(liveLinkMutex);
        liveLinkMutex = NULL;
    }
    free(liveLinkBuffer);
    liveLinkBuffer = NULL;
    liveLinkRenderCallback = NULL;
    liveLinkRenderUserdata = NULL;
    liveLinkSampleRate = 0u;
    liveLinkBufferFrames = 0u;
}

void tapeheadLiveLinkPause(bool pause)
{
    if (!tapeheadLiveLinkIsOpen()) return;
    SDL_AtomicSet(&liveLinkPaused, pause ? 1 : 0);
    if (pause && liveLinkMutex != NULL) {
        /* Match SDL_PauseAudioDevice's callback-quiescence guarantee before
           editor code mutates voice/sample state. */
        SDL_LockMutex(liveLinkMutex);
        SDL_UnlockMutex(liveLinkMutex);
    }
}

void tapeheadLiveLinkLock(void)
{
    if (liveLinkMutex != NULL) SDL_LockMutex(liveLinkMutex);
}

void tapeheadLiveLinkUnlock(void)
{
    if (liveLinkMutex != NULL) SDL_UnlockMutex(liveLinkMutex);
}

bool tapeheadLiveLinkIsOpen(void)
{
    return liveLinkWriter.shared != NULL;
}

void tapeheadLiveLinkPumpTransport(void)
{
    TapeLinkCommand command = tapeLinkWriterTakeCommand(&liveLinkWriter);
    if (command == TAPE_LINK_COMMAND_TOGGLE_SONG) {
        if (songPlaying &&
            (playMode == PLAYMODE_SONG || playMode == PLAYMODE_RECSONG))
            stopPlaying();
        else
            pbPlaySong();
    } else if (command == TAPE_LINK_COMMAND_TOGGLE_PATTERN) {
        if (songPlaying &&
            (playMode == PLAYMODE_PATT || playMode == PLAYMODE_RECPATT))
            stopPlaying();
        else
            pbPlayPtn();
    }
}
