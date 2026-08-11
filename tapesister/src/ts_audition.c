#include "tapesister/ts_audition.h"

#include <float.h>
#include <math.h>
#include <string.h>

static bool valid_source(const ts_audition_source *source) {
  if (source == NULL || source->samples == NULL || source->frame_count == 0 ||
      source->sample_rate < 8000 || source->sample_rate > 192000 ||
      source->root_midi_note > 127)
    return false;
  for (size_t i = 0; i < source->frame_count; i++)
    if (!isfinite(source->samples[i]))
      return false;
  return true;
}

double ts_audition_step_for(const uint8_t note, const uint8_t root,
                            const uint32_t source_rate,
                            const uint32_t device_rate) {
  if (note > 127 || root > 127 || source_rate == 0 || device_rate == 0)
    return 0.0;
  return exp2(((double)note - root) / 12.0) * source_rate / device_rate;
}

bool ts_audition_init(ts_audition_mixer *mixer, const uint32_t rate) {
  if (mixer == NULL || rate < 8000 || rate > 384000)
    return false;
  memset(mixer, 0, sizeof(*mixer));
  mixer->device_sample_rate = rate;
  mixer->mode = TS_AUDITION_ONE_SHOT;
  return true;
}

static void start_voice(ts_audition_mixer *m, ts_audition_voice *v,
                        const ts_audition_source *source, uint8_t note,
                        double step) {
  memset(v, 0, sizeof(*v));
  v->source = source;
  v->note = note;
  v->step = step;
  v->age = ++m->next_age;
  v->active = true;
  v->ramp = TS_AUDITION_START_RAMP;
  v->gain = 0.0f;
  v->gain_step = 1.0f / (float)TS_AUDITION_START_RAMP;
}

bool ts_audition_note_on(ts_audition_mixer *m, const ts_audition_source *source,
                         const uint8_t note) {
  if (m == NULL || note > 127 || !valid_source(source))
    return false;
  const double step = ts_audition_step_for(
      note, source->root_midi_note, source->sample_rate, m->device_sample_rate);
  if (!isfinite(step) || step <= 0.0)
    return false;
  ts_audition_voice *chosen = NULL;
  for (size_t i = 0; i < TS_AUDITION_VOICES; i++)
    if (!m->voices[i].active) {
      chosen = &m->voices[i];
      break;
    }
  if (chosen != NULL) {
    start_voice(m, chosen, source, note, step);
    return true;
  }
  chosen = &m->voices[0];
  for (size_t i = 1; i < TS_AUDITION_VOICES; i++)
    if (m->voices[i].age < chosen->age)
      chosen = &m->voices[i];
  chosen->releasing = true;
  chosen->ramp = TS_AUDITION_RELEASE_RAMP;
  chosen->gain_step = -chosen->gain / (float)TS_AUDITION_RELEASE_RAMP;
  chosen->pending = true;
  chosen->pending_source = source;
  chosen->pending_note = note;
  chosen->pending_step = step;
  return true;
}

void ts_audition_note_off(ts_audition_mixer *m, const uint8_t note) {
  if (m == NULL || m->mode == TS_AUDITION_ONE_SHOT)
    return;
  for (size_t i = 0; i < TS_AUDITION_VOICES; i++)
    if (m->voices[i].active && m->voices[i].note == note &&
        !m->voices[i].releasing) {
      m->voices[i].releasing = true;
      m->voices[i].ramp = TS_AUDITION_RELEASE_RAMP;
      m->voices[i].gain_step =
          -m->voices[i].gain / (float)TS_AUDITION_RELEASE_RAMP;
    }
}

void ts_audition_stop_all(ts_audition_mixer *m) {
  if (m == NULL)
    return;
  for (size_t i = 0; i < TS_AUDITION_VOICES; i++)
    if (m->voices[i].active) {
      m->voices[i].pending = false;
      m->voices[i].releasing = true;
      m->voices[i].ramp = TS_AUDITION_RELEASE_RAMP;
      m->voices[i].gain_step =
          -m->voices[i].gain / (float)TS_AUDITION_RELEASE_RAMP;
    }
}

static void finish_or_pending(ts_audition_mixer *m, ts_audition_voice *v) {
  if (v->pending) {
    const ts_audition_source *source = v->pending_source;
    const uint8_t note = v->pending_note;
    const double step = v->pending_step;
    start_voice(m, v, source, note, step);
  } else
    memset(v, 0, sizeof(*v));
}

void ts_audition_mix(ts_audition_mixer *m, float *stereo, const size_t frames) {
  if (stereo == NULL)
    return;
  memset(stereo, 0, frames * 2U * sizeof(*stereo));
  if (m == NULL)
    return;
  for (size_t frame = 0; frame < frames; frame++) {
    double sum = 0.0;
    for (size_t i = 0; i < TS_AUDITION_VOICES; i++) {
      ts_audition_voice *v = &m->voices[i];
      if (!v->active)
        continue;
      const size_t index = (size_t)v->position;
      if (index >= v->source->frame_count) {
        finish_or_pending(m, v);
        continue;
      }
      const size_t next =
          index + 1U < v->source->frame_count ? index + 1U : index;
      const float fraction = (float)(v->position - (double)index);
      float sample =
          v->source->samples[index] +
          (v->source->samples[next] - v->source->samples[index]) * fraction;
      sum += sample * v->gain;
      v->position += v->step;
      if (v->ramp > 0) {
        v->gain += v->gain_step;
        v->ramp--;
      }
      if (!v->releasing && v->ramp == 0) {
        v->gain = 1.0f;
        v->gain_step = 0.0f;
      }
      if (v->releasing && v->ramp == 0)
        finish_or_pending(m, v);
    }
    if (!isfinite(sum)) {
      sum = 0.0;
      m->overload = true;
    }
    if (sum > 1.0) {
      sum = 1.0;
      m->overload = true;
    } else if (sum < -1.0) {
      sum = -1.0;
      m->overload = true;
    }
    stereo[frame * 2U] = stereo[frame * 2U + 1U] = (float)sum;
  }
}

size_t ts_audition_active_voices(const ts_audition_mixer *m) {
  if (m == NULL)
    return 0;
  size_t count = 0;
  for (size_t i = 0; i < TS_AUDITION_VOICES; i++)
    if (m->voices[i].active)
      count++;
  return count;
}
