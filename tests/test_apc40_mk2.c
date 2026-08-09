#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL_atomic.h>
#include "ft2_apc40_mk2.h"
#include "ft2_midi_map.h"

void SDLCALL SDL_AtomicLock(SDL_SpinLock *lock)
{
	while (__sync_lock_test_and_set(lock, 1)) { }
}

void SDLCALL SDL_AtomicUnlock(SDL_SpinLock *lock)
{
	__sync_lock_release(lock);
}

static void testIntroduction(void)
{
	uint8_t message[12];
	static const uint8_t expected[12] =
	{
		0xF0, 0x47, 0x7F, 0x29, 0x60, 0x00, 0x04,
		0x42, 0x00, 0x01, 0x00, 0xF7
	};

	assert(tapeheadAPC40Mk2BuildIntroduction(0x42, message,
		sizeof (message)) == sizeof (message));
	assert(!memcmp(message, expected, sizeof (expected)));
	assert(tapeheadAPC40Mk2BuildIntroduction(0x43, message,
		sizeof (message)) == 0);
	assert(tapeheadAPC40Mk2BuildIntroduction(0x42, message, 11) == 0);
}

static void testRatioRing(void)
{
	assert(tapeheadAPC40Mk2RatioRingValue(0, 17) == 0);
	assert(tapeheadAPC40Mk2RatioRingValue(7, 17) == 56); /* 1:1 */
	assert(tapeheadAPC40Mk2RatioRingValue(16, 17) == 127);
	assert(tapeheadAPC40Mk2RatioRingValue(99, 17) == 127);

	uint8_t previous = 0;
	for (uint8_t i = 1; i < 17; i++)
	{
		const uint8_t value = tapeheadAPC40Mk2RatioRingValue(i, 17);
		assert(value > previous); /* every musical ratio has unique feedback */
		previous = value;
	}
}

static void testRGBTransitionClearsObsoleteAnimation(void)
{
	uint8_t messages[12];
	assert(tapeheadAPC40Mk2BuildRGBTransition(17, 21, 37, 13,
		messages, sizeof (messages)) == 12);
	const uint8_t expectedAnimated[12] =
	{
		0x98, 17, 0, /* clear pulse */
		0x9D, 17, 0, /* clear blink */
		0x90, 17, 21,
		0x9D, 17, 37
	};
	assert(!memcmp(messages, expectedAnimated, sizeof (expectedAnimated)));

	assert(tapeheadAPC40Mk2BuildRGBTransition(17, 20, 0, 0,
		messages, sizeof (messages)) == 9);
	const uint8_t expectedSteady[9] =
	{
		0x98, 17, 0,
		0x9D, 17, 0,
		0x90, 17, 20
	};
	assert(!memcmp(messages, expectedSteady, sizeof (expectedSteady)));
	assert(tapeheadAPC40Mk2BuildRGBTransition(17, 20, 0, 0,
		messages, 8) == 0);
}

static void testBuiltInMappings(void)
{
	tapeheadMidiMapReset();
	tapeheadMidiMapSetEnabled(true);

	/* User mappings are loaded first and profile defaults must not replace
	** the same physical input. */
	assert(tapeheadMidiMapAddBinding("NoteOn.1.0", "TransportStopAll"));
	tapeheadAPC40Mk2InstallDefaultMappings();
	assert(tapeheadMidiMapGetBindingCount() == 148);

	/* Matrix, sequence, per-track button/fader, ratio knob, jog and transport
	** defaults all resolve through the Phase 4.3 map. */
	assert(tapeheadMidiMapHandleMessage(0x90, 31, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 32, 127)); /* column */
	assert(tapeheadMidiMapHandleMessage(0x90, 82, 127)); /* whole bank */
	assert(tapeheadMidiMapHandleMessage(0x90, 86, 127)); /* row 4 */
	assert(tapeheadMidiMapHandleMessage(0x90, 102, 127)); /* Q/Poly */
	assert(tapeheadMidiMapHandleMessage(0x90, 0x32, 127)); /* bank 1 */
	assert(tapeheadMidiMapHandleMessage(0x97, 0x30, 127));
	assert(tapeheadMidiMapHandleMessage(0xB7, 0x07, 64));
	assert(tapeheadMidiMapHandleMessage(0xB0, 0x37, 127));
	assert(tapeheadMidiMapHandleMessage(0xB0, 0x0F, 64));
	assert(tapeheadMidiMapHandleMessage(0xB0, 0x2F, 127));
	assert(tapeheadMidiMapHandleMessage(0xB0, 0x40, 127));
	assert(tapeheadMidiMapHandleMessage(0xB0, 0x40, 0));
	assert(tapeheadMidiMapHandleMessage(0x90, 0x5B, 127));
	assert(!tapeheadMidiMapHandleMessage(0x90, 0x5C, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 0x5D, 127));
	assert(tapeheadMidiMapGetPendingCount() == 15);

	/* Releases are consumed without adding a second toggle action. */
	assert(tapeheadMidiMapHandleMessage(0x80, 31, 127));
	assert(tapeheadMidiMapGetPendingCount() == 15);

	/* Phase 4.3 maps Tap Tempo to global FastTracks Pattern/Song mode. */
	assert(tapeheadMidiMapHandleMessage(0x90, 0x63, 127));
	assert(tapeheadMidiMapGetPendingCount() == 16);

	/* Human-readable control names resolve to their documented raw messages. */
	tapeheadMidiMapReset();
	assert(tapeheadAPC40Mk2AddNamedMapping("GridSlot01",
		"MatrixSlotTrigger:1"));
	assert(tapeheadAPC40Mk2AddNamedMapping("MasterFader", "MasterVolume"));
	assert(tapeheadMidiMapGetBindingCount() == 2);
}

int main(void)
{
	testIntroduction();
	testRatioRing();
	testRGBTransitionClearsObsoleteAnimation();
	testBuiltInMappings();
	puts("APC40 mkII profile tests passed.");
	return 0;
}
