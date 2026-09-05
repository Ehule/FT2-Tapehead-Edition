#ifndef TAPE_LINK_H
#define TAPE_LINK_H

#include <stddef.h>
#include <stdint.h>

#define TAPE_LINK_DEFAULT_NAME "tapehead_tapesister_livelink_v1"
#define TAPE_LINK_CHANNELS 2u
#define TAPE_LINK_CAPACITY_FRAMES 131072u

typedef struct {
    void *mapping;
    void *shared;
    int descriptor;
    char name[128];
    uint32_t session;
} TapeLinkWriter;

typedef struct {
    void *mapping;
    void *shared;
    int descriptor;
    char name[128];
    uint32_t session;
    uint32_t sample_rate;
    uint32_t output_rate;
    uint32_t last_heartbeat;
    uint32_t stale_frames;
    double phase;
    float current[2];
    float next[2];
    float last[2];
    float fade;
    int primed;
    int connected;
} TapeLinkReader;

typedef struct {
    int connected;
    uint32_t sample_rate;
    uint32_t buffered_frames;
    uint32_t session;
    uint32_t overruns;
    uint32_t underruns;
} TapeLinkStatus;

void tapeLinkWriterInit(TapeLinkWriter *writer);
int tapeLinkWriterOpen(TapeLinkWriter *writer, uint32_t sample_rate,
                       char *error, size_t error_size);
int tapeLinkWriterOpenNamed(TapeLinkWriter *writer, const char *name,
                            uint32_t sample_rate, char *error,
                            size_t error_size);
size_t tapeLinkWriterWrite(TapeLinkWriter *writer, const float *interleaved,
                           size_t frames);
void tapeLinkWriterClose(TapeLinkWriter *writer);
void tapeLinkWriterStatus(const TapeLinkWriter *writer, TapeLinkStatus *status);

void tapeLinkReaderInit(TapeLinkReader *reader);
int tapeLinkReaderOpen(TapeLinkReader *reader, char *error, size_t error_size);
int tapeLinkReaderOpenNamed(TapeLinkReader *reader, const char *name,
                            char *error, size_t error_size);
size_t tapeLinkReaderRead(TapeLinkReader *reader, float *interleaved,
                          size_t frames, uint32_t output_rate);
void tapeLinkReaderClose(TapeLinkReader *reader);
void tapeLinkReaderStatus(const TapeLinkReader *reader, TapeLinkStatus *status);

#endif
