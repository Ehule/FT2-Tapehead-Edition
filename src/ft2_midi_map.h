#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TAPEHEAD_MIDI_MAP_MAX_BINDINGS 256
#define TAPEHEAD_MIDI_EVENT_QUEUE_CAPACITY 256

/*
** Generic, device-independent MIDI performance mapping.
**
** Configuration is written with one-based MIDI channels and tracker tracks:
**   NoteOn.1.48=TrackPerformanceMuteToggle:1
**   CC.1.7=TrackTrim:1
**   CC.1.48=FastTrackRatio:1
** Matrix bank and slot arguments are also one-based in the INI file.
**
** Incoming RtMidi messages are matched on its callback thread, but mapped
** actions are only executed later by tapeheadMidiMapProcessPending() on the
** main thread.
*/
void tapeheadMidiMapReset(void);
void tapeheadMidiMapSetEnabled(bool enabled);
bool tapeheadMidiMapIsEnabled(void);
bool tapeheadMidiMapAddBinding(const char *input, const char *action);
bool tapeheadMidiMapDisableBinding(const char *input);

/* Profile defaults fill only inputs that were not explicitly mapped in the
** INI file, so a shared profile remains completely user-overridable. */
bool tapeheadMidiMapAddDefaultBinding(const char *input, const char *action);

/* Returns true when the MIDI message belongs to the performance map. */
bool tapeheadMidiMapHandleMessage(uint8_t status, uint8_t data1, uint8_t data2);

/* Record an authoritative absolute-controller value emitted as surface
** feedback. A matching input echo (or an unchanged hardware report) is
** consumed without being dispatched as a fresh movement. */
void tapeheadMidiMapSetFeedbackValue(uint8_t midiChannel, uint8_t controller,
	uint8_t value);

/* Must be called from the main/UI thread. */
void tapeheadMidiMapProcessPending(void);

/* Read-only diagnostics, also used by the native injected-event tests. */
size_t tapeheadMidiMapGetBindingCount(void);
size_t tapeheadMidiMapGetPendingCount(void);
uint32_t tapeheadMidiMapGetDroppedEventCount(void);
