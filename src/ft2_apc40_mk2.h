#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Built-in Akai APC40 mkII profile. The generic MIDI map remains available;
** profile bindings only fill controls that the INI file did not override. */
void tapeheadAPC40Mk2InstallDefaultMappings(void);
bool tapeheadAPC40Mk2AddNamedMapping(const char *controlName,
	const char *action);
void tapeheadAPC40Mk2Open(void);
void tapeheadAPC40Mk2Close(void);
void tapeheadAPC40Mk2Refresh(void);
bool tapeheadAPC40Mk2IsActive(void);

/* Pure protocol helpers used by focused native tests. */
size_t tapeheadAPC40Mk2BuildIntroduction(uint8_t mode, uint8_t *message,
	size_t capacity);
uint8_t tapeheadAPC40Mk2RatioRingValue(uint8_t ratioIndex,
	uint8_t ratioCount);
uint8_t tapeheadAPC40Mk2ScaleRGBColor(uint8_t color, uint8_t brightness);
size_t tapeheadAPC40Mk2BuildRGBTransition(uint8_t note, uint8_t primary,
	uint8_t secondary, uint8_t animation, uint8_t *messages, size_t capacity);
