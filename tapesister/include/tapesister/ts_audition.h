#pragma once

#include "tapesister/ts_render.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TS_AUDITION_VOICES 16U
#define TS_AUDITION_START_RAMP 64U
#define TS_AUDITION_RELEASE_RAMP 128U
#define TS_AUDITION_VOICE_HEADROOM 0.22f

typedef enum ts_audition_mode {
  TS_AUDITION_ONE_SHOT,
  TS_AUDITION_GATED
} ts_audition_mode;

typedef struct ts_audition_source {
  const float *samples;
  size_t frame_count;
  uint32_t sample_rate;
  uint8_t root_midi_note;
  struct ts_preview *owner;
} ts_audition_source;

typedef struct ts_audition_voice {
  const ts_audition_source *source;
  const ts_audition_source *pending_source;
  double position, step, pending_step;
  float gain, gain_step;
  uint64_t age;
  uint32_t ramp;
  uint8_t note, pending_note;
  bool active, releasing, pending;
  struct ts_preview *preview, *pending_preview;
} ts_audition_voice;

typedef struct ts_audition_mixer {
  ts_audition_voice voices[TS_AUDITION_VOICES];
  uint32_t device_sample_rate;
  uint64_t next_age;
  ts_audition_mode mode;
  bool overload;
  atomic_uint_least32_t overload_generation;
  atomic_uint_least64_t cursor_age, cursor_frame, cursor_frames;
} ts_audition_mixer;

bool ts_audition_init(ts_audition_mixer *mixer, uint32_t device_sample_rate);
bool ts_audition_note_on(ts_audition_mixer *mixer,
                         const ts_audition_source *source, uint8_t midi_note);
void ts_audition_note_off(ts_audition_mixer *mixer, uint8_t midi_note);
void ts_audition_stop_all(ts_audition_mixer *mixer);
void ts_audition_discard_all(ts_audition_mixer *mixer);
void ts_audition_mix(ts_audition_mixer *mixer, float *stereo,
                     size_t frame_count);
size_t ts_audition_active_voices(const ts_audition_mixer *mixer);
double ts_audition_step_for(uint8_t note, uint8_t root_note,
                            uint32_t source_rate, uint32_t device_rate);
uint32_t ts_audition_overload_generation(const ts_audition_mixer *mixer);
double ts_audition_cursor_position(const ts_audition_mixer *mixer);
