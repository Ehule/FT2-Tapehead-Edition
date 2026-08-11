#include "tapesister/ts_audition.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  float wave[512];
  for (size_t i = 0; i < 512; i++)
    wave[i] = 0.5f;
  ts_audition_source source = {wave, 512, 48000, 60};
  ts_audition_mixer m;
  CHECK(ts_audition_init(&m, 48000));
  CHECK(fabs(ts_audition_step_for(60, 60, 48000, 48000) - 1.0) < 1e-12);
  CHECK(fabs(ts_audition_step_for(72, 60, 48000, 48000) - 2.0) < 1e-12);
  CHECK(fabs(ts_audition_step_for(48, 60, 48000, 48000) - 0.5) < 1e-12);
  CHECK(fabs(ts_audition_step_for(60, 60, 44100, 48000) - (44100.0 / 48000.0)) <
        1e-12);
  CHECK(ts_audition_step_for(128, 60, 48000, 48000) == 0.0);
  ts_audition_mixer poly;
  CHECK(ts_audition_init(&poly, 48000));
  for (int n = 0; n < 4; n++)
    CHECK(ts_audition_note_on(&poly, &source, (uint8_t)(60 + n)));
  float poly_out[512];
  ts_audition_mix(&poly, poly_out, 256);
  for (size_t i = 0; i < 512; i++)
    CHECK(isfinite(poly_out[i]) && fabsf(poly_out[i]) <= 0.45f);
  CHECK(!poly.overload);
  ts_audition_discard_all(&poly);
  CHECK(ts_audition_note_on(&m, &source, 60));
  CHECK(m.voices[0].ramp == TS_AUDITION_START_RAMP);
  float out[512] = {0};
  ts_audition_mix(&m, out, 2);
  CHECK(out[0] == 0.0f && out[2] > 0.0f);
  ts_audition_note_off(&m, 60);
  CHECK(!m.voices[0].releasing); /* one-shot ignores release */
  m.mode = TS_AUDITION_GATED;
  ts_audition_note_off(&m, 60);
  CHECK(m.voices[0].releasing);
  float before = out[2];
  ts_audition_mix(&m, out, TS_AUDITION_RELEASE_RAMP);
  CHECK(fabsf(out[0] - before) < 0.01f && ts_audition_active_voices(&m) == 0);
  for (int n = 0; n < 16; n++)
    CHECK(ts_audition_note_on(&m, &source, (uint8_t)(40 + n)));
  CHECK(ts_audition_active_voices(&m) == 16);
  uint64_t oldest = m.voices[0].age;
  CHECK(ts_audition_note_on(&m, &source, 90));
  CHECK(m.voices[0].age == oldest && m.voices[0].pending &&
        m.voices[0].pending_note == 90);
  ts_audition_mix(&m, out, TS_AUDITION_RELEASE_RAMP);
  CHECK(m.voices[0].note == 90);
  CHECK(ts_audition_note_on(
      &m, &source, 90)); /* same-note retrigger consumes another voice */
  ts_audition_stop_all(&m);
  ts_audition_mix(&m, out, TS_AUDITION_RELEASE_RAMP);
  CHECK(ts_audition_active_voices(&m) == 0);
  float tiny[] = {0.0f, 1.0f, 0.0f};
  ts_audition_source t = {tiny, 3, 48000, 60};
  CHECK(ts_audition_note_on(&m, &t, 60));
  m.voices[0].ramp = 0;
  m.voices[0].gain = 1.0f;
  ts_audition_mix(&m, out, 4);
  CHECK(out[0] == 0 && fabsf(out[2] - TS_AUDITION_VOICE_HEADROOM) < 1e-6f && out[4] == 0 &&
        ts_audition_active_voices(&m) == 0);
  ts_audition_source empty = {tiny, 0, 48000, 60}, one = {tiny, 1, 48000, 60};
  CHECK(!ts_audition_note_on(&m, &empty, 60));
  CHECK(ts_audition_note_on(&m, &one, 60));
  ts_audition_mix(&m, out, 2);
  CHECK(ts_audition_active_voices(&m) == 0);
  float bad[] = {NAN};
  ts_audition_source invalid = {bad, 1, 48000, 60};
  CHECK(!ts_audition_note_on(&m, &invalid, 60));
  for (int n = 0; n < 16; n++)
    CHECK(ts_audition_note_on(&m, &source, (uint8_t)(40 + n)));
  ts_audition_mix(&m, out, 256);
  CHECK(m.overload);
  const uint32_t overload_generation = ts_audition_overload_generation(&m);
  CHECK(overload_generation > 0);
  for (size_t i = 0; i < 512; i++)
    CHECK(isfinite(out[i]) && fabsf(out[i]) <= 1.0f);
  /* Sources are immutable and caller-owned; all active pointers remain
   * identical. */
  for (size_t i = 0; i < TS_AUDITION_VOICES; i++)
    if (m.voices[i].active)
      CHECK(m.voices[i].source == &source);
  ts_audition_stop_all(&m);
  ts_audition_mix(&m, out, TS_AUDITION_RELEASE_RAMP);
  ts_audition_mix(&m, out, 1);
  CHECK(!m.overload);
  CHECK(ts_audition_overload_generation(&m) >= overload_generation);
  puts("PASS audition mixer pitch, ramps, stealing, bounds and lifetime");
  return 0;
}
