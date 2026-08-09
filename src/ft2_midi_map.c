#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL_atomic.h>
#include "ft2_header.h"
#include "ft2_fasttracks.h"
#include "ft2_midi_map.h"
#include "ft2_sample_morph.h"
#include "ft2_tapehead_actions.h"

enum
{
	TAPEHEAD_MIDI_INPUT_NOTE_ON = 1,
	TAPEHEAD_MIDI_INPUT_CC
};

enum
{
	TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_TOGGLE = 1,
	TAPEHEAD_MIDI_ACTION_PERFORMANCE_UNMUTE_ALL,
	TAPEHEAD_MIDI_ACTION_TRACK_TRIM,
	TAPEHEAD_MIDI_ACTION_TRACK_SELECT,
	TAPEHEAD_MIDI_ACTION_TRACK_MUTE_TOGGLE,
	TAPEHEAD_MIDI_ACTION_UNMUTE_ALL,
	TAPEHEAD_MIDI_ACTION_PERFORMANCE_UNMUTE_NEXT,
	TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_PREVIOUS,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_TOGGLE,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_MASTER_TOGGLE,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_NEXT,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_PREVIOUS,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_RESET,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_RESET_ALL,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_REVERSE_TOGGLE,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_CLUTCH_TOGGLE,
	TAPEHEAD_MIDI_ACTION_MATRIX_MODE_PATTERN,
	TAPEHEAD_MIDI_ACTION_MATRIX_MODE_SAMPLE,
	TAPEHEAD_MIDI_ACTION_MATRIX_MODE_TOGGLE,
	TAPEHEAD_MIDI_ACTION_MATRIX_BANK_SELECT,
	TAPEHEAD_MIDI_ACTION_MATRIX_BANK_NEXT,
	TAPEHEAD_MIDI_ACTION_MATRIX_BANK_PREVIOUS,
	TAPEHEAD_MIDI_ACTION_MATRIX_SLOT_TRIGGER,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_SONG,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_PATTERN,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_SONG,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_DECK,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_ALL,
	TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_ALL,
	TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_MASTER,
	TAPEHEAD_MIDI_ACTION_PERFORMANCE_SOLO_TOGGLE,
	TAPEHEAD_MIDI_ACTION_TRACK_RECORD_ARM,
	TAPEHEAD_MIDI_ACTION_CURSOR_LEFT,
	TAPEHEAD_MIDI_ACTION_CURSOR_RIGHT,
	TAPEHEAD_MIDI_ACTION_CURSOR_UP,
	TAPEHEAD_MIDI_ACTION_CURSOR_DOWN,
	TAPEHEAD_MIDI_ACTION_SONG_ORDER_PREVIOUS,
	TAPEHEAD_MIDI_ACTION_SONG_ORDER_NEXT,
	TAPEHEAD_MIDI_ACTION_SHIFT_MODIFIER,
	TAPEHEAD_MIDI_ACTION_MASTER_VOLUME,
	TAPEHEAD_MIDI_ACTION_TEMPO_RELATIVE,
	TAPEHEAD_MIDI_ACTION_SPEED_DOWN,
	TAPEHEAD_MIDI_ACTION_SPEED_UP,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_TRANSMISSION_CLUTCH,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_GLOBAL_REVERSE,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_DIRECTION_OR_SONG,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_ALL_NEXT,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_ALL_PREVIOUS,
	TAPEHEAD_MIDI_ACTION_MATRIX_GRID_MODE_TOGGLE,
	TAPEHEAD_MIDI_ACTION_MATRIX_VISIBILITY_TOGGLE,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_SELECTED_DECK,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_SELECTED_MODE,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_MODE_TOGGLE,
	TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_ARM_TOGGLE,
	TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_SELECT,
	TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_ALL_NEXT,
	TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_ALL_PREVIOUS,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_OR_MATRIX_NEXT,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_OR_MATRIX_PREVIOUS,
	TAPEHEAD_MIDI_ACTION_MATRIX_LAYER_BANK_SELECT,
	TAPEHEAD_MIDI_ACTION_MATRIX_SEQUENCE_ROW,
	TAPEHEAD_MIDI_ACTION_MATRIX_SEQUENCE_COLUMN,
	TAPEHEAD_MIDI_ACTION_MATRIX_SEQUENCE_BANK,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_SONG_TOGGLE,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_PATTERN_TOGGLE,
	TAPEHEAD_MIDI_ACTION_MATRIX_MASTER_VOLUME,
	TAPEHEAD_MIDI_ACTION_MATRIX_CROSSFADER,
	TAPEHEAD_MIDI_ACTION_FAST_TRACK_GLOBAL_MODE,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP,
	TAPEHEAD_MIDI_ACTION_PATTERN_JOG_RELATIVE,
	TAPEHEAD_MIDI_ACTION_PATTERN_JOG_ABSOLUTE,
	TAPEHEAD_MIDI_ACTION_TRANSPORT_PUNCH
};

typedef struct tapeheadMidiBinding_t
{
	uint8_t inputType, channel, number;
	uint8_t action;
	int16_t argument;
} tapeheadMidiBinding_t;

typedef struct tapeheadMidiEvent_t
{
	uint8_t inputType, channel, number, value;
	uint8_t action;
	int16_t argument;
} tapeheadMidiEvent_t;

static bool performanceControlEnabled;
static tapeheadMidiBinding_t bindings[TAPEHEAD_MIDI_MAP_MAX_BINDINGS];
static size_t bindingCount;

static SDL_SpinLock queueLock;
static tapeheadMidiEvent_t eventQueue[TAPEHEAD_MIDI_EVENT_QUEUE_CAPACITY];
static size_t queueReadPos, queueWritePos, queueCount;
static uint32_t droppedEventCount;

static bool textEqualsIgnoreCase(const char *a, const char *b)
{
	while (*a != '\0' && *b != '\0')
	{
		if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++))
			return false;
	}

	return *a == '\0' && *b == '\0';
}

static bool parseInput(const char *text, uint8_t *inputType, uint8_t *midiChannel,
	uint8_t *number)
{
	char kind[16], trailing;
	unsigned int parsedChannel, parsedNumber;
	if (sscanf(text, "%15[^.].%u.%u%c", kind, &parsedChannel,
		&parsedNumber, &trailing) != 3)
	{
		return false;
	}

	if (parsedChannel < 1 || parsedChannel > 16 || parsedNumber > 127)
		return false;

	if (textEqualsIgnoreCase(kind, "NoteOn") ||
		textEqualsIgnoreCase(kind, "Note"))
	{
		*inputType = TAPEHEAD_MIDI_INPUT_NOTE_ON;
	}
	else if (textEqualsIgnoreCase(kind, "CC"))
	{
		*inputType = TAPEHEAD_MIDI_INPUT_CC;
	}
	else
	{
		return false;
	}

	*midiChannel = (uint8_t)(parsedChannel - 1);
	*number = (uint8_t)parsedNumber;
	return true;
}

static bool parseNumberArgument(const char *text, const char *actionName,
	uint32_t maximum, int16_t *argument)
{
	const size_t nameLength = strlen(actionName);
	if (strlen(text) <= nameLength || text[nameLength] != ':' ||
		strncmp(text, actionName, nameLength) != 0)
	{
		return false;
	}

	unsigned int number;
	char trailing;
	if (sscanf(text + nameLength + 1, "%u%c", &number, &trailing) != 1 ||
		number < 1 || number > maximum)
	{
		return false;
	}

	*argument = (int16_t)(number - 1);
	return true;
}

static bool parseTrackArgument(const char *text, const char *actionName,
	int16_t *argument)
{
	return parseNumberArgument(text, actionName, MAX_CHANNELS, argument);
}

static bool parseAction(const char *text, uint8_t inputType, uint8_t *action,
	int16_t *argument)
{
	if (!strcmp(text, "None") || !strcmp(text, "Disabled"))
	{
		*action = 0;
		*argument = 0;
		return true;
	}

	if (inputType == TAPEHEAD_MIDI_INPUT_NOTE_ON)
	{
		if (parseTrackArgument(text, "TrackPerformanceMuteToggle", argument))
		{
			*action = TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_TOGGLE;
			return true;
		}

		if (!strcmp(text, "PerformanceUnmuteAll"))
		{
			*action = TAPEHEAD_MIDI_ACTION_PERFORMANCE_UNMUTE_ALL;
			*argument = 0;
			return true;
		}

#define PARSE_TRACK_ACTION(name, id) \
		if (parseTrackArgument(text, name, argument)) \
		{ \
			*action = id; \
			return true; \
		}

		PARSE_TRACK_ACTION("TrackSelect", TAPEHEAD_MIDI_ACTION_TRACK_SELECT)
		PARSE_TRACK_ACTION("TrackMuteToggle", TAPEHEAD_MIDI_ACTION_TRACK_MUTE_TOGGLE)
		PARSE_TRACK_ACTION("FastTrackToggle", TAPEHEAD_MIDI_ACTION_FAST_TRACK_TOGGLE)
		PARSE_TRACK_ACTION("FastTrackRatioNext", TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_NEXT)
		PARSE_TRACK_ACTION("FastTrackRatioPrevious", TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_PREVIOUS)
		PARSE_TRACK_ACTION("FastTrackRatioReset", TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_RESET)
		PARSE_TRACK_ACTION("FastTrackReverseToggle", TAPEHEAD_MIDI_ACTION_FAST_TRACK_REVERSE_TOGGLE)
		PARSE_TRACK_ACTION("FastTrackClutchToggle", TAPEHEAD_MIDI_ACTION_FAST_TRACK_CLUTCH_TOGGLE)
		PARSE_TRACK_ACTION("TrackPerformanceSoloToggle", TAPEHEAD_MIDI_ACTION_PERFORMANCE_SOLO_TOGGLE)
		PARSE_TRACK_ACTION("TrackRecordArm", TAPEHEAD_MIDI_ACTION_TRACK_RECORD_ARM)
		PARSE_TRACK_ACTION("FastTrackDirectionOrSongMode", TAPEHEAD_MIDI_ACTION_FAST_TRACK_DIRECTION_OR_SONG)

#undef PARSE_TRACK_ACTION

		if (parseNumberArgument(text, "MatrixBankSelect", 8, argument))
		{
			*action = TAPEHEAD_MIDI_ACTION_MATRIX_BANK_SELECT;
			return true;
		}
		if (parseNumberArgument(text, "MatrixLayerBankSelect", 8, argument))
		{
			*action = TAPEHEAD_MIDI_ACTION_MATRIX_LAYER_BANK_SELECT;
			return true;
		}
		if (parseNumberArgument(text, "MatrixSlotTrigger", 32, argument))
		{
			*action = TAPEHEAD_MIDI_ACTION_MATRIX_SLOT_TRIGGER;
			return true;
		}
		if (parseNumberArgument(text, "MatrixSequenceRow", 4, argument))
		{
			*action = TAPEHEAD_MIDI_ACTION_MATRIX_SEQUENCE_ROW;
			return true;
		}
		if (parseNumberArgument(text, "MatrixSequenceColumn", 8, argument))
		{
			*action = TAPEHEAD_MIDI_ACTION_MATRIX_SEQUENCE_COLUMN;
			return true;
		}

#define PARSE_SIMPLE_ACTION(name, id) \
		if (!strcmp(text, name)) \
		{ \
			*action = id; \
			*argument = 0; \
			return true; \
		}

		PARSE_SIMPLE_ACTION("UnmuteAll", TAPEHEAD_MIDI_ACTION_UNMUTE_ALL)
		PARSE_SIMPLE_ACTION("PerformanceUnmuteNext", TAPEHEAD_MIDI_ACTION_PERFORMANCE_UNMUTE_NEXT)
		PARSE_SIMPLE_ACTION("PerformanceMutePrevious", TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_PREVIOUS)
		PARSE_SIMPLE_ACTION("FastTrackMasterToggle", TAPEHEAD_MIDI_ACTION_FAST_TRACK_MASTER_TOGGLE)
		PARSE_SIMPLE_ACTION("FastTrackResetAll", TAPEHEAD_MIDI_ACTION_FAST_TRACK_RESET_ALL)
		PARSE_SIMPLE_ACTION("MatrixModePattern", TAPEHEAD_MIDI_ACTION_MATRIX_MODE_PATTERN)
		PARSE_SIMPLE_ACTION("MatrixModeSample", TAPEHEAD_MIDI_ACTION_MATRIX_MODE_SAMPLE)
		PARSE_SIMPLE_ACTION("MatrixModeToggle", TAPEHEAD_MIDI_ACTION_MATRIX_MODE_TOGGLE)
		PARSE_SIMPLE_ACTION("MatrixBankNext", TAPEHEAD_MIDI_ACTION_MATRIX_BANK_NEXT)
		PARSE_SIMPLE_ACTION("MatrixBankPrevious", TAPEHEAD_MIDI_ACTION_MATRIX_BANK_PREVIOUS)
		PARSE_SIMPLE_ACTION("TransportPlaySong", TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_SONG)
		PARSE_SIMPLE_ACTION("TransportPlayPattern", TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_PATTERN)
		PARSE_SIMPLE_ACTION("TransportStopSong", TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_SONG)
		PARSE_SIMPLE_ACTION("TransportStopDeck", TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_DECK)
		PARSE_SIMPLE_ACTION("TransportStopAll", TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_ALL)
		PARSE_SIMPLE_ACTION("PerformanceMuteAll", TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_ALL)
		PARSE_SIMPLE_ACTION("PerformanceMuteMaster", TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_MASTER)
		PARSE_SIMPLE_ACTION("CursorLeft", TAPEHEAD_MIDI_ACTION_CURSOR_LEFT)
		PARSE_SIMPLE_ACTION("CursorRight", TAPEHEAD_MIDI_ACTION_CURSOR_RIGHT)
		PARSE_SIMPLE_ACTION("CursorUp", TAPEHEAD_MIDI_ACTION_CURSOR_UP)
		PARSE_SIMPLE_ACTION("CursorDown", TAPEHEAD_MIDI_ACTION_CURSOR_DOWN)
		PARSE_SIMPLE_ACTION("SongOrderPrevious", TAPEHEAD_MIDI_ACTION_SONG_ORDER_PREVIOUS)
		PARSE_SIMPLE_ACTION("SongOrderNext", TAPEHEAD_MIDI_ACTION_SONG_ORDER_NEXT)
		PARSE_SIMPLE_ACTION("ShiftModifier", TAPEHEAD_MIDI_ACTION_SHIFT_MODIFIER)
		PARSE_SIMPLE_ACTION("SpeedDown", TAPEHEAD_MIDI_ACTION_SPEED_DOWN)
		PARSE_SIMPLE_ACTION("SpeedUp", TAPEHEAD_MIDI_ACTION_SPEED_UP)
		PARSE_SIMPLE_ACTION("FastTrackTransmissionClutchToggle", TAPEHEAD_MIDI_ACTION_FAST_TRACK_TRANSMISSION_CLUTCH)
		PARSE_SIMPLE_ACTION("FastTrackGlobalReverseToggle", TAPEHEAD_MIDI_ACTION_FAST_TRACK_GLOBAL_REVERSE)
		PARSE_SIMPLE_ACTION("FastTrackGlobalModeToggle", TAPEHEAD_MIDI_ACTION_FAST_TRACK_GLOBAL_MODE)
		PARSE_SIMPLE_ACTION("FastTrackRatioAllNext", TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_ALL_NEXT)
		PARSE_SIMPLE_ACTION("FastTrackRatioAllPrevious", TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_ALL_PREVIOUS)
		PARSE_SIMPLE_ACTION("FastTrackRatioAllOrMatrixBankNext", TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_OR_MATRIX_NEXT)
		PARSE_SIMPLE_ACTION("FastTrackRatioAllOrMatrixBankPrevious", TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_OR_MATRIX_PREVIOUS)
		PARSE_SIMPLE_ACTION("MatrixGridModeToggle", TAPEHEAD_MIDI_ACTION_MATRIX_GRID_MODE_TOGGLE)
		PARSE_SIMPLE_ACTION("MatrixSequenceBank", TAPEHEAD_MIDI_ACTION_MATRIX_SEQUENCE_BANK)
		PARSE_SIMPLE_ACTION("MatrixVisibilityToggle", TAPEHEAD_MIDI_ACTION_MATRIX_VISIBILITY_TOGGLE)
		PARSE_SIMPLE_ACTION("TransportStopSelectedDeck", TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_SELECTED_DECK)
		PARSE_SIMPLE_ACTION("TransportPlaySelectedMode", TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_SELECTED_MODE)
		PARSE_SIMPLE_ACTION("TransportPlaySongToggle", TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_SONG_TOGGLE)
		PARSE_SIMPLE_ACTION("TransportPlayPatternToggle", TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_PATTERN_TOGGLE)
		PARSE_SIMPLE_ACTION("TransportStop", TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP)
		PARSE_SIMPLE_ACTION("TransportModeToggle", TAPEHEAD_MIDI_ACTION_TRANSPORT_MODE_TOGGLE)
		PARSE_SIMPLE_ACTION("SampleMorphArmToggle", TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_ARM_TOGGLE)
		PARSE_SIMPLE_ACTION("SampleMorphAllNext", TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_ALL_NEXT)
		PARSE_SIMPLE_ACTION("SampleMorphAllPrevious", TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_ALL_PREVIOUS)

#undef PARSE_SIMPLE_ACTION
	}
	else if (inputType == TAPEHEAD_MIDI_INPUT_CC)
	{
		if (parseTrackArgument(text, "TrackTrim", argument))
		{
			*action = TAPEHEAD_MIDI_ACTION_TRACK_TRIM;
			return true;
		}
		if (parseTrackArgument(text, "FastTrackRatio", argument))
		{
			*action = TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO;
			return true;
		}
		if (parseTrackArgument(text, "SampleMorphSelect", argument))
		{
			*action = TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_SELECT;
			return true;
		}
		if (!strcmp(text, "MasterVolume"))
		{
			*action = TAPEHEAD_MIDI_ACTION_MASTER_VOLUME;
			*argument = 0;
			return true;
		}
		if (!strcmp(text, "TempoRelative"))
		{
			*action = TAPEHEAD_MIDI_ACTION_TEMPO_RELATIVE;
			*argument = 0;
			return true;
		}
		if (!strcmp(text, "MatrixMasterVolume"))
		{
			*action = TAPEHEAD_MIDI_ACTION_MATRIX_MASTER_VOLUME;
			*argument = 0;
			return true;
		}
		if (!strcmp(text, "MatrixCrossfader"))
		{
			*action = TAPEHEAD_MIDI_ACTION_MATRIX_CROSSFADER;
			*argument = 0;
			return true;
		}
		if (!strcmp(text, "PatternJogRelative"))
		{
			*action = TAPEHEAD_MIDI_ACTION_PATTERN_JOG_RELATIVE;
			*argument = 0;
			return true;
		}
		if (!strcmp(text, "PatternJogAbsolute"))
		{
			*action = TAPEHEAD_MIDI_ACTION_PATTERN_JOG_ABSOLUTE;
			*argument = 0;
			return true;
		}
		if (!strcmp(text, "TransportPunch"))
		{
			*action = TAPEHEAD_MIDI_ACTION_TRANSPORT_PUNCH;
			*argument = 0;
			return true;
		}
	}

	return false;
}

static const tapeheadMidiBinding_t *findBinding(uint8_t inputType,
	uint8_t midiChannel, uint8_t number)
{
	for (size_t i = 0; i < bindingCount; i++)
	{
		const tapeheadMidiBinding_t *binding = &bindings[i];
		if (binding->inputType == inputType && binding->channel == midiChannel &&
			binding->number == number)
		{
			return binding;
		}
	}

	return NULL;
}

static bool enqueueMappedEvent(const tapeheadMidiBinding_t *binding,
	uint8_t value)
{
	SDL_AtomicLock(&queueLock);

	/* A fader can emit far more values than the UI can draw. Keep only the
	** newest pending value for each physical channel/controller pair. */
	if (binding->inputType == TAPEHEAD_MIDI_INPUT_CC &&
		binding->action != TAPEHEAD_MIDI_ACTION_TEMPO_RELATIVE &&
		binding->action != TAPEHEAD_MIDI_ACTION_PATTERN_JOG_RELATIVE &&
		binding->action != TAPEHEAD_MIDI_ACTION_PATTERN_JOG_ABSOLUTE &&
		binding->action != TAPEHEAD_MIDI_ACTION_TRANSPORT_PUNCH)
	{
		for (size_t i = 0, pos = queueReadPos; i < queueCount; i++)
		{
			tapeheadMidiEvent_t *event = &eventQueue[pos];
			if (event->inputType == TAPEHEAD_MIDI_INPUT_CC &&
				event->channel == binding->channel &&
				event->number == binding->number)
			{
				event->value = value;
				SDL_AtomicUnlock(&queueLock);
				return true;
			}

			pos = (pos + 1) % TAPEHEAD_MIDI_EVENT_QUEUE_CAPACITY;
		}
	}

	if (queueCount >= TAPEHEAD_MIDI_EVENT_QUEUE_CAPACITY)
	{
		droppedEventCount++;
		SDL_AtomicUnlock(&queueLock);
		return false;
	}

	tapeheadMidiEvent_t *event = &eventQueue[queueWritePos];
	event->inputType = binding->inputType;
	event->channel = binding->channel;
	event->number = binding->number;
	event->value = value;
	event->action = binding->action;
	event->argument = binding->argument;

	queueWritePos = (queueWritePos + 1) % TAPEHEAD_MIDI_EVENT_QUEUE_CAPACITY;
	queueCount++;
	SDL_AtomicUnlock(&queueLock);
	return true;
}

static bool popMappedEvent(tapeheadMidiEvent_t *event)
{
	SDL_AtomicLock(&queueLock);
	if (queueCount == 0)
	{
		SDL_AtomicUnlock(&queueLock);
		return false;
	}

	*event = eventQueue[queueReadPos];
	queueReadPos = (queueReadPos + 1) % TAPEHEAD_MIDI_EVENT_QUEUE_CAPACITY;
	queueCount--;
	SDL_AtomicUnlock(&queueLock);
	return true;
}

void tapeheadMidiMapReset(void)
{
	/* Configuration is loaded before RtMidi's callback thread starts. */
	performanceControlEnabled = false;
	bindingCount = 0;

	SDL_AtomicLock(&queueLock);
	queueReadPos = 0;
	queueWritePos = 0;
	queueCount = 0;
	droppedEventCount = 0;
	SDL_AtomicUnlock(&queueLock);
}

void tapeheadMidiMapSetEnabled(bool enabled)
{
	performanceControlEnabled = enabled;
}

bool tapeheadMidiMapIsEnabled(void)
{
	return performanceControlEnabled;
}

bool tapeheadMidiMapAddBinding(const char *input, const char *actionText)
{
	if (input == NULL || actionText == NULL)
		return false;

	tapeheadMidiBinding_t binding;
	memset(&binding, 0, sizeof (binding));
	if (!parseInput(input, &binding.inputType, &binding.channel,
		&binding.number) ||
		!parseAction(actionText, binding.inputType, &binding.action,
			&binding.argument))
	{
		return false;
	}

	for (size_t i = 0; i < bindingCount; i++)
	{
		if (bindings[i].inputType == binding.inputType &&
			bindings[i].channel == binding.channel &&
			bindings[i].number == binding.number)
		{
			bindings[i] = binding; /* conventional INI last-value-wins rule */
			return true;
		}
	}

	if (bindingCount >= TAPEHEAD_MIDI_MAP_MAX_BINDINGS)
		return false;

	bindings[bindingCount++] = binding;
	return true;
}

bool tapeheadMidiMapDisableBinding(const char *input)
{
	return tapeheadMidiMapAddBinding(input, "None");
}

bool tapeheadMidiMapAddDefaultBinding(const char *input,
	const char *actionText)
{
	if (input == NULL || actionText == NULL)
		return false;

	tapeheadMidiBinding_t binding;
	memset(&binding, 0, sizeof (binding));
	if (!parseInput(input, &binding.inputType, &binding.channel,
		&binding.number) ||
		!parseAction(actionText, binding.inputType, &binding.action,
			&binding.argument))
	{
		return false;
	}

	if (findBinding(binding.inputType, binding.channel, binding.number) != NULL)
		return true;

	if (bindingCount >= TAPEHEAD_MIDI_MAP_MAX_BINDINGS)
		return false;

	bindings[bindingCount++] = binding;
	return true;
}

bool tapeheadMidiMapHandleMessage(uint8_t status, uint8_t data1, uint8_t data2)
{
	if (!performanceControlEnabled || status < 0x80 || status >= 0xF0)
		return false;

	const uint8_t messageType = status & 0xF0;
	const uint8_t midiChannel = status & 0x0F;
	data1 &= 0x7F;
	data2 &= 0x7F;

	if (messageType == 0x80 || (messageType == 0x90 && data2 == 0))
	{
		const tapeheadMidiBinding_t *releaseBinding = findBinding(
			TAPEHEAD_MIDI_INPUT_NOTE_ON, midiChannel, data1);
		/* Toggle buttons remain press-edge actions. The Shift modifier is the one
		** binding whose release is meaningful and must reach the main thread. */
		if (releaseBinding != NULL &&
			releaseBinding->action == TAPEHEAD_MIDI_ACTION_SHIFT_MODIFIER)
		{
			enqueueMappedEvent(releaseBinding, 0);
		}
		return releaseBinding != NULL;
	}

	uint8_t inputType;
	if (messageType == 0x90)
		inputType = TAPEHEAD_MIDI_INPUT_NOTE_ON;
	else if (messageType == 0xB0)
		inputType = TAPEHEAD_MIDI_INPUT_CC;
	else
		return false;

	const tapeheadMidiBinding_t *binding =
		findBinding(inputType, midiChannel, data1);
	if (binding == NULL)
		return false;

	/* A full queue still consumes the configured controller message so it
	** cannot leak into FT2's musical note-entry path. */
	enqueueMappedEvent(binding, data2);
	return true;
}

void tapeheadMidiMapProcessPending(void)
{
	tapeheadMidiEvent_t event;
	while (popMappedEvent(&event))
	{
		if (event.action == 0)
			continue;

		switch (event.action)
		{
			case TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_TOGGLE:
				tapeheadActionTrackPerformanceMuteToggle(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_PERFORMANCE_UNMUTE_ALL:
				tapeheadActionPerformanceUnmuteAll();
			break;

			case TAPEHEAD_MIDI_ACTION_TRACK_TRIM:
			{
				const int32_t trim =
					(event.value * TAPEHEAD_TRACK_TRIM_MAX + 63) / 127;
				tapeheadActionTrackTrimSet(event.argument, trim);
			}
			break;

			case TAPEHEAD_MIDI_ACTION_TRACK_SELECT:
				tapeheadActionTrackSelect(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_TRACK_MUTE_TOGGLE:
				tapeheadActionTrackMuteToggle(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_UNMUTE_ALL:
				tapeheadActionUnmuteAll();
			break;

			case TAPEHEAD_MIDI_ACTION_PERFORMANCE_UNMUTE_NEXT:
				tapeheadActionPerformanceUnmuteNext();
			break;

			case TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_PREVIOUS:
				tapeheadActionPerformanceMutePrevious();
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_TOGGLE:
				tapeheadActionFastTrackToggle(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_MASTER_TOGGLE:
				tapeheadActionFastTrackMasterToggle();
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO:
			{
				const int32_t lastRatio = fastTracksPOCGetRatioCount() - 1;
				const int32_t ratioIndex = (event.value * lastRatio + 63) / 127;
				tapeheadActionFastTrackRatioSet(event.argument, ratioIndex);
			}
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_NEXT:
				tapeheadActionFastTrackRatioNext(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_PREVIOUS:
				tapeheadActionFastTrackRatioPrevious(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_RESET:
				tapeheadActionFastTrackRatioReset(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_RESET_ALL:
				tapeheadActionFastTrackResetAll();
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_REVERSE_TOGGLE:
				tapeheadActionFastTrackReverseToggle(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_CLUTCH_TOGGLE:
				if (!tapeheadActionShiftModifierIsHeld())
					tapeheadActionFastTrackClutchToggle(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_MODE_PATTERN:
				tapeheadActionMatrixSetTarget(TAPEHEAD_MATRIX_PATTERN);
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_MODE_SAMPLE:
				tapeheadActionMatrixSetTarget(TAPEHEAD_MATRIX_SAMPLE);
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_MODE_TOGGLE:
				tapeheadActionMatrixToggleTarget();
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_BANK_SELECT:
				tapeheadActionMatrixBankSelect((uint8_t)event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_LAYER_BANK_SELECT:
				tapeheadActionMatrixLayerBankSelect((uint8_t)event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_BANK_NEXT:
				tapeheadActionMatrixBankNext();
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_BANK_PREVIOUS:
				tapeheadActionMatrixBankPrevious();
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_SLOT_TRIGGER:
				tapeheadActionMatrixSlotTrigger((uint8_t)event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_SEQUENCE_ROW:
				tapeheadActionMatrixSequenceRow((uint8_t)event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_SEQUENCE_COLUMN:
				tapeheadActionMatrixSequenceColumn((uint8_t)event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_SEQUENCE_BANK:
				tapeheadActionMatrixSequenceBank();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_SONG:
				tapeheadActionTransportPlaySong();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_PATTERN:
				tapeheadActionTransportPlayPattern();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_SONG:
				tapeheadActionTransportStopSong();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_DECK:
				tapeheadActionTransportStopDeck();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_ALL:
				tapeheadActionTransportStopAll();
			break;

			case TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_ALL:
				tapeheadActionPerformanceMuteAll();
			break;

			case TAPEHEAD_MIDI_ACTION_PERFORMANCE_MUTE_MASTER:
				tapeheadActionPerformanceMuteMaster();
			break;

			case TAPEHEAD_MIDI_ACTION_PERFORMANCE_SOLO_TOGGLE:
				tapeheadActionTrackPerformanceSoloToggle(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_TRACK_RECORD_ARM:
				tapeheadActionTrackRecordArm(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_CURSOR_LEFT:
				tapeheadActionCursorLeft();
			break;

			case TAPEHEAD_MIDI_ACTION_CURSOR_RIGHT:
				tapeheadActionCursorRight();
			break;

			case TAPEHEAD_MIDI_ACTION_CURSOR_UP:
				tapeheadActionCursorUp();
			break;

			case TAPEHEAD_MIDI_ACTION_CURSOR_DOWN:
				tapeheadActionCursorDown();
			break;

			case TAPEHEAD_MIDI_ACTION_SONG_ORDER_PREVIOUS:
				tapeheadActionSongOrderPrevious();
			break;

			case TAPEHEAD_MIDI_ACTION_SONG_ORDER_NEXT:
				tapeheadActionSongOrderNext();
			break;

			case TAPEHEAD_MIDI_ACTION_SHIFT_MODIFIER:
				tapeheadActionSetShiftModifier(event.value != 0);
			break;

			case TAPEHEAD_MIDI_ACTION_MASTER_VOLUME:
				tapeheadActionMasterVolumeSet((event.value * 256 + 63) / 127);
			break;

			case TAPEHEAD_MIDI_ACTION_TEMPO_RELATIVE:
			{
				const int32_t delta = event.value < 64 ? event.value :
					(int32_t)event.value - 128;
				tapeheadActionTempoAdjust(delta);
			}
			break;

			case TAPEHEAD_MIDI_ACTION_SPEED_DOWN:
				tapeheadActionSpeedAdjust(-1);
			break;

			case TAPEHEAD_MIDI_ACTION_SPEED_UP:
				tapeheadActionSpeedAdjust(1);
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_TRANSMISSION_CLUTCH:
				tapeheadActionFastTrackTransmissionClutchToggle();
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_GLOBAL_REVERSE:
				tapeheadActionFastTrackGlobalReverseToggle();
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_GLOBAL_MODE:
				tapeheadActionFastTrackGlobalModeToggle();
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_DIRECTION_OR_SONG:
				tapeheadActionFastTrackDirectionOrSongMode(event.argument);
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_ALL_NEXT:
				tapeheadActionFastTrackRatioAllNext();
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_ALL_PREVIOUS:
				tapeheadActionFastTrackRatioAllPrevious();
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_GRID_MODE_TOGGLE:
				tapeheadActionMatrixGridModeToggle();
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_VISIBILITY_TOGGLE:
				tapeheadActionMatrixVisibilityToggle();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP_SELECTED_DECK:
				tapeheadActionTransportStopSelectedDeck();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_SELECTED_MODE:
				tapeheadActionTransportPlaySelectedMode();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_SONG_TOGGLE:
				tapeheadActionTransportPlaySongToggle();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_PLAY_PATTERN_TOGGLE:
				tapeheadActionTransportPlayPatternToggle();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_STOP:
				tapeheadActionTransportStop();
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_MODE_TOGGLE:
				tapeheadActionTransportModeToggle();
			break;

			case TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_ARM_TOGGLE:
				sampleMorphToggleArmed();
			break;

			case TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_SELECT:
				sampleMorphSetFromController(event.argument, event.value);
			break;

			case TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_ALL_NEXT:
				sampleMorphStepAll(1);
			break;

			case TAPEHEAD_MIDI_ACTION_SAMPLE_MORPH_ALL_PREVIOUS:
				sampleMorphStepAll(-1);
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_MASTER_VOLUME:
				tapeheadActionMatrixMasterVolumeSet(
					(event.value * 256 + 63) / 127);
			break;

			case TAPEHEAD_MIDI_ACTION_MATRIX_CROSSFADER:
				tapeheadActionMatrixCrossfaderSet(event.value);
			break;

			case TAPEHEAD_MIDI_ACTION_PATTERN_JOG_RELATIVE:
			{
				const int32_t delta = event.value < 64 ? event.value :
					(int32_t)event.value - 128;
				tapeheadActionPatternJogRelative(delta);
			}
			break;

			case TAPEHEAD_MIDI_ACTION_PATTERN_JOG_ABSOLUTE:
				tapeheadActionPatternJogAbsolute(event.value);
			break;

			case TAPEHEAD_MIDI_ACTION_TRANSPORT_PUNCH:
				tapeheadActionTransportPunchPedal(event.value >= 64);
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_OR_MATRIX_NEXT:
				tapeheadActionFastTrackRatioAllOrMatrixBankNext();
			break;

			case TAPEHEAD_MIDI_ACTION_FAST_TRACK_RATIO_OR_MATRIX_PREVIOUS:
				tapeheadActionFastTrackRatioAllOrMatrixBankPrevious();
			break;

			default: break;
		}
	}

	tapeheadActionPatternJogService();
}

size_t tapeheadMidiMapGetBindingCount(void)
{
	return bindingCount;
}

size_t tapeheadMidiMapGetPendingCount(void)
{
	SDL_AtomicLock(&queueLock);
	const size_t count = queueCount;
	SDL_AtomicUnlock(&queueLock);
	return count;
}

uint32_t tapeheadMidiMapGetDroppedEventCount(void)
{
	SDL_AtomicLock(&queueLock);
	const uint32_t count = droppedEventCount;
	SDL_AtomicUnlock(&queueLock);
	return count;
}
