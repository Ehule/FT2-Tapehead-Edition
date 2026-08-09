#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
** Dedicated MIDI ports for a performance control surface.
**
** These ports are intentionally separate from FT2's musical MIDI input and
** Tapehead's MIDI Dub output. Incoming surface messages can therefore never
** become tracker notes, and selecting a surface never disconnects the user's
** keyboard.
*/
bool tapeheadMidiSurfaceOpen(const char *inputDeviceName,
	const char *outputDeviceName);
void tapeheadMidiSurfaceClose(void);

bool tapeheadMidiSurfaceInputIsOpen(void);
bool tapeheadMidiSurfaceOutputIsOpen(void);
bool tapeheadMidiSurfaceSend(const uint8_t *message, size_t messageSize);

/* Pure name-selection helper used by the native regression test. */
int32_t tapeheadMidiSurfaceChoosePort(const char *requestedName,
	const char *const *availableNames, uint32_t availableCount);
