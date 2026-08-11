# TapeSister recipe schema 1

TapeSister recipes use UTF-8 JSON, LF line endings, a final newline, and the
canonical field order emitted by `ts_recipe_format()`. Schema 1 is a closed
schema: all 35 fields are required, duplicate and unknown fields are errors,
and unsupported schema or renderer versions are not migrated.

The identity fields are `schema = "tapesister.recipe"`, `schema_version = 1`,
and `renderer_version = 1`. The seed is exactly 16 lowercase hexadecimal
digits. Enum strings are lowercase and booleans are JSON booleans.

## Integer units

- `sample_rate`, `requested_frames`, and `root_midi_note` are integral counts.
- `fine_tune_cent100` and `pitch_env_cent100` use hundredths of a cent.
- `attack_us`, `decay_us`, `release_us`, `pitch_env_us`, and `delay_us` use
  microseconds.
- `filter_cutoff_millihz` uses millihertz.
- Fields ending in `_ppm` use parts per million; 1,000,000 represents 1.0.
- `filter_env_octave_cent100` uses hundredths of an octave.
- `fixed_gain_centidb` uses hundredths of a decibel.

Root frequency is not serialized. Renderer 1 derives it from MIDI note and
fine tune with equal temperament relative to A4 = 440 Hz, then quantizes to
one hundredth hertz to preserve the approved Phase 1A corpus. Requested frame
count, rather than a second duration field, controls output length.

`target_peak` scales content to the explicit `target_peak_ppm` value.
`fixed_headroom` applies only `fixed_gain_centidb`; it fails if the resulting
buffer exceeds full scale and never normalizes, limits, or clips it.

PCM16 conversion rejects non-finite values, safeguards inputs to [-1, 1],
multiplies by 32768, rounds halves away from zero, saturates to the signed
16-bit range, and emits little-endian mono samples without dither.
