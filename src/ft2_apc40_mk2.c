#ifdef HAS_MIDI

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "ft2_apc40_mk2.h"
#include "ft2_config.h"
#include "ft2_fasttracks.h"
#include "ft2_header.h"
#include "ft2_midi_map.h"
#include "ft2_midi_surface.h"
#include "ft2_pattern_launcher.h"
#include "ft2_pattern_launcher_ui.h"
#include "ft2_poly_matrix.h"
#include "ft2_sample_launcher.h"
#include "ft2_sample_morph.h"
#include "ft2_structs.h"
#include "ft2_tapehead_actions.h"

enum
{
	APC_MODE_GENERIC = 0x40,
	APC_MODE_ALTERNATE_ABLETON = 0x42,
	APC_RGB_PRIMARY = 0,
	APC_RGB_PULSE_EIGHTH = 8,
	APC_RGB_BLINK_EIGHTH = 13,
	APC_COLOR_OFF = 0,
	APC_COLOR_DIM_GREY = 1,
	APC_COLOR_WHITE = 3,
	APC_COLOR_RED = 5,
	APC_COLOR_ORANGE = 9,
	APC_COLOR_DIM_ORANGE = 11,
	APC_COLOR_YELLOW = 13,
	APC_COLOR_DIM_YELLOW = 14,
	APC_COLOR_DARK_YELLOW = 15,
	APC_COLOR_DIM_GREEN = 20,
	APC_COLOR_GREEN = 21,
	APC_COLOR_TEAL = 33,
	APC_COLOR_CYAN = 37,
	APC_COLOR_DARK_CYAN = 38,
	APC_COLOR_BLUE = 41,
	APC_COLOR_DIM_BLUE = 42,
	APC_COLOR_DIM_PURPLE = 48,
	APC_COLOR_PURPLE = 49
};

uint8_t tapeheadAPC40Mk2ScaleRGBColor(uint8_t color, uint8_t brightness)
{
	/* Mode 2 exposes palette entries rather than continuous RGB intensity.
	** Each row keeps one logical hue ordered from darkest to brightest. */
	static const uint8_t shades[][3] =
	{
		{ 7, 6, APC_COLOR_RED },
		{ 11, 10, APC_COLOR_ORANGE },
		{ 11, 11, APC_COLOR_DIM_ORANGE },
		{ APC_COLOR_DARK_YELLOW, APC_COLOR_DIM_YELLOW, APC_COLOR_YELLOW },
		{ APC_COLOR_DARK_YELLOW, APC_COLOR_DIM_YELLOW, APC_COLOR_DIM_YELLOW },
		{ APC_COLOR_DARK_YELLOW, APC_COLOR_DARK_YELLOW, APC_COLOR_DARK_YELLOW },
		{ 19, APC_COLOR_DIM_GREEN, APC_COLOR_GREEN },
		{ 19, APC_COLOR_DIM_GREEN, APC_COLOR_DIM_GREEN },
		{ 35, 34, APC_COLOR_TEAL },
		{ 39, APC_COLOR_DARK_CYAN, APC_COLOR_CYAN },
		{ 39, APC_COLOR_DARK_CYAN, APC_COLOR_DARK_CYAN },
		{ 43, APC_COLOR_DIM_BLUE, APC_COLOR_BLUE },
		{ 43, APC_COLOR_DIM_BLUE, APC_COLOR_DIM_BLUE },
		{ 47, APC_COLOR_DIM_PURPLE, APC_COLOR_PURPLE },
		{ 47, APC_COLOR_DIM_PURPLE, APC_COLOR_DIM_PURPLE }
	};
	static const uint8_t colors[] =
	{
		APC_COLOR_RED, APC_COLOR_ORANGE, APC_COLOR_DIM_ORANGE,
		APC_COLOR_YELLOW, APC_COLOR_DIM_YELLOW, APC_COLOR_DARK_YELLOW,
		APC_COLOR_GREEN, APC_COLOR_DIM_GREEN, APC_COLOR_TEAL, APC_COLOR_CYAN,
		APC_COLOR_DARK_CYAN, APC_COLOR_BLUE, APC_COLOR_DIM_BLUE,
		APC_COLOR_PURPLE, APC_COLOR_DIM_PURPLE
	};

	if (color == APC_COLOR_OFF || brightness == 0) return APC_COLOR_OFF;
	if (brightness >= 100) return color;
	const uint8_t level = brightness <= 33 ? 0 : brightness <= 66 ? 1 : 2;
	for (size_t i = 0; i < sizeof (colors) / sizeof (colors[0]); i++)
		if (color == colors[i]) return shades[i][level];
	return color;
}

typedef struct apcControl_t
{
	const char *name;
	const char *input;
	const char *action;
} apcControl_t;

static const apcControl_t fixedControls[] =
{
	{ "DeviceLeft", "NoteOn.1.58", "SampleMorphAllPrevious" },
	{ "DeviceRight", "NoteOn.1.59", "SampleMorphAllNext" },
	{ "BankLeft", "NoteOn.1.60", "FastTrackRatioAllOrMatrixBankPrevious" },
	{ "BankRight", "NoteOn.1.61", "FastTrackRatioAllOrMatrixBankNext" },
	{ "DeviceOnOff", "NoteOn.1.62", "SampleMorphArmToggle" },
	{ "DeviceLock", "NoteOn.1.63", "FastTrackResetAll" },
	{ "ClipDeviceView", "NoteOn.1.64", "FastTrackTransmissionClutchToggle" },
	{ "DetailView", "NoteOn.1.65", "FastTrackMasterToggle" },
	{ "Master", "NoteOn.1.80", "PerformanceMuteMaster" },
	{ "StopAllClips", "NoteOn.1.81", "TransportStopDeck" },
	{ "SceneLaunch1", "NoteOn.1.82", "MatrixSequenceBank" },
	{ "SceneLaunch2", "NoteOn.1.83", "MatrixSequenceRow:1" },
	{ "SceneLaunch3", "NoteOn.1.84", "MatrixSequenceRow:2" },
	{ "SceneLaunch4", "NoteOn.1.85", "MatrixSequenceRow:3" },
	{ "SceneLaunch5", "NoteOn.1.86", "MatrixSequenceRow:4" },
	{ "Pan", "NoteOn.1.87", "MatrixModePattern" },
	{ "Sends", "NoteOn.1.88", "MatrixModeSample" },
	{ "User", "NoteOn.1.89", "MatrixVisibilityToggle" },
	{ "Metronome", "NoteOn.1.90", "FastTrackGlobalReverseToggle" },
	{ "Play", "NoteOn.1.91", "TransportPlaySongToggle" },
	{ "Record", "NoteOn.1.93", "TransportPlayPatternToggle" },
	{ "Up", "NoteOn.1.94", "SongOrderPrevious" },
	{ "Down", "NoteOn.1.95", "SongOrderNext" },
	{ "Right", "NoteOn.1.96", "CursorRight" },
	{ "Left", "NoteOn.1.97", "CursorLeft" },
	{ "Shift", "NoteOn.1.98", "ShiftModifier" },
	{ "TapTempo", "NoteOn.1.99", "FastTrackGlobalModeToggle" },
	{ "NudgeMinus", "NoteOn.1.100", "SpeedDown" },
	{ "NudgePlus", "NoteOn.1.101", "SpeedUp" },
	{ "Session", "NoteOn.1.102", "MatrixGridModeToggle" },
	{ "Bank", "NoteOn.1.103", "TransportStopSelectedDeck" },
	{ "TempoEncoder", "CC.1.13", "TempoRelative" },
	{ "MasterFader", "CC.1.14", "MasterVolume" },
	{ "Crossfader", "CC.1.15", "PatternJogAbsolute" },
	{ "CueLevel", "CC.1.47", "PatternJogRelative" },
	{ "Footswitch", "CC.1.64", "TransportPunch" }
};

static bool profileActive, feedbackActive;
static uint8_t noteCache[16][128], ccCache[16][128];

typedef struct apcRGBState_t
{
	bool valid;
	uint8_t primary, secondary, animation;
} apcRGBState_t;

static apcRGBState_t rgbState[128];

static bool textEqualsIgnoreCase(const char *a, const char *b)
{
	while (*a != '\0' && *b != '\0')
	{
		if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++))
			return false;
	}
	return *a == '\0' && *b == '\0';
}

static bool mapBinding(const char *input, const char *action, bool isDefault)
{
	return isDefault ? tapeheadMidiMapAddDefaultBinding(input, action) :
		tapeheadMidiMapAddBinding(input, action);
}

static bool matchNumberedName(const char *key, const char *prefix,
	int32_t count, int32_t *index)
{
	char name[32];
	for (int32_t i = 0; i < count; i++)
	{
		snprintf(name, sizeof (name), "%s%02d", prefix, i + 1);
		if (textEqualsIgnoreCase(key, name))
		{
			*index = i;
			return true;
		}
	}
	return false;
}

static bool resolveNamedInput(const char *key, char input[32])
{
	for (size_t i = 0; i < sizeof (fixedControls) / sizeof (fixedControls[0]); i++)
	{
		if (textEqualsIgnoreCase(key, fixedControls[i].name))
		{
			snprintf(input, 32, "%s", fixedControls[i].input);
			return true;
		}
	}

	int32_t index;
	if (matchNumberedName(key, "GridTopPad", 8, &index))
		snprintf(input, 32, "NoteOn.1.%d", 32 + index);
	else if (matchNumberedName(key, "GridSlot", 32, &index))
	{
		const int32_t row = index / 8;
		const int32_t column = index & 7;
		snprintf(input, 32, "NoteOn.1.%d", ((3 - row) * 8) + column);
	}
	else if (matchNumberedName(key, "RecordArm", 8, &index))
		snprintf(input, 32, "NoteOn.%d.48", index + 1);
	else if (matchNumberedName(key, "Solo", 8, &index))
		snprintf(input, 32, "NoteOn.%d.49", index + 1);
	else if (matchNumberedName(key, "Activator", 8, &index))
		snprintf(input, 32, "NoteOn.%d.50", index + 1);
	else if (matchNumberedName(key, "TrackSelect", 8, &index))
		snprintf(input, 32, "NoteOn.%d.51", index + 1);
	else if (matchNumberedName(key, "ClipStop", 8, &index))
		snprintf(input, 32, "NoteOn.%d.52", index + 1);
	else if (matchNumberedName(key, "CrossfaderAB", 8, &index))
		snprintf(input, 32, "NoteOn.%d.66", index + 1);
	else if (matchNumberedName(key, "TrackFader", 8, &index))
		snprintf(input, 32, "CC.%d.7", index + 1);
	else if (matchNumberedName(key, "TrackControl", 8, &index))
		snprintf(input, 32, "CC.1.%d", 48 + index);
	else if (matchNumberedName(key, "DeviceKnob", 8, &index))
		snprintf(input, 32, "CC.1.%d", 16 + index);
	else
		return false;
	return true;
}

bool tapeheadAPC40Mk2AddNamedMapping(const char *controlName,
	const char *action)
{
	char input[32];
	return controlName != NULL && action != NULL &&
		resolveNamedInput(controlName, input) &&
		mapBinding(input, action, false);
}

void tapeheadAPC40Mk2InstallDefaultMappings(void)
{
	char key[32], input[32], action[48];

	for (int32_t i = 0; i < 32; i++)
	{
		snprintf(key, sizeof (key), "GridSlot%02d", i + 1);
		resolveNamedInput(key, input);
		snprintf(action, sizeof (action), "MatrixSlotTrigger:%d", i + 1);
		mapBinding(input, action, true);
	}
	for (int32_t i = 0; i < 8; i++)
	{
		snprintf(key, sizeof (key), "GridTopPad%02d", i + 1);
		resolveNamedInput(key, input);
		snprintf(action, sizeof (action), "MatrixSequenceColumn:%d", i + 1);
		mapBinding(input, action, true);

		const struct { const char *name, *format; } trackControls[] =
		{
			{ "RecordArm", "FastTrackClutchToggle:%d" },
			{ "Solo", "TrackPerformanceSoloToggle:%d" },
			{ "Activator", "MatrixLayerBankSelect:%d" },
			{ "TrackSelect", "FastTrackDirectionOrSongMode:%d" },
			{ "ClipStop", "TrackPerformanceMuteToggle:%d" },
			{ "CrossfaderAB", "FastTrackToggle:%d" },
			{ "TrackFader", "TrackTrim:%d" },
			{ "TrackControl", "FastTrackRatio:%d" },
			{ "DeviceKnob", "SampleMorphSelect:%d" }
		};
		for (size_t j = 0; j < sizeof (trackControls) / sizeof (trackControls[0]); j++)
		{
			snprintf(key, sizeof (key), "%s%02d", trackControls[j].name, i + 1);
			resolveNamedInput(key, input);
			if (strstr(trackControls[j].format, "%d") != NULL)
				snprintf(action, sizeof (action), trackControls[j].format, i + 1);
			else
				snprintf(action, sizeof (action), "%s", trackControls[j].format);
			mapBinding(input, action, true);
		}
	}

	for (size_t i = 0; i < sizeof (fixedControls) / sizeof (fixedControls[0]); i++)
		mapBinding(fixedControls[i].input, fixedControls[i].action, true);
}

size_t tapeheadAPC40Mk2BuildIntroduction(uint8_t mode, uint8_t *message,
	size_t capacity)
{
	if (message == NULL || capacity < 12 ||
		(mode != APC_MODE_GENERIC && mode != 0x41 &&
		 mode != APC_MODE_ALTERNATE_ABLETON))
	{
		return 0;
	}

	const uint8_t introduction[12] =
	{
		0xF0, 0x47, 0x7F, 0x29, 0x60, 0x00, 0x04,
		mode, 0x00, 0x01, 0x00, 0xF7
	};
	memcpy(message, introduction, sizeof (introduction));
	return sizeof (introduction);
}

uint8_t tapeheadAPC40Mk2RatioRingValue(uint8_t ratioIndex,
	uint8_t ratioCount)
{
	if (ratioCount <= 1) return 0;
	if (ratioIndex >= ratioCount) ratioIndex = ratioCount - 1;
	return (uint8_t)(((uint32_t)ratioIndex * 127 +
		((ratioCount - 1) / 2)) / (ratioCount - 1));
}

size_t tapeheadAPC40Mk2BuildRGBTransition(uint8_t note, uint8_t primary,
	uint8_t secondary, uint8_t animation, uint8_t *messages, size_t capacity)
{
	const bool hasAnimation = animation != APC_RGB_PRIMARY &&
		secondary != APC_COLOR_OFF;
	const size_t length = hasAnimation ? 12 : 9;
	if (messages == NULL || capacity < length || note >= 128 ||
		animation >= 16)
	{
		return 0;
	}

	const uint8_t transition[12] =
	{
		(uint8_t)(0x90 | APC_RGB_PULSE_EIGHTH), note, APC_COLOR_OFF,
		(uint8_t)(0x90 | APC_RGB_BLINK_EIGHTH), note, APC_COLOR_OFF,
		(uint8_t)(0x90 | APC_RGB_PRIMARY), note, primary,
		(uint8_t)(0x90 | animation), note, secondary
	};
	memcpy(messages, transition, length);
	return length;
}

static void resetCaches(void)
{
	memset(noteCache, 0xFF, sizeof (noteCache));
	memset(ccCache, 0xFF, sizeof (ccCache));
	memset(rgbState, 0, sizeof (rgbState));
}

static bool sendMessage(const uint8_t message[3])
{
	if (!feedbackActive || !tapeheadMidiSurfaceSend(message, 3))
	{
		feedbackActive = false;
		return false;
	}
	return true;
}

static void sendNote(uint8_t midiChannel, uint8_t note, uint8_t value)
{
	if (midiChannel >= 16 || note >= 128 ||
		noteCache[midiChannel][note] == value) return;
	const uint8_t message[3] = { (uint8_t)(0x90 | midiChannel), note, value };
	if (sendMessage(message)) noteCache[midiChannel][note] = value;
}

static void sendRGBState(uint8_t note, uint8_t primary, uint8_t secondary,
	uint8_t animation)
{
	if (note >= 128)
		return;
	primary = tapeheadAPC40Mk2ScaleRGBColor(primary,
		tapeheadConfig.apc40RGBBrightness);
	secondary = tapeheadAPC40Mk2ScaleRGBColor(secondary,
		tapeheadConfig.apc40RGBBrightness);
	apcRGBState_t *state = &rgbState[note];
	if (state->valid && state->primary == primary &&
		state->secondary == secondary && state->animation == animation)
	{
		return;
	}

	/* Pulse and blink are separate MIDI channels addressing the same physical
	** pad. Neutralize both before installing the new authoritative state, or
	** the APC autonomously keeps replaying obsolete host animations. */
	uint8_t transition[12];
	const size_t length = tapeheadAPC40Mk2BuildRGBTransition(note, primary,
		secondary, animation, transition, sizeof (transition));
	if (length == 0) return;
	for (size_t offset = 0; offset < length; offset += 3)
		if (!sendMessage(&transition[offset])) return;

	state->valid = true;
	state->primary = primary;
	state->secondary = secondary;
	state->animation = animation;
}

static void sendRGB(uint8_t note, uint8_t color)
{
	sendRGBState(note, color, APC_COLOR_OFF, APC_RGB_PRIMARY);
}

static void sendRGBAnimated(uint8_t note, uint8_t color, uint8_t animation)
{
	if (animation == APC_RGB_PRIMARY)
	{
		sendRGB(note, color);
		return;
	}
	sendRGBState(note, APC_COLOR_OFF, color, animation);
}

static void sendRGBPair(uint8_t note, uint8_t primary, uint8_t secondary,
	uint8_t animation)
{
	sendRGBState(note, primary, secondary, animation);
}

static void sendCC(uint8_t midiChannel, uint8_t controller, uint8_t value)
{
	if (midiChannel >= 16 || controller >= 128 ||
		ccCache[midiChannel][controller] == value) return;
	const uint8_t message[3] = { (uint8_t)(0xB0 | midiChannel), controller, value };
	if (sendMessage(message)) ccCache[midiChannel][controller] = value;
}

static int8_t patternQueuePos(uint8_t patternNum)
{
	const uint8_t count = patternLauncherGetQueueCount();
	for (uint8_t i = 0; i < count; i++)
		if (patternLauncherGetQueueItem(i) == patternNum) return (int8_t)i;
	return -1;
}

static uint8_t queueColor(int8_t position)
{
	static const uint8_t colors[4] =
	{
		APC_COLOR_YELLOW, 12, APC_COLOR_DIM_YELLOW, APC_COLOR_DARK_YELLOW
	};
	if (position < 0) return APC_COLOR_YELLOW;
	if (position > 3) position = 3;
	return colors[position];
}

static uint8_t gridNote(uint8_t localSlot)
{
	return (uint8_t)(((3 - (localSlot / 8)) * 8) + (localSlot & 7));
}

static uint8_t dimQColor(uint8_t color)
{
	if (color == APC_COLOR_GREEN) return APC_COLOR_DIM_GREEN;
	if (color == APC_COLOR_YELLOW) return APC_COLOR_DIM_YELLOW;
	if (color == APC_COLOR_RED || color == APC_COLOR_ORANGE)
		return APC_COLOR_DIM_ORANGE;
	return color;
}

static void resolveGridState(uint8_t note, bool loaded, bool hasQ,
	uint8_t qColor, uint8_t qAnimation, bool hasPoly, uint8_t polyColor,
	uint8_t polyAnimation)
{
	const bool polyLayer = tapeheadActionMatrixGridIsPoly();
	if (hasQ && hasPoly)
	{
		if (polyLayer)
			sendRGBPair(note, polyColor, dimQColor(qColor),
				qAnimation == APC_RGB_PRIMARY ? APC_RGB_BLINK_EIGHTH : qAnimation);
		else
			sendRGBPair(note, qColor,
				polyColor == APC_COLOR_CYAN ? APC_COLOR_TEAL : APC_COLOR_DARK_CYAN,
				polyAnimation == APC_RGB_PRIMARY ? APC_RGB_BLINK_EIGHTH : polyAnimation);
	}
	else if (hasQ)
	{
		sendRGBAnimated(note, polyLayer ? dimQColor(qColor) : qColor,
			qAnimation);
	}
	else if (hasPoly)
	{
		const uint8_t color = polyLayer ? polyColor :
			(polyColor == APC_COLOR_CYAN ? APC_COLOR_TEAL : APC_COLOR_DARK_CYAN);
		sendRGBAnimated(note, color, polyAnimation);
	}
	else
	{
		sendRGB(note, !loaded ? APC_COLOR_OFF :
			(polyLayer ? APC_COLOR_DIM_BLUE : APC_COLOR_DIM_GREEN));
	}
}

static void refreshPatternSlot(uint8_t localSlot)
{
	const uint8_t note = gridNote(localSlot);
	const uint8_t patternNum =
		(uint8_t)((patternLauncherGetPage() * 32) + localSlot);
	const int8_t queuePos = patternQueuePos(patternNum);
	const bool polyActive = polyMatrixIsPatternActive(patternNum);
	const bool polyStart = patternLauncherPolyHandoffIsPending(patternNum) ||
		polyMatrixPatternQHandoffPending(patternNum);
	const bool polyStop = polyMatrixPatternStopPending(patternNum);
	const bool hasPoly = polyActive || polyStart || polyStop;
	uint8_t polyColor = polyStop ? APC_COLOR_DARK_CYAN :
		(polyStart ? APC_COLOR_TEAL : APC_COLOR_CYAN);
	const uint8_t polyAnimation = polyStop ? APC_RGB_BLINK_EIGHTH :
		(polyStart ? APC_RGB_PULSE_EIGHTH : APC_RGB_PRIMARY);

	const bool loaded = patternLauncherPatternIsExposed(patternNum);
	bool hasQ = tapeheadActionMatrixSequenceSlotIsPending(localSlot);
	uint8_t qColor = hasQ ? APC_COLOR_YELLOW : APC_COLOR_GREEN;
	uint8_t qAnimation = APC_RGB_PRIMARY;
	if (patternLauncherGetCurrent() == patternNum)
	{
		hasQ = true;
		const uint8_t exitMode = patternLauncherGetExitMode();
		if (exitMode == PATTERN_LAUNCHER_EXIT_RETURN) qColor = APC_COLOR_YELLOW;
		else if (exitMode == PATTERN_LAUNCHER_EXIT_STOP) qColor = APC_COLOR_RED;
		else if (exitMode == PATTERN_LAUNCHER_EXIT_CONTINUE) qColor = APC_COLOR_ORANGE;
		else qColor = APC_COLOR_GREEN;
		qAnimation = exitMode == PATTERN_LAUNCHER_EXIT_STOP ?
			APC_RGB_BLINK_EIGHTH : APC_RGB_PULSE_EIGHTH;
	}
	else if (queuePos >= 0)
	{
		hasQ = true;
		qColor = queueColor(queuePos);
		qAnimation = APC_RGB_BLINK_EIGHTH;
	}
	else if (hasQ)
	{
		qAnimation = APC_RGB_BLINK_EIGHTH;
	}

	resolveGridState(note, loaded, hasQ, qColor, qAnimation, hasPoly,
		polyColor, polyAnimation);
}

static void refreshSampleSlot(uint8_t localSlot)
{
	const uint8_t note = gridNote(localSlot);
	const uint16_t tile = (sampleLauncherGetBank() * 32) + localSlot;
	const int8_t queuePos = sampleLauncherGetQQueuePos(tile);
	const bool polyActive = sampleLauncherGetPolySlot(tile) >= 0;
	const bool polyStart = sampleLauncherPolyStartPending(tile);
	const bool polyStop = sampleLauncherPolyStopPending(tile);
	const bool hasPoly = polyActive || polyStart || polyStop;
	const uint8_t polyColor = polyStop ? APC_COLOR_DARK_CYAN :
		(polyStart ? APC_COLOR_TEAL : APC_COLOR_CYAN);
	const uint8_t polyAnimation = polyStop ? APC_RGB_BLINK_EIGHTH :
		(polyStart ? APC_RGB_PULSE_EIGHTH : APC_RGB_PRIMARY);

	const bool loaded = sampleLauncherTileIsLoaded(tile);
	bool hasQ = tapeheadActionMatrixSequenceSlotIsPending(localSlot);
	uint8_t qColor = hasQ ? APC_COLOR_YELLOW : APC_COLOR_GREEN;
	uint8_t qAnimation = APC_RGB_PRIMARY;
	if (sampleLauncherGetQCurrent() == tile)
	{
		hasQ = true;
		qColor = sampleLauncherQStopPending() ? APC_COLOR_RED : APC_COLOR_GREEN;
		qAnimation = sampleLauncherQStopPending() ? APC_RGB_BLINK_EIGHTH :
			APC_RGB_PULSE_EIGHTH;
	}
	else if (queuePos >= 0)
	{
		hasQ = true;
		qColor = queueColor(queuePos);
		qAnimation = APC_RGB_BLINK_EIGHTH;
	}
	else if (hasQ)
	{
		qAnimation = APC_RGB_BLINK_EIGHTH;
	}

	resolveGridState(note, loaded, hasQ, qColor, qAnimation, hasPoly,
		polyColor, polyAnimation);
}

static bool selectedDeckHasWork(void)
{
	if (tapeheadActionMatrixGetTarget() == TAPEHEAD_MATRIX_SAMPLE)
	{
		return tapeheadActionMatrixGridIsPoly() ?
			sampleLauncherHasPolyWork() : sampleLauncherHasQWork();
	}
	return tapeheadActionMatrixGridIsPoly() ? polyMatrixHasAudioWork() :
		patternLauncherIsEnabled();
}

static bool allSelectedFastTracksReversed(void)
{
	bool any = false;
	for (int32_t i = 0; i < song.numChannels && i < 8; i++)
	{
		if (!fastTracksPOCIsSelected(i)) continue;
		any = true;
		if (!fastTracksPOCIsReversed(i)) return false;
	}
	return any;
}

static void refreshTracks(void)
{
	const uint8_t ratioCount = fastTracksPOCGetRatioCount();
	const bool shiftHeld = tapeheadActionShiftModifierIsHeld();
	const uint8_t visibleBank = tapeheadActionMatrixGetBank(shiftHeld
		? TAPEHEAD_MATRIX_SAMPLE : TAPEHEAD_MATRIX_PATTERN);
	for (int32_t i = 0; i < 8; i++)
	{
		const bool active = i < song.numChannels;
		sendNote((uint8_t)i, 0x30,
			active && fastTracksPOCIsSelected(i) && fastTracksPOCIsClutched(i));
		sendNote((uint8_t)i, 0x31,
			active && tapeheadActionGetPerformanceSoloTrack() == i);
		sendNote((uint8_t)i, 0x32, visibleBank == i);
		const bool trackSelectState = active && fastTracksPOCIsSelected(i) &&
			(shiftHeld ? fastTracksPOCGetMode(i) == FAST_TRACKS_MODE_SONG :
			fastTracksPOCIsReversed(i));
		sendNote((uint8_t)i, 0x33, trackSelectState);
		sendNote((uint8_t)i, 0x34, active && performanceMute[i]);

		uint8_t fastTrackState = 0;
		if (active && fastTracksPOCIsSelected(i))
			fastTrackState = fastTracksPOCMasterIsEnabled() ? 2 : 1;
		sendNote((uint8_t)i, 0x42, fastTrackState);

		sendCC(0, (uint8_t)(0x38 + i), active ? 1 : 0);
		if (active)
			sendCC(0, (uint8_t)(0x30 + i), tapeheadAPC40Mk2RatioRingValue(
				fastTracksPOCGetRatioIndex(i), ratioCount));

		const bool morphVisible = active && sampleMorphIsArmed() &&
			sampleMorphGetPopulatedCount(i) > 0;
		sendCC(0, (uint8_t)(0x18 + i), morphVisible ? 1 : 0);
		if (morphVisible)
			sendCC(0, (uint8_t)(0x10 + i), sampleMorphGetRingValue(i));
	}
}

static void refreshGlobalButtons(void)
{
	const tapeheadMatrixTarget_t target = tapeheadActionMatrixGetTarget();
	bool anyMute = false, allMute = song.numChannels > 0;
	for (int32_t i = 0; i < song.numChannels && i < 8; i++)
	{
		anyMute |= performanceMute[i];
		allMute &= performanceMute[i];
	}

	sendNote(0, 0x3A, sampleMorphIsArmed());
	sendNote(0, 0x3B, sampleMorphIsArmed());
	sendNote(0, 0x3C, fastTracksPOCAnyEnabled());
	sendNote(0, 0x3D, fastTracksPOCAnyEnabled());
	sendNote(0, 0x3E, sampleMorphIsArmed());
	sendNote(0, 0x3F, fastTracksPOCAnyEnabled());
	sendNote(0, 0x40, fastTracksPOCTransmissionClutchIsLatched());
	sendNote(0, 0x41, fastTracksPOCMasterIsEnabled());
	sendNote(0, 0x50, allMute ? 1 : (anyMute ? 2 : 0));
	sendNote(0, 0x57, target == TAPEHEAD_MATRIX_PATTERN);
	sendNote(0, 0x58, target == TAPEHEAD_MATRIX_SAMPLE);
	sendNote(0, 0x59, patternLauncherStandaloneIsShown());
	sendNote(0, 0x5A, allSelectedFastTracksReversed());
	sendNote(0, 0x63, tapeheadActionFastTrackGlobalModeState());
	sendNote(0, 0x66, tapeheadActionMatrixGridIsPoly());
	sendNote(0, 0x67, selectedDeckHasWork());

	const bool playingSong = songPlaying && !patternLauncherIsEnabled() &&
		(playMode == PLAYMODE_SONG || playMode == PLAYMODE_RECSONG);
	const bool playingPattern = songPlaying && !patternLauncherIsEnabled() &&
		(playMode == PLAYMODE_PATT || playMode == PLAYMODE_RECPATT);
	sendNote(0, 0x5B, playingSong);
	sendNote(0, 0x5D, playingPattern);

	const tapeheadMatrixSequenceType_t sequenceType =
		tapeheadActionMatrixSequenceGetType();
	const int32_t sequenceIndex =
		tapeheadActionMatrixSequenceGetControlIndex();
	/* Scene LEDs are fixed-color. Keep the whole launch rail softly lit and
	** blink only the active automatic sequence control. */
	sendNote(0, 0x52,
		sequenceType == TAPEHEAD_MATRIX_SEQUENCE_BANK ? 2 : 1);
	for (uint8_t row = 0; row < 4; row++)
		sendNote(0, (uint8_t)(0x53 + row), sequenceType ==
			TAPEHEAD_MATRIX_SEQUENCE_ROW && sequenceIndex == row ? 2 : 1);
}

static void clearFeedbackSurface(void)
{
	if (!feedbackActive || !tapeheadMidiSurfaceOutputIsOpen())
		return;

	const uint8_t rgbChannels[] =
	{
		APC_RGB_PRIMARY, APC_RGB_PULSE_EIGHTH, APC_RGB_BLINK_EIGHTH
	};
	for (size_t channel = 0;
		channel < sizeof (rgbChannels) / sizeof (rgbChannels[0]); channel++)
	{
		for (uint8_t note = 0; note < 40; note++)
		{
			const uint8_t message[3] =
			{
				(uint8_t)(0x90 | rgbChannels[channel]), note, APC_COLOR_OFF
			};
			if (!sendMessage(message)) return;
		}
	}

	for (uint8_t channel = 0; channel < 8; channel++)
	{
		static const uint8_t stripNotes[] =
		{
			0x30, 0x31, 0x32, 0x33, 0x34, 0x42
		};
		for (size_t i = 0;
			i < sizeof (stripNotes) / sizeof (stripNotes[0]); i++)
		{
			const uint8_t message[3] =
			{
				(uint8_t)(0x90 | channel), stripNotes[i], 0
			};
			if (!sendMessage(message)) return;
		}
	}

	for (uint8_t note = 0x3A; note <= 0x67; note++)
	{
		const uint8_t message[3] = { 0x90, note, 0 };
		if (!sendMessage(message)) return;
	}
	for (uint8_t controller = 0x10; controller <= 0x3F; controller++)
	{
		const uint8_t message[3] = { 0xB0, controller, 0 };
		if (!sendMessage(message)) return;
	}
	resetCaches();
}

void tapeheadAPC40Mk2Open(void)
{
	profileActive = true;
	feedbackActive = false;
	resetCaches();
	if (!tapeheadMidiSurfaceOutputIsOpen()) return;

	uint8_t introduction[12];
	const size_t length = tapeheadAPC40Mk2BuildIntroduction(
		APC_MODE_ALTERNATE_ABLETON, introduction, sizeof (introduction));
	feedbackActive = length > 0 && tapeheadMidiSurfaceSend(introduction, length);
	if (feedbackActive)
	{
		/* Also neutralize animations retained after an earlier crash before the
		** first authoritative Phase 4.2 redraw. */
		clearFeedbackSurface();
		tapeheadAPC40Mk2Refresh();
	}
}

void tapeheadAPC40Mk2Close(void)
{
	if (feedbackActive && tapeheadMidiSurfaceOutputIsOpen())
	{
		clearFeedbackSurface();
		uint8_t introduction[12];
		const size_t length = tapeheadAPC40Mk2BuildIntroduction(
			APC_MODE_GENERIC, introduction, sizeof (introduction));
		if (length > 0) (void)tapeheadMidiSurfaceSend(introduction, length);
	}
	feedbackActive = profileActive = false;
}

void tapeheadAPC40Mk2Refresh(void)
{
	if (!profileActive || !feedbackActive) return;
	const tapeheadMatrixSequenceType_t sequenceType =
		tapeheadActionMatrixSequenceGetType();
	const int32_t sequenceIndex =
		tapeheadActionMatrixSequenceGetControlIndex();
	for (uint8_t i = 0; i < 8; i++)
	{
		const bool active = sequenceType == TAPEHEAD_MATRIX_SEQUENCE_COLUMN &&
			sequenceIndex == i;
		if (active)
			sendRGBAnimated((uint8_t)(32 + i),
				APC_COLOR_PURPLE,
				APC_RGB_PULSE_EIGHTH);
		else
			sendRGB((uint8_t)(32 + i), APC_COLOR_DIM_PURPLE);
	}
	for (uint8_t i = 0; i < 32; i++)
	{
		if (tapeheadActionMatrixGetTarget() == TAPEHEAD_MATRIX_SAMPLE)
			refreshSampleSlot(i);
		else
			refreshPatternSlot(i);
	}
	refreshTracks();
	refreshGlobalButtons();
}

bool tapeheadAPC40Mk2IsActive(void)
{
	return profileActive;
}

#else
typedef int prevent_compiler_warning;
#endif
