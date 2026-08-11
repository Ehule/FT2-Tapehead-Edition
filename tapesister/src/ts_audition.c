#include "tapesister/ts_audition.h"
#include "tapesister/ts_preview.h"

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
  atomic_init(&mixer->overload_generation, 0);
  atomic_init(&mixer->cursor_age, 0);
  atomic_init(&mixer->cursor_frame, 0);
  atomic_init(&mixer->cursor_frames, 0);
  mixer->device_sample_rate = rate;
  mixer->mode = TS_AUDITION_ONE_SHOT;
  return true;
}

static void start_voice(ts_audition_mixer *m, ts_audition_voice *v,
                        const ts_audition_source *source, uint8_t note,
                        double step, ts_preview *preview) {
  memset(v, 0, sizeof(*v));
  v->source = source;
  v->preview = preview;
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
    ts_preview_retain(source->owner);
    start_voice(m, chosen, source, note, step, source->owner);
    return true;
  }
  chosen = &m->voices[0];
  for (size_t i = 1; i < TS_AUDITION_VOICES; i++)
    if (m->voices[i].age < chosen->age)
      chosen = &m->voices[i];
  chosen->releasing = true;
  chosen->ramp = TS_AUDITION_RELEASE_RAMP;
  chosen->gain_step = -chosen->gain / (float)TS_AUDITION_RELEASE_RAMP;
  if (chosen->pending) ts_preview_release_callback(chosen->pending_preview);
  chosen->pending = true;
  chosen->pending_source = source;
  ts_preview_retain(source->owner);
  chosen->pending_preview = source->owner;
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
      if (m->voices[i].pending)
        ts_preview_release_callback(m->voices[i].pending_preview);
      m->voices[i].pending = false;
      m->voices[i].pending_preview = NULL;
      m->voices[i].pending_source = NULL;
      m->voices[i].releasing = true;
      m->voices[i].ramp = TS_AUDITION_RELEASE_RAMP;
      m->voices[i].gain_step =
          -m->voices[i].gain / (float)TS_AUDITION_RELEASE_RAMP;
    }
  atomic_store_explicit(&m->cursor_age, 0, memory_order_release);
}

void ts_audition_discard_all(ts_audition_mixer *m) {
  if (m == NULL) return;
  for (size_t i = 0; i < TS_AUDITION_VOICES; i++) {
    if (m->voices[i].preview) ts_preview_release_callback(m->voices[i].preview);
    if (m->voices[i].pending_preview) ts_preview_release_callback(m->voices[i].pending_preview);
    memset(&m->voices[i], 0, sizeof(m->voices[i]));
  }
  atomic_store_explicit(&m->cursor_age, 0, memory_order_release);
}

static void finish_or_pending(ts_audition_mixer *m, ts_audition_voice *v) {
  ts_preview *finished = v->preview;
  if (v->pending) {
    const ts_audition_source *source = v->pending_source;
    const uint8_t note = v->pending_note;
    const double step = v->pending_step;
    ts_preview *preview = v->pending_preview;
    start_voice(m, v, source, note, step, preview);
  } else
    memset(v, 0, sizeof(*v));
  ts_preview_release_callback(finished);
}

void ts_audition_mix(ts_audition_mixer *m, float *stereo, const size_t frames) {
  if (stereo == NULL)
    return;
  memset(stereo, 0, frames * 2U * sizeof(*stereo));
  if (m == NULL)
    return;
  m->overload = false;
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
      sum += sample * v->gain * TS_AUDITION_VOICE_HEADROOM;
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
  if (m->overload)
    atomic_fetch_add_explicit(&m->overload_generation, 1, memory_order_release);
  uint64_t newest = 0, position = 0, source_frames = 0;
  for (size_t i = 0; i < TS_AUDITION_VOICES; i++) {
    const ts_audition_voice *v = &m->voices[i];
    if (v->active && v->age > newest && v->source != NULL) {
      newest = v->age;
      position = (uint64_t)v->position;
      source_frames = v->source->frame_count;
    }
  }
  atomic_store_explicit(&m->cursor_frame, position, memory_order_relaxed);
  atomic_store_explicit(&m->cursor_frames, source_frames, memory_order_relaxed);
  atomic_store_explicit(&m->cursor_age, newest, memory_order_release);
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

uint32_t ts_audition_overload_generation(const ts_audition_mixer *m) {
  if (m == NULL)
    return 0;
  return atomic_load_explicit(&m->overload_generation, memory_order_acquire);
}

double ts_audition_cursor_position(const ts_audition_mixer *m) {
  if (m == NULL || atomic_load_explicit(&m->cursor_age, memory_order_acquire) == 0)
    return -1.0;
  const uint64_t frames = atomic_load_explicit(&m->cursor_frames, memory_order_relaxed);
  uint64_t frame = atomic_load_explicit(&m->cursor_frame, memory_order_relaxed);
  if (frames <= 1) return frames == 1 ? 0.0 : -1.0;
  if (frame >= frames) frame = frames - 1;
  return (double)frame / (double)(frames - 1);
}
