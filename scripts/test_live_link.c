#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "../src/tape_link.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#define test_pid _getpid
#define test_sleep_ms(milliseconds) Sleep(milliseconds)
#else
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define test_pid getpid
static void test_sleep_ms(long milliseconds)
{
    struct timespec delay = {
        milliseconds / 1000L, (milliseconds % 1000L) * 1000000L
    };
    (void)nanosleep(&delay, NULL);
}
#endif

static int child_reader(const char *name)
{
    TapeLinkReader reader;
    float output[2048];
    char error[160];
    tapeLinkReaderInit(&reader);
    if (!tapeLinkReaderOpenNamed(&reader, name, error, sizeof(error))) return 2;
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (tapeLinkReaderRead(&reader, output, 1024u, 48000u) != 1024u)
            return 3;
        if (fabsf(output[2046] - 0.25f) < 0.001f &&
            fabsf(output[2047] + 0.5f) < 0.001f) {
            tapeLinkReaderClose(&reader);
            return 0;
        }
        test_sleep_ms(10);
    }
    tapeLinkReaderClose(&reader);
    return 4;
}

static void test_short_producer_quantum(const char *base_name,
                                        size_t receiver_frames)
{
    TapeLinkWriter writer;
    TapeLinkReader reader;
    TapeLinkStatus status;
    float input[512];
    float output[4096];
    char name[128];
    char error[160];
    snprintf(name, sizeof(name), "%s_%zu", base_name, receiver_frames);
    for (size_t frame = 0u; frame < 256u; ++frame) {
        input[frame * 2u] = 0.125f;
        input[frame * 2u + 1u] = -0.25f;
    }
    tapeLinkWriterInit(&writer);
    tapeLinkReaderInit(&reader);
    assert(tapeLinkWriterOpenNamed(&writer, name, 48000u, error,
                                   sizeof(error)));
    assert(tapeLinkReaderOpenNamed(&reader, name, error, sizeof(error)));
    for (int cycle = 0; cycle < 32; ++cycle) {
        size_t chunks = receiver_frames / 256u;
        /* Model one late 256-frame producer wake and its next-cycle catch-up.
           The 25 ms/two-consumer-callback reserve must absorb both. */
        if (cycle == 12) --chunks;
        else if (cycle == 13) ++chunks;
        for (size_t chunk = 0u; chunk < chunks; ++chunk)
            assert(tapeLinkWriterWrite(&writer, input, 256u) == 256u);
        assert(tapeLinkReaderRead(&reader, output, receiver_frames, 48000u) ==
               receiver_frames);
        if (cycle >= 8) {
            assert(fabsf(output[(receiver_frames - 1u) * 2u] - 0.125f) <
                   0.001f);
            assert(fabsf(output[(receiver_frames - 1u) * 2u + 1u] + 0.25f) <
                   0.001f);
        }
    }
    tapeLinkReaderStatus(&reader, &status);
    assert(status.underruns == 0u);
    tapeLinkReaderClose(&reader);
    tapeLinkWriterClose(&writer);
}

int main(int argc, char **argv)
{
    TapeLinkWriter writer;
    TapeLinkWriter replacement_writer;
    TapeLinkReader reader;
    TapeLinkReader replacement_reader;
    TapeLinkStatus status;
    float input[8192];
    float output[2048];
    char name[96];
    char error[160];

    if (argc == 3 && strcmp(argv[1], "--reader") == 0)
        return child_reader(argv[2]);

    snprintf(name, sizeof(name), "tape_link_test_%ld", (long)test_pid());
    test_short_producer_quantum(name, 256u);
    test_short_producer_quantum(name, 512u);
    test_short_producer_quantum(name, 1024u);
    test_short_producer_quantum(name, 2048u);
    tapeLinkWriterInit(&writer);
    tapeLinkWriterInit(&replacement_writer);
    tapeLinkReaderInit(&reader);
    tapeLinkReaderInit(&replacement_reader);
    assert(!tapeLinkReaderOpenNamed(&reader, name, error, sizeof(error)));
    memset(output, 0x7f, sizeof(output));
    assert(tapeLinkReaderRead(&reader, output, 1024u, 48000u) == 1024u);
    for (size_t sample = 0u; sample < 2048u; ++sample)
        assert(output[sample] == 0.0f);
    assert(tapeLinkWriterOpenNamed(&writer, name, 44100u, error,
                                   sizeof(error)));
    assert(tapeLinkReaderOpenNamed(&reader, name, error, sizeof(error)));
    assert(tapeLinkReaderSendCommand(
        &reader, TAPE_LINK_COMMAND_TOGGLE_SONG));
    assert(!tapeLinkReaderSendCommand(
        &reader, TAPE_LINK_COMMAND_TOGGLE_PATTERN));
    assert(tapeLinkWriterTakeCommand(&writer) ==
           TAPE_LINK_COMMAND_TOGGLE_SONG);
    assert(tapeLinkWriterTakeCommand(&writer) == TAPE_LINK_COMMAND_NONE);
    assert(tapeLinkReaderSendCommand(
        &reader, TAPE_LINK_COMMAND_TOGGLE_PATTERN));
    assert(tapeLinkWriterTakeCommand(&writer) ==
           TAPE_LINK_COMMAND_TOGGLE_PATTERN);
    assert(tapeLinkReaderTransportState(&reader) ==
           TAPE_LINK_TRANSPORT_STOPPED);
    tapeLinkWriterSetTransportState(&writer, TAPE_LINK_TRANSPORT_SONG);
    assert(tapeLinkReaderTransportState(&reader) ==
           TAPE_LINK_TRANSPORT_SONG);
    tapeLinkReaderStatus(&reader, &status);
    assert(status.transport_state == TAPE_LINK_TRANSPORT_SONG);
    tapeLinkWriterSetTransportState(&writer, TAPE_LINK_TRANSPORT_PATTERN);
    assert(tapeLinkReaderTransportState(&reader) ==
           TAPE_LINK_TRANSPORT_PATTERN);
    for (size_t frame = 0u; frame < 4096u; ++frame) {
        input[frame * 2u] = 0.25f;
        input[frame * 2u + 1u] = -0.5f;
    }
    /* A 512-frame callback at 48 kHz primes from the nominal 25 ms reserve.
       The former 50 ms target would still return silence here. */
    assert(tapeLinkWriterWrite(&writer, input, 1200u) == 1200u);
    assert(tapeLinkReaderRead(&reader, output, 512u, 48000u) == 512u);
    assert(fabsf(output[1022] - 0.25f) < 0.001f);
    assert(fabsf(output[1023] + 0.5f) < 0.001f);
    tapeLinkReaderStatus(&reader, &status);
    assert(status.connected);
    assert(status.sample_rate == 44100u);
    assert(status.buffered_frames > 0u);
#ifdef _WIN32
    {
        int child_status = 0;
        intptr_t child = _spawnl(_P_NOWAIT, argv[0], argv[0], "--reader",
                                  name, (char *)NULL);
        assert(child != -1);
        test_sleep_ms(100);
        assert(tapeLinkWriterWrite(&writer, input, 4096u) == 4096u);
        assert(_cwait(&child_status, child, 0) == child);
        assert(child_status == 0);
    }
#else
    {
        pid_t child = fork();
        int child_status = 0;
        assert(child >= 0);
        if (child == 0) _exit(child_reader(name));
        test_sleep_ms(100);
        assert(tapeLinkWriterWrite(&writer, input, 4096u) == 4096u);
        assert(waitpid(child, &child_status, 0) == child);
        assert(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
    }
#endif

    /* A newer producer owns the name. Closing the superseded producer must
       not unlink the replacement's mapping on POSIX. */
    assert(tapeLinkWriterOpenNamed(&replacement_writer, name, 48000u, error,
                                   sizeof(error)));
    assert(tapeLinkWriterWrite(&writer, input, 1u) == 0u);
    tapeLinkWriterClose(&writer);
    assert(tapeLinkReaderOpenNamed(&replacement_reader, name, error,
                                   sizeof(error)));
    assert(tapeLinkWriterWrite(&replacement_writer, input, 4096u) == 4096u);
    assert(tapeLinkReaderSendCommand(
        &replacement_reader, TAPE_LINK_COMMAND_TOGGLE_SONG));
    assert(tapeLinkWriterTakeCommand(&replacement_writer) ==
           TAPE_LINK_COMMAND_TOGGLE_SONG);
    assert(tapeLinkReaderRead(&replacement_reader, output, 1024u, 48000u) ==
           1024u);
    assert(fabsf(output[2046] - 0.25f) < 0.001f);
    assert(fabsf(output[2047] + 0.5f) < 0.001f);
    tapeLinkWriterSetTransportState(&replacement_writer,
                                    TAPE_LINK_TRANSPORT_PATTERN);
    assert(tapeLinkReaderTransportState(&replacement_reader) ==
           TAPE_LINK_TRANSPORT_PATTERN);
    tapeLinkWriterClose(&replacement_writer);
    assert(tapeLinkReaderRead(&replacement_reader, output, 1024u, 48000u) ==
           1024u);
    assert(tapeLinkReaderTransportState(&replacement_reader) ==
           TAPE_LINK_TRANSPORT_STOPPED);
    assert(fabsf(output[2046]) < 0.0001f);
    assert(fabsf(output[2047]) < 0.0001f);
    tapeLinkReaderClose(&reader);
    tapeLinkReaderClose(&replacement_reader);
    memset(output, 0x7f, sizeof(output));
    assert(tapeLinkReaderRead(&replacement_reader, output, 1024u, 48000u) ==
           1024u);
    for (size_t sample = 0u; sample < 2048u; ++sample)
        assert(output[sample] == 0.0f);
    puts("Tapehead/TapeSister Live Link transport tests passed");
    return 0;
}
