#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "tape_companion.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef TAPE_COMPANION_TEST
#include <SDL2/SDL_syswm.h>
#endif
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef TAPE_COMPANION_TEST
#define SDL_GetWindowFlags tapeCompanionTestGetWindowFlags
#define SDL_RestoreWindow tapeCompanionTestRestoreWindow
#define SDL_ShowWindow tapeCompanionTestShowWindow
#define SDL_RaiseWindow tapeCompanionTestRaiseWindow
#define SDL_SetWindowInputFocus tapeCompanionTestSetWindowInputFocus
extern Uint32 tapeCompanionTestGetWindowFlags(SDL_Window *window);
extern void tapeCompanionTestRestoreWindow(SDL_Window *window);
extern void tapeCompanionTestShowWindow(SDL_Window *window);
extern void tapeCompanionTestRaiseWindow(SDL_Window *window);
extern int tapeCompanionTestSetWindowInputFocus(SDL_Window *window);
#endif

enum {
    TAPE_COMPANION_MAGIC = 0x54434D50u,
    TAPE_COMPANION_VERSION = 1u,
    TAPE_COMPANION_RUNNING = 1u,
    TAPE_COMPANION_STALE_MS = 5000u
};

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t bytes;
    volatile uint32_t state;
    uint32_t process_id;
    volatile uint32_t heartbeat_ms;
    volatile uint32_t request_sequence;
    volatile uint32_t window_sequence;
    volatile uint32_t window_low;
    volatile uint32_t window_high;
} TapeCompanionShared;

static uint32_t atomic_load_u32(const volatile uint32_t *value)
{
#ifdef _WIN32
    return (uint32_t)InterlockedCompareExchange((volatile LONG *)value, 0, 0);
#else
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
#endif
}

static void atomic_store_u32(volatile uint32_t *value, uint32_t replacement)
{
#ifdef _WIN32
    (void)InterlockedExchange((volatile LONG *)value, (LONG)replacement);
#else
    __atomic_store_n(value, replacement, __ATOMIC_RELEASE);
#endif
}

static uint32_t atomic_add_u32(volatile uint32_t *value, uint32_t amount)
{
#ifdef _WIN32
    return (uint32_t)InterlockedExchangeAdd((volatile LONG *)value,
                                            (LONG)amount) + amount;
#else
    return __atomic_add_fetch(value, amount, __ATOMIC_ACQ_REL);
#endif
}

static uint32_t monotonic_milliseconds(void)
{
#ifdef _WIN32
    return (uint32_t)GetTickCount();
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0u;
    return (uint32_t)((uint64_t)now.tv_sec * 1000u +
                      (uint64_t)now.tv_nsec / 1000000u);
#endif
}

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error == NULL || error_size == 0u) return;
    snprintf(error, error_size, "%s", message != NULL ? message : "");
}

static void normalized_name(char *output, size_t output_size,
                            const char *name)
{
    if (output == NULL || output_size == 0u) return;
#ifdef _WIN32
    snprintf(output, output_size, "Local\\%s", name != NULL ? name : "");
#else
    snprintf(output, output_size, "/%s", name != NULL ? name : "");
#endif
}

static int shared_valid(const TapeCompanionShared *shared)
{
    return shared != NULL && shared->magic == TAPE_COMPANION_MAGIC &&
           shared->version == TAPE_COMPANION_VERSION &&
           shared->bytes == sizeof(*shared) &&
           atomic_load_u32(&shared->state) == TAPE_COMPANION_RUNNING;
}

static uint64_t native_window_token(SDL_Window *window)
{
#if defined(_WIN32) && !defined(TAPE_COMPANION_TEST)
    SDL_SysWMinfo info;
    if (window == NULL) return 0u;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) ||
        info.subsystem != SDL_SYSWM_WINDOWS)
        return 0u;
    return (uint64_t)(uintptr_t)info.info.win.window;
#else
    (void)window;
    return 0u;
#endif
}

static void shared_set_window(TapeCompanionShared *shared, uint64_t token)
{
    if (!shared_valid(shared)) return;
    (void)atomic_add_u32(&shared->window_sequence, 1u);
    atomic_store_u32(&shared->window_low, (uint32_t)token);
    atomic_store_u32(&shared->window_high, (uint32_t)(token >> 32));
    (void)atomic_add_u32(&shared->window_sequence, 1u);
}

static uint64_t shared_get_window(const TapeCompanionShared *shared)
{
    uint32_t before, after, low, high;
    for (int attempt = 0; attempt < 8; ++attempt) {
        before = atomic_load_u32(&shared->window_sequence);
        if ((before & 1u) != 0u) continue;
        low = atomic_load_u32(&shared->window_low);
        high = atomic_load_u32(&shared->window_high);
        after = atomic_load_u32(&shared->window_sequence);
        if (before == after && (after & 1u) == 0u)
            return ((uint64_t)high << 32) | low;
    }
    return 0u;
}

void tapeCompanionInit(TapeCompanion *companion)
{
    if (companion == NULL) return;
    memset(companion, 0, sizeof(*companion));
    companion->descriptor = -1;
}

int tapeCompanionOpen(TapeCompanion *companion, const char *name,
                      SDL_Window *window, char *error, size_t error_size)
{
    TapeCompanionShared *shared;
    const size_t bytes = sizeof(TapeCompanionShared);
    if (companion == NULL || name == NULL || name[0] == '\0') {
        set_error(error, error_size, "Invalid companion endpoint");
        return 0;
    }
    tapeCompanionClose(companion);
    normalized_name(companion->name, sizeof(companion->name), name);
#ifdef _WIN32
    companion->mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
        PAGE_READWRITE, 0, (DWORD)bytes, companion->name);
    if (companion->mapping == NULL) {
        set_error(error, error_size, "Could not create companion mapping");
        return 0;
    }
    companion->shared = MapViewOfFile(companion->mapping,
                                      FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (companion->shared == NULL) {
        CloseHandle((HANDLE)companion->mapping);
        companion->mapping = NULL;
        set_error(error, error_size, "Could not map companion endpoint");
        return 0;
    }
#else
    companion->descriptor = shm_open(companion->name, O_CREAT | O_RDWR, 0600);
    if (companion->descriptor < 0 ||
        ftruncate(companion->descriptor, (off_t)bytes) != 0) {
        if (companion->descriptor >= 0) close(companion->descriptor);
        companion->descriptor = -1;
        set_error(error, error_size, "Could not create companion mapping");
        return 0;
    }
    companion->shared = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                             MAP_SHARED, companion->descriptor, 0);
    if (companion->shared == MAP_FAILED) {
        companion->shared = NULL;
        close(companion->descriptor);
        companion->descriptor = -1;
        set_error(error, error_size, "Could not map companion endpoint");
        return 0;
    }
#endif
    shared = (TapeCompanionShared *)companion->shared;
    atomic_store_u32(&shared->state, 0u);
    memset(shared, 0, sizeof(*shared));
    shared->magic = TAPE_COMPANION_MAGIC;
    shared->version = TAPE_COMPANION_VERSION;
    shared->bytes = (uint32_t)sizeof(*shared);
#ifdef _WIN32
    shared->process_id = (uint32_t)GetCurrentProcessId();
#else
    shared->process_id = (uint32_t)getpid();
#endif
    atomic_store_u32(&shared->heartbeat_ms, monotonic_milliseconds());
    atomic_store_u32(&shared->state, TAPE_COMPANION_RUNNING);
    companion->fallback_window = window;
    companion->active_window = window;
    companion->last_request_sequence = 0u;
    shared_set_window(shared, native_window_token(window));
    set_error(error, error_size, "");
    return 1;
}

void tapeCompanionSetActiveWindow(TapeCompanion *companion,
                                  SDL_Window *window)
{
    TapeCompanionShared *shared;
    if (companion == NULL || window == NULL) return;
    companion->active_window = window;
    shared = (TapeCompanionShared *)companion->shared;
    if (!shared_valid(shared)) return;
    atomic_store_u32(&shared->heartbeat_ms, monotonic_milliseconds());
    shared_set_window(shared, native_window_token(window));
}

int tapeCompanionRequestFocus(const char *name)
{
    TapeCompanionShared *shared = NULL;
    char normalized[128];
    uint64_t token = 0u;
    int available = 0;
    const size_t bytes = sizeof(TapeCompanionShared);
#ifdef _WIN32
    HANDLE mapping;
#else
    int descriptor;
    struct stat status;
#endif
    if (name == NULL || name[0] == '\0') return 0;
    normalized_name(normalized, sizeof(normalized), name);
#ifdef _WIN32
    mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, normalized);
    if (mapping == NULL) return 0;
    shared = (TapeCompanionShared *)MapViewOfFile(mapping,
                                                  FILE_MAP_ALL_ACCESS,
                                                  0, 0, bytes);
    if (shared == NULL) {
        CloseHandle(mapping);
        return 0;
    }
#else
    descriptor = shm_open(normalized, O_RDWR, 0600);
    if (descriptor < 0) return 0;
    if (fstat(descriptor, &status) != 0 || status.st_size < (off_t)bytes) {
        close(descriptor);
        return 0;
    }
    shared = (TapeCompanionShared *)mmap(NULL, bytes,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED, descriptor, 0);
    if (shared == MAP_FAILED) {
        close(descriptor);
        return 0;
    }
#endif
    if (shared_valid(shared) &&
        monotonic_milliseconds() - atomic_load_u32(&shared->heartbeat_ms) <=
            TAPE_COMPANION_STALE_MS) {
        available = 1;
        token = shared_get_window(shared);
        (void)atomic_add_u32(&shared->request_sequence, 1u);
    }
#if defined(_WIN32) && !defined(TAPE_COMPANION_TEST)
    if (token != 0u && IsWindow((HWND)(uintptr_t)token)) {
        HWND target = (HWND)(uintptr_t)token;
        if (IsIconic(target)) ShowWindow(target, SW_RESTORE);
        (void)BringWindowToTop(target);
        (void)SetForegroundWindow(target);
    }
#endif
#ifdef _WIN32
    UnmapViewOfFile(shared);
    CloseHandle(mapping);
#else
    (void)token;
    munmap(shared, bytes);
    close(descriptor);
#endif
    return available;
}

int tapeCompanionPump(TapeCompanion *companion)
{
    TapeCompanionShared *shared;
    SDL_Window *target;
    uint32_t sequence;
    if (companion == NULL) return 0;
    shared = (TapeCompanionShared *)companion->shared;
    if (!shared_valid(shared)) return 0;
    atomic_store_u32(&shared->heartbeat_ms, monotonic_milliseconds());
    sequence = atomic_load_u32(&shared->request_sequence);
    if (sequence == companion->last_request_sequence) return 0;
    companion->last_request_sequence = sequence;
    target = companion->active_window;
    if (target == NULL ||
        (SDL_GetWindowFlags(target) & SDL_WINDOW_HIDDEN) != 0u)
        target = companion->fallback_window;
    if (target == NULL) return 1;
    if ((SDL_GetWindowFlags(target) & SDL_WINDOW_MINIMIZED) != 0u)
        SDL_RestoreWindow(target);
    SDL_ShowWindow(target);
    SDL_RaiseWindow(target);
    (void)SDL_SetWindowInputFocus(target);
    tapeCompanionSetActiveWindow(companion, target);
    return 1;
}

void tapeCompanionClose(TapeCompanion *companion)
{
    if (companion == NULL) return;
    if (companion->shared != NULL) {
        TapeCompanionShared *shared =
            (TapeCompanionShared *)companion->shared;
        if (shared->magic == TAPE_COMPANION_MAGIC)
            atomic_store_u32(&shared->state, 0u);
#ifdef _WIN32
        UnmapViewOfFile(companion->shared);
#else
        munmap(companion->shared, sizeof(TapeCompanionShared));
#endif
        companion->shared = NULL;
    }
#ifdef _WIN32
    if (companion->mapping != NULL) {
        CloseHandle((HANDLE)companion->mapping);
        companion->mapping = NULL;
    }
#else
    if (companion->descriptor >= 0) {
        close(companion->descriptor);
        companion->descriptor = -1;
    }
    if (companion->name[0] != '\0') shm_unlink(companion->name);
#endif
    companion->name[0] = '\0';
    companion->last_request_sequence = 0u;
    companion->fallback_window = NULL;
    companion->active_window = NULL;
}
