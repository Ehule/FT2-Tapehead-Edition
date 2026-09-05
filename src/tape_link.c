#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "tape_link.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

enum {
    TAPE_LINK_MAGIC = 0x54484C4Bu,
    TAPE_LINK_VERSION = 1u,
    TAPE_LINK_RUNNING = 1u
};

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t header_bytes;
    uint32_t capacity_frames;
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t session;
    volatile uint32_t state;
    volatile uint32_t write_frame;
    volatile uint32_t read_frame;
    volatile uint32_t heartbeat;
    volatile uint32_t overruns;
    volatile uint32_t underruns;
    uint32_t reserved[3];
    float samples[TAPE_LINK_CAPACITY_FRAMES * TAPE_LINK_CHANNELS];
} TapeLinkShared;

typedef char TapeLinkCapacityMustBePowerOfTwo[
    (TAPE_LINK_CAPACITY_FRAMES & (TAPE_LINK_CAPACITY_FRAMES - 1u)) == 0u ?
    1 : -1];

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
    return __atomic_add_fetch(value, amount, __ATOMIC_RELAXED);
#endif
}

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error == NULL || error_size == 0u) return;
    snprintf(error, error_size, "%s", message != NULL ? message : "");
}

static void normalized_name(char *output, size_t output_size, const char *name)
{
    if (output == NULL || output_size == 0u) return;
#ifdef _WIN32
    snprintf(output, output_size, "Local\\%s",
             name != NULL && name[0] != '\0' ? name : TAPE_LINK_DEFAULT_NAME);
#else
    snprintf(output, output_size, "/%s",
             name != NULL && name[0] != '\0' ? name : TAPE_LINK_DEFAULT_NAME);
#endif
}

static int map_writer(TapeLinkWriter *writer, const char *name,
                      char *error, size_t error_size)
{
    const size_t bytes = sizeof(TapeLinkShared);
    normalized_name(writer->name, sizeof(writer->name), name);
#ifdef _WIN32
    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
        PAGE_READWRITE, (DWORD)(((uint64_t)bytes) >> 32), (DWORD)bytes,
        writer->name);
    if (mapping == NULL) {
        set_error(error, error_size, "Could not create the Live Link mapping");
        return 0;
    }
    writer->mapping = mapping;
    writer->shared = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (writer->shared == NULL) {
        CloseHandle(mapping);
        writer->mapping = NULL;
        set_error(error, error_size, "Could not map the Live Link buffer");
        return 0;
    }
#else
    writer->descriptor = shm_open(writer->name, O_CREAT | O_RDWR, 0600);
    if (writer->descriptor < 0) {
        set_error(error, error_size, strerror(errno));
        return 0;
    }
    if (ftruncate(writer->descriptor, (off_t)bytes) != 0) {
        set_error(error, error_size, strerror(errno));
        close(writer->descriptor);
        writer->descriptor = -1;
        shm_unlink(writer->name);
        return 0;
    }
    writer->shared = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED,
                          writer->descriptor, 0);
    if (writer->shared == MAP_FAILED) {
        writer->shared = NULL;
        set_error(error, error_size, strerror(errno));
        close(writer->descriptor);
        writer->descriptor = -1;
        return 0;
    }
#endif
    return 1;
}

static int map_reader(TapeLinkReader *reader, const char *name,
                      char *error, size_t error_size)
{
    const size_t bytes = sizeof(TapeLinkShared);
    normalized_name(reader->name, sizeof(reader->name), name);
#ifdef _WIN32
    HANDLE mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, reader->name);
    if (mapping == NULL) {
        set_error(error, error_size, "Tapehead Live Link is not running");
        return 0;
    }
    reader->mapping = mapping;
    reader->shared = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (reader->shared == NULL) {
        CloseHandle(mapping);
        reader->mapping = NULL;
        set_error(error, error_size, "Could not map the Live Link buffer");
        return 0;
    }
#else
    struct stat mapping_status;
    reader->descriptor = shm_open(reader->name, O_RDWR, 0600);
    if (reader->descriptor < 0) {
        set_error(error, error_size, errno == ENOENT ?
                  "Tapehead Live Link is not running" : strerror(errno));
        return 0;
    }
    if (fstat(reader->descriptor, &mapping_status) != 0 ||
        mapping_status.st_size < (off_t)bytes) {
        set_error(error, error_size, "Tapehead Live Link has an incompatible mapping");
        close(reader->descriptor);
        reader->descriptor = -1;
        return 0;
    }
    reader->shared = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED,
                          reader->descriptor, 0);
    if (reader->shared == MAP_FAILED) {
        reader->shared = NULL;
        set_error(error, error_size, strerror(errno));
        close(reader->descriptor);
        reader->descriptor = -1;
        return 0;
    }
#endif
    return 1;
}

static int shared_valid(const TapeLinkShared *shared)
{
    return shared != NULL && shared->magic == TAPE_LINK_MAGIC &&
           shared->version == TAPE_LINK_VERSION &&
           shared->header_bytes == offsetof(TapeLinkShared, samples) &&
           shared->capacity_frames == TAPE_LINK_CAPACITY_FRAMES &&
           shared->channels == TAPE_LINK_CHANNELS &&
           shared->sample_rate >= 8000u && shared->sample_rate <= 384000u;
}

void tapeLinkWriterInit(TapeLinkWriter *writer)
{
    if (writer == NULL) return;
    memset(writer, 0, sizeof(*writer));
    writer->descriptor = -1;
}

int tapeLinkWriterOpen(TapeLinkWriter *writer, uint32_t sample_rate,
                       char *error, size_t error_size)
{
    return tapeLinkWriterOpenNamed(writer, TAPE_LINK_DEFAULT_NAME, sample_rate,
                                   error, error_size);
}

int tapeLinkWriterOpenNamed(TapeLinkWriter *writer, const char *name,
                            uint32_t sample_rate, char *error,
                            size_t error_size)
{
    TapeLinkShared *shared;
    uint32_t old_session;
    if (writer == NULL || sample_rate < 8000u || sample_rate > 384000u) {
        set_error(error, error_size, "Invalid Live Link sample rate");
        return 0;
    }
    tapeLinkWriterClose(writer);
    if (!map_writer(writer, name, error, error_size)) return 0;
    shared = (TapeLinkShared *)writer->shared;
    old_session = shared_valid(shared) ? shared->session : 0u;
    atomic_store_u32(&shared->state, 0u);
    memset(shared, 0, sizeof(*shared));
    shared->magic = TAPE_LINK_MAGIC;
    shared->version = TAPE_LINK_VERSION;
    shared->header_bytes = (uint32_t)offsetof(TapeLinkShared, samples);
    shared->capacity_frames = TAPE_LINK_CAPACITY_FRAMES;
    shared->channels = TAPE_LINK_CHANNELS;
    shared->sample_rate = sample_rate;
    writer->session = old_session + 1u;
    if (writer->session == 0u) writer->session = 1u;
    shared->session = writer->session;
    atomic_store_u32(&shared->state, TAPE_LINK_RUNNING);
    set_error(error, error_size, "");
    return 1;
}

size_t tapeLinkWriterWrite(TapeLinkWriter *writer, const float *interleaved,
                           size_t frames)
{
    TapeLinkShared *shared;
    uint32_t write_frame, read_frame, available, free_frames;
    size_t accepted;
    if (writer == NULL || interleaved == NULL || frames == 0u) return 0u;
    shared = (TapeLinkShared *)writer->shared;
    if (!shared_valid(shared) ||
        atomic_load_u32(&shared->state) != TAPE_LINK_RUNNING ||
        shared->session != writer->session) return 0u;
    write_frame = atomic_load_u32(&shared->write_frame);
    read_frame = atomic_load_u32(&shared->read_frame);
    available = write_frame - read_frame;
    if (available > TAPE_LINK_CAPACITY_FRAMES) available = TAPE_LINK_CAPACITY_FRAMES;
    free_frames = TAPE_LINK_CAPACITY_FRAMES - available;
    accepted = frames < free_frames ? frames : free_frames;
    for (size_t i = 0u; i < accepted; ++i) {
        uint32_t at = (write_frame + (uint32_t)i) &
                      (TAPE_LINK_CAPACITY_FRAMES - 1u);
        float left = interleaved[i * 2u];
        float right = interleaved[i * 2u + 1u];
        shared->samples[at * 2u] = isfinite(left) ? left : 0.0f;
        shared->samples[at * 2u + 1u] = isfinite(right) ? right : 0.0f;
    }
    atomic_store_u32(&shared->write_frame, write_frame + (uint32_t)accepted);
    (void)atomic_add_u32(&shared->heartbeat, 1u);
    if (accepted < frames)
        (void)atomic_add_u32(&shared->overruns, (uint32_t)(frames - accepted));
    return accepted;
}

void tapeLinkWriterClose(TapeLinkWriter *writer)
{
    int owns_session = 0;
    if (writer == NULL) return;
    if (writer->shared != NULL) {
        TapeLinkShared *shared = (TapeLinkShared *)writer->shared;
        owns_session = shared_valid(shared) &&
                       shared->session == writer->session;
        if (owns_session)
            atomic_store_u32(&shared->state, 0u);
#ifdef _WIN32
        UnmapViewOfFile(writer->shared);
#else
        munmap(writer->shared, sizeof(TapeLinkShared));
#endif
    }
#ifdef _WIN32
    if (writer->mapping != NULL) CloseHandle((HANDLE)writer->mapping);
#else
    if (writer->descriptor >= 0) close(writer->descriptor);
    if (owns_session && writer->name[0] != '\0') shm_unlink(writer->name);
#endif
    tapeLinkWriterInit(writer);
}

void tapeLinkWriterStatus(const TapeLinkWriter *writer, TapeLinkStatus *status)
{
    const TapeLinkShared *shared;
    if (status == NULL) return;
    memset(status, 0, sizeof(*status));
    if (writer == NULL) return;
    shared = (const TapeLinkShared *)writer->shared;
    if (!shared_valid(shared) || shared->session != writer->session) return;
    status->connected = atomic_load_u32(&shared->state) == TAPE_LINK_RUNNING;
    status->sample_rate = shared->sample_rate;
    status->buffered_frames = atomic_load_u32(&shared->write_frame) -
                              atomic_load_u32(&shared->read_frame);
    if (status->buffered_frames > TAPE_LINK_CAPACITY_FRAMES)
        status->buffered_frames = TAPE_LINK_CAPACITY_FRAMES;
    status->session = shared->session;
    status->overruns = atomic_load_u32(&shared->overruns);
    status->underruns = atomic_load_u32(&shared->underruns);
}

void tapeLinkReaderInit(TapeLinkReader *reader)
{
    if (reader == NULL) return;
    memset(reader, 0, sizeof(*reader));
    reader->descriptor = -1;
}

int tapeLinkReaderOpen(TapeLinkReader *reader, char *error, size_t error_size)
{
    return tapeLinkReaderOpenNamed(reader, TAPE_LINK_DEFAULT_NAME, error,
                                   error_size);
}

int tapeLinkReaderOpenNamed(TapeLinkReader *reader, const char *name,
                            char *error, size_t error_size)
{
    TapeLinkShared *shared;
    if (reader == NULL) return 0;
    tapeLinkReaderClose(reader);
    if (!map_reader(reader, name, error, error_size)) return 0;
    shared = (TapeLinkShared *)reader->shared;
    if (atomic_load_u32(&shared->state) != TAPE_LINK_RUNNING ||
        !shared_valid(shared)) {
        tapeLinkReaderClose(reader);
        set_error(error, error_size, "Tapehead Live Link has an incompatible session");
        return 0;
    }
    reader->session = shared->session;
    reader->sample_rate = shared->sample_rate;
    /* Live Link is a live monitor, not a delayed-history transfer. A newly
       connected consumer begins with the next producer frame. */
    atomic_store_u32(&shared->read_frame,
                     atomic_load_u32(&shared->write_frame));
    reader->last_heartbeat = atomic_load_u32(&shared->heartbeat);
    reader->connected = 1;
    set_error(error, error_size, "");
    return 1;
}

static int reader_take(TapeLinkReader *reader, float frame[2])
{
    TapeLinkShared *shared = (TapeLinkShared *)reader->shared;
    uint32_t write_frame, read_frame, available, at;
    if (atomic_load_u32(&shared->state) != TAPE_LINK_RUNNING ||
        !shared_valid(shared) || shared->session != reader->session) return 0;
    write_frame = atomic_load_u32(&shared->write_frame);
    read_frame = atomic_load_u32(&shared->read_frame);
    available = write_frame - read_frame;
    if (available == 0u || available > TAPE_LINK_CAPACITY_FRAMES) return 0;
    at = read_frame & (TAPE_LINK_CAPACITY_FRAMES - 1u);
    frame[0] = shared->samples[at * 2u];
    frame[1] = shared->samples[at * 2u + 1u];
    atomic_store_u32(&shared->read_frame, read_frame + 1u);
    return 1;
}

static int reader_prime(TapeLinkReader *reader)
{
    TapeLinkShared *shared = (TapeLinkShared *)reader->shared;
    uint32_t write_frame, read_frame, available, target;
    if (!shared_valid(shared)) return 0;
    write_frame = atomic_load_u32(&shared->write_frame);
    read_frame = atomic_load_u32(&shared->read_frame);
    available = write_frame - read_frame;
    if (available > TAPE_LINK_CAPACITY_FRAMES) {
        read_frame = write_frame;
        available = 0u;
        atomic_store_u32(&shared->read_frame, read_frame);
    }
    target = reader->sample_rate / 20u;
    if (target < 256u) target = 256u;
    if (target > TAPE_LINK_CAPACITY_FRAMES / 2u)
        target = TAPE_LINK_CAPACITY_FRAMES / 2u;
    if (available < target) return 0;
    if (available > target) {
        read_frame = write_frame - target;
        atomic_store_u32(&shared->read_frame, read_frame);
    }
    if (!reader_take(reader, reader->current) ||
        !reader_take(reader, reader->next)) return 0;
    reader->phase = 0.0;
    reader->primed = 1;
    return 1;
}

static void reader_fade_tail(TapeLinkReader *reader, float *interleaved,
                             size_t frames, uint32_t output_rate)
{
    float gain_step = 1.0f / ((float)output_rate * 0.010f);
    for (size_t i = 0u; i < frames; ++i) {
        reader->fade -= gain_step;
        if (reader->fade < 0.0f) reader->fade = 0.0f;
        interleaved[i * 2u] = reader->last[0] * reader->fade;
        interleaved[i * 2u + 1u] = reader->last[1] * reader->fade;
    }
}

size_t tapeLinkReaderRead(TapeLinkReader *reader, float *interleaved,
                          size_t frames, uint32_t output_rate)
{
    TapeLinkShared *shared;
    uint32_t heartbeat, write_frame, read_frame, buffered, target;
    double ratio, correction;
    if (interleaved == NULL || frames == 0u) return 0u;
    memset(interleaved, 0, frames * TAPE_LINK_CHANNELS * sizeof(float));
    if (reader == NULL || output_rate == 0u) return frames;
    shared = (TapeLinkShared *)reader->shared;
    if (shared == NULL || !shared_valid(shared) ||
        atomic_load_u32(&shared->state) != TAPE_LINK_RUNNING ||
        shared->session != reader->session) {
        reader->connected = 0;
        reader->primed = 0;
        reader_fade_tail(reader, interleaved, frames, output_rate);
        return frames;
    }
    reader->sample_rate = shared->sample_rate;
    reader->output_rate = output_rate;
    heartbeat = atomic_load_u32(&shared->heartbeat);
    if (heartbeat == reader->last_heartbeat) {
        uint32_t addition = (uint32_t)frames;
        reader->stale_frames = UINT32_MAX - reader->stale_frames < addition ?
                               UINT32_MAX : reader->stale_frames + addition;
    }
    else {
        reader->last_heartbeat = heartbeat;
        reader->stale_frames = 0u;
    }
    write_frame = atomic_load_u32(&shared->write_frame);
    read_frame = atomic_load_u32(&shared->read_frame);
    buffered = write_frame - read_frame;
    if (buffered > TAPE_LINK_CAPACITY_FRAMES) buffered = 0u;
    reader->connected = reader->stale_frames < output_rate / 2u || buffered > 0u;
    if (!reader->primed && !reader_prime(reader)) {
        reader_fade_tail(reader, interleaved, frames, output_rate);
        return frames;
    }
    target = reader->sample_rate / 20u;
    if (target < 256u) target = 256u;
    correction = target > 0u ?
        ((double)buffered - (double)target) / (double)target * 0.002 : 0.0;
    if (correction < -0.002) correction = -0.002;
    if (correction > 0.002) correction = 0.002;
    ratio = (double)reader->sample_rate / (double)output_rate *
            (1.0 + correction);
    for (size_t i = 0u; i < frames; ++i) {
        float gain_step = 1.0f / ((float)output_rate * 0.010f);
        if (reader->connected) {
            reader->fade += gain_step;
            if (reader->fade > 1.0f) reader->fade = 1.0f;
        } else {
            reader->fade -= gain_step;
            if (reader->fade < 0.0f) reader->fade = 0.0f;
        }
        reader->last[0] = reader->current[0] +
            (reader->next[0] - reader->current[0]) * (float)reader->phase;
        reader->last[1] = reader->current[1] +
            (reader->next[1] - reader->current[1]) * (float)reader->phase;
        interleaved[i * 2u] = reader->last[0] * reader->fade;
        interleaved[i * 2u + 1u] = reader->last[1] * reader->fade;
        reader->phase += ratio;
        while (reader->phase >= 1.0) {
            reader->current[0] = reader->next[0];
            reader->current[1] = reader->next[1];
            if (!reader_take(reader, reader->next)) {
                reader->primed = 0;
                reader->connected = 0;
                (void)atomic_add_u32(&shared->underruns, 1u);
                for (++i; i < frames; ++i) {
                    reader->fade -= gain_step;
                    if (reader->fade < 0.0f) reader->fade = 0.0f;
                    interleaved[i * 2u] = reader->last[0] * reader->fade;
                    interleaved[i * 2u + 1u] = reader->last[1] * reader->fade;
                }
                return frames;
            }
            reader->phase -= 1.0;
        }
    }
    return frames;
}

void tapeLinkReaderClose(TapeLinkReader *reader)
{
    if (reader == NULL) return;
    if (reader->shared != NULL) {
#ifdef _WIN32
        UnmapViewOfFile(reader->shared);
#else
        munmap(reader->shared, sizeof(TapeLinkShared));
#endif
    }
#ifdef _WIN32
    if (reader->mapping != NULL) CloseHandle((HANDLE)reader->mapping);
#else
    if (reader->descriptor >= 0) close(reader->descriptor);
#endif
    tapeLinkReaderInit(reader);
}

void tapeLinkReaderStatus(const TapeLinkReader *reader, TapeLinkStatus *status)
{
    const TapeLinkShared *shared;
    if (status == NULL) return;
    memset(status, 0, sizeof(*status));
    if (reader == NULL) return;
    shared = (const TapeLinkShared *)reader->shared;
    if (!shared_valid(shared) || shared->session != reader->session) return;
    status->connected = reader->connected &&
                        atomic_load_u32(&shared->state) == TAPE_LINK_RUNNING;
    status->sample_rate = shared->sample_rate;
    status->buffered_frames = atomic_load_u32(&shared->write_frame) -
                              atomic_load_u32(&shared->read_frame);
    if (status->buffered_frames > TAPE_LINK_CAPACITY_FRAMES)
        status->buffered_frames = 0u;
    status->session = shared->session;
    status->overruns = atomic_load_u32(&shared->overruns);
    status->underruns = atomic_load_u32(&shared->underruns);
}
