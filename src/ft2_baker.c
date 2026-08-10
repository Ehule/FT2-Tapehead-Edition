#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_baker.h"
#include "ft2_baker_assets.h"
#include "ft2_baker_core.h"
#include "ft2_fasttracks.h"
#include "ft2_gui.h"
#include "ft2_module_saver.h"
#include "ft2_mouse.h"
#include "ft2_microtonal.h"
#include "ft2_pattern_launcher.h"
#include "ft2_poly_matrix.h"
#include "ft2_structs.h"
#include "ft2_sysreqs.h"
#include "ft2_sample_launcher.h"
#include "ft2_video.h"

#define BAKE_PATTERN_ROWS 64
#define BAKE_MAX_ROWS (MAX_PATTERNS * BAKE_PATTERN_ROWS)
#define BAKE_TICK_SAFETY_LIMIT 10000000U

enum
{
	BAKE_IDLE = 0,
	BAKE_OFFLINE,
	BAKE_LIVE_ARMED,
	BAKE_LIVE_CAPTURING,
	BAKE_PERFORMANCE_CAPTURING,
	BAKE_LIVE_SAVING
};

static volatile uint8_t bakeState;
static SDL_Thread *bakeThread;
static note_t *bakePatterns[MAX_PATTERNS];
static int32_t bakeRow;
static uint32_t bakeCollisions, bakeUnsupportedSubTicks, bakeRelocatedEvents;
static uint32_t bakeMergedDuplicateEvents, bakeStrippedMicrotonalCommands;
static uint32_t bakePreservedMicrotonalCommands;
static bool bakeOverflow, bakeMergeExactDuplicates, bakeTickResolution;
static bakerOutputTarget_t bakeOutputTarget;
static uint16_t bakeInitialBPM, bakeInitialSpeed;
static uint64_t bakePerformanceStartCounter;
static UNICHAR bakeFilenameU[PATH_MAX+1];
static bakerChannelAllocator_t bakeAllocator;
static bakerAudibilityState_t bakeAudibility;
static uint32_t bakeSkippedSampleLaunches;
static bool bakeInvalidSampleTiles[SAMPLE_LAUNCHER_MAX_TILES];
static int8_t bakeSampleVoiceChannel[SAMPLE_LAUNCHER_MAX_POLY + 1];

static bool sampleMatrixPreflight(void)
{
	char positions[160] = "";
	uint32_t invalidCount = 0;
	for (uint16_t tile = 0; tile < SAMPLE_LAUNCHER_MAX_TILES; tile++)
	{
		if (!sampleLauncherTileIsPopulated(tile) || sampleLauncherTileIsLoaded(tile))
			continue;
		bakeInvalidSampleTiles[tile] = true;
		invalidCount++;
		if (strlen(positions) < sizeof (positions) - 12)
		{
			char position[12];
			snprintf(position, sizeof (position), "%sB%u/T%u",
				positions[0] == '\0' ? "" : ", ",
				tile / SAMPLE_LAUNCHER_TILES_PER_BANK,
				tile % SAMPLE_LAUNCHER_TILES_PER_BANK);
			strncat(positions, position, sizeof (positions) - strlen(positions) - 1);
		}
	}
	if (invalidCount == 0)
		return true;

	char message[512];
	snprintf(message, sizeof (message),
		"Some populated Sample Matrix tiles do not have valid sample mappings and cannot be represented in the baked XM: %s. Their launches will be omitted. All other performance activity will be recorded. Continue?",
		positions);
	return okBox(2, "Bake Sample Matrix", message, NULL) == 1;
}

bool bakerIsRunning(void)
{
	return bakeState == BAKE_OFFLINE || bakeState == BAKE_LIVE_CAPTURING ||
		bakeState == BAKE_PERFORMANCE_CAPTURING;
}

bool bakerIsOfflineRunning(void)
{
	return bakeState == BAKE_OFFLINE;
}

bool bakerLiveIsArmed(void)
{
	return bakeState == BAKE_LIVE_ARMED;
}

bool bakerLiveIsCapturing(void)
{
	return bakeState == BAKE_LIVE_CAPTURING ||
		bakeState == BAKE_PERFORMANCE_CAPTURING;
}

static int32_t performanceElapsedRow(void)
{
	const uint64_t frequency = SDL_GetPerformanceFrequency();
	if (frequency == 0)
		return 0;

	const uint64_t elapsed = SDL_GetPerformanceCounter() -
		bakePerformanceStartCounter;
	/* One TPL-1 XM row is one tracker tick: 2.5/BPM seconds. Keep the
	** calculation integral so long performances remain stable. */
	const uint64_t numerator = elapsed * bakeInitialBPM * 2;
	const uint64_t denominator = frequency * 5;
	const uint64_t row = numerator / denominator;
	return row >= BAKE_MAX_ROWS ? BAKE_MAX_ROWS : (int32_t)row;
}

void bakerBeginManualRow(void)
{
	if (bakeState == BAKE_LIVE_ARMED)
	{
		bakeInitialBPM = song.BPM;
		bakeInitialSpeed = song.speed;
		bakePerformanceStartCounter = SDL_GetPerformanceCounter();
		bakeState = BAKE_PERFORMANCE_CAPTURING;
		showRecPlusOverlay("PERFORMANCE BAKE");
	}

	if (bakeState == BAKE_PERFORMANCE_CAPTURING)
	{
		const int32_t elapsedRow = performanceElapsedRow();
		/* An absolute fader message may cross many source rows at one instant.
		** Preserve their order instead of collapsing them into one XM cell. */
		bakeRow = MAX(elapsedRow, bakeRow + 1);
		if (bakeRow >= BAKE_MAX_ROWS)
			bakeOverflow = true;
	}
}

void bakerCaptureManualEvent(int32_t channelIndex, const note_t *event)
{
	bakerCaptureEvent(channelIndex, event);
}

static bool eventIsEmpty(const note_t *event)
{
	return event->note == 0 && event->instr == 0 && event->vol == 0 &&
		event->efx == 0 && event->efxData == 0;
}

static bool compositionNeedsTickResolution(const fastTracksRuntimeState_t *fastTracksState)
{
	if (fastTracksState != NULL)
	{
		for (int32_t i = 0; i < FAST_TRACKS_CHANNEL_COUNT; i++)
		{
			if (fastTracksState->tracks[i].mode != FAST_TRACKS_MODE_STANDARD)
				return true;
		}
	}

	/* A composition may deliberately begin with the Fast Tracks master off and
	** enable or configure it later through Zxx. Detect that source language up
	** front so the offline baker does not choose the coarse row clock before
	** the command is encountered. */
	for (int32_t orderIndex = 0; orderIndex < song.songLength; orderIndex++)
	{
		const int32_t patternIndex = song.orders[orderIndex];
		if (pattern[patternIndex] == NULL)
			continue;

		const int32_t rows = CLAMP(patternNumRows[patternIndex], 1, 256);
		for (int32_t row = 0; row < rows; row++)
		{
			const note_t *rowData = &pattern[patternIndex][row * MAX_CHANNELS];
			for (int32_t channelIndex = 0; channelIndex < song.numChannels; channelIndex++)
			{
				if (rowData[channelIndex].efx == 0x23)
					return true;
			}
		}
	}

	return false;
}

void bakerBeginTick(void)
{
	const bool performanceClock = songPlaying || patternLauncherHasRouting() ||
		polyMatrixHasAudioWork() || sampleLauncherHasTransportWork();
	if (bakerIsRunning() &&
		bakerTimelineShouldAdvance(bakeTickResolution, performanceClock, song.tick))
	{
		if (bakeRow < BAKE_MAX_ROWS)
			bakeRow++;

		if (bakeRow >= BAKE_MAX_ROWS)
			bakeOverflow = true;
	}

	if (bakerIsRunning())
	{
		const uint32_t becameMuted = bakerAudibilityUpdate(&bakeAudibility,
			editor.channelMuted, performanceMute, (uint8_t)song.numChannels);
		for (int32_t i = 0; i < song.numChannels; i++)
		{
			if ((becameMuted & (UINT32_C(1) << i)) != 0)
			{
				note_t cut;
				memset(&cut, 0, sizeof (cut));
				cut.note = NOTE_OFF;
				bakerCaptureEvent(i, &cut);
			}
		}
	}
}

void bakerCaptureResolvedEvent(int32_t channelIndex, const note_t *event,
	uint8_t resolvedInstrument, uint8_t resolvedSample)
{
	if (!bakerChannelIsAudible(&bakeAudibility, channelIndex))
		return;

	note_t resolved = *event;
	if (resolved.note >= 1 && resolved.note <= 96 && resolvedInstrument > 0 &&
		resolvedInstrument <= MAX_INST && instr[resolvedInstrument] != NULL)
	{
		const uint8_t bakedInstrument = bakerAssetsResolveInstrument(resolved.note,
			resolvedInstrument, resolvedSample);
		if (bakedInstrument == 0)
			return; /* A hard asset error prevents saving; never capture the wrong sample. */
		resolved.instr = bakedInstrument;
	}
	bakerCaptureEvent(channelIndex, &resolved);
}

void bakerCaptureSampleLauncherAction(uint16_t tile, uint8_t voice, bool start)
{
	if (!bakerLiveIsCapturing() && !bakerLiveIsArmed())
		return;
	if (bakerLiveIsArmed())
		bakerBeginManualRow();

	int32_t channelIndex = song.numChannels > 0
		? (tile < SAMPLE_LAUNCHER_MAX_TILES
			? sampleLauncherGetTileBus(tile) : voice) % song.numChannels : 0;
	if (!start)
	{
		if (voice <= SAMPLE_LAUNCHER_MAX_POLY && bakeSampleVoiceChannel[voice] >= 0)
			channelIndex = bakeSampleVoiceChannel[voice];
		note_t cut;
		memset(&cut, 0, sizeof (cut));
		cut.note = NOTE_OFF;
		bakerCaptureResolvedEvent(channelIndex, &cut, 0, 0);
		if (voice <= SAMPLE_LAUNCHER_MAX_POLY)
			bakeSampleVoiceChannel[voice] = -1;
		return;
	}

	uint8_t instrument, sample;
	if (!sampleLauncherGetTileReference(tile, &instrument, &sample) ||
		instrument == 0 || instrument > MAX_INST || instr[instrument] == NULL ||
		instr[instrument]->smp[sample].dataPtr == NULL ||
		instr[instrument]->smp[sample].length == 0)
	{
		if (tile < SAMPLE_LAUNCHER_MAX_TILES)
			bakeInvalidSampleTiles[tile] = true;
		bakeSkippedSampleLaunches++;
		return;
	}

	note_t event;
	memset(&event, 0, sizeof (event));
	if (start)
	{
		event.note = NOTE_C4 + sample + 1;
		event.instr = instrument;
	}
	else
	{
		event.note = NOTE_OFF;
	}
	bakerCaptureResolvedEvent(channelIndex, &event, instrument, sample);
	if (voice <= SAMPLE_LAUNCHER_MAX_POLY)
		bakeSampleVoiceChannel[voice] = (int8_t)channelIndex;
}

void bakerCaptureEvent(int32_t channelIndex, const note_t *event)
{
	if (!bakerIsRunning() || event == NULL || eventIsEmpty(event) ||
		channelIndex < 0 || channelIndex >= song.numChannels ||
		bakeRow < 0 || bakeRow >= BAKE_MAX_ROWS)
	{
		return;
	}

	note_t flattened = *event;

	/* These commands have already done their work in the source replayer. A
	** conventional XM must receive their outcome, not repeat the Tapehead or
	** source-flow instruction in its newly linear order list. */
	if (microtonalEffectIsPitchExtension(flattened.efx))
	{
		if (bakeOutputTarget == BAKER_OUTPUT_STANDARD_XM)
		{
			/* Standard XM has no cent-accurate persistent offset or smooth
			** drift. Strip the extension deliberately; never make another
			** player guess. */
			flattened.efx = 0;
			flattened.efxData = 0;
			bakeStrippedMicrotonalCommands++;
		}
		else
		{
			/* Tapehead XM uses the same XM pattern cells and keeps Mxx/Nxx as
			** compositional instructions for a later Tapehead playback. */
			bakePreservedMicrotonalCommands++;
		}
	}
	else if (flattened.efx == 0x0B || flattened.efx == 0x0D || flattened.efx == 0x23 ||
		(flattened.efx == 0x0E && (flattened.efxData & 0xF0) == 0x60))
	{
		flattened.efx = 0;
		flattened.efxData = 0;
	}

	/* A resolved control-only cell intentionally disappears from the standard
	** XM. Do this check before sub-row translation: otherwise a Zxx/flow command
	** reached by a private Fast Track between master rows is falsely reported as
	** an unsupported sub-row musical event. */
	if (eventIsEmpty(&flattened))
		return;

	/* A tick-resolution bake gives every replayer tick its own ordinary XM row.
	** This is the essential Fast Tracks flattening rule: a 5:1 private head no
	** longer has to squeeze five row crossings into one master-row cell. The
	** finished XM runs these rows at TPL 1, so source F01..F1F speed commands
	** have already done their work and must not override that baked clock. BPM
	** commands (F20..FFF) remain meaningful and are preserved. */
	if (bakeTickResolution && flattened.efx == 0x0F && flattened.efxData < 0x20)
	{
		flattened.efx = 0;
		flattened.efxData = 0;
		if (eventIsEmpty(&flattened))
			return;
	}

	const uint16_t elapsedTick = song.speed >= song.tick ? song.speed - song.tick : 0;
	if (!bakeTickResolution && elapsedTick > 0)
	{
		/* One sub-row note can be represented by EDx. Effects or a delay beyond
		** XM's nibble cannot be translated losslessly by this first milestone. */
		if (elapsedTick > 15 || flattened.note == 0 || flattened.efx != 0)
		{
			bakeUnsupportedSubTicks++;
			return;
		}

		flattened.efx = 0x0E;
		flattened.efxData = 0xD0 | (uint8_t)elapsedTick;
	}

	const int32_t patternIndex = bakeRow / BAKE_PATTERN_ROWS;
	const int32_t patternRow = bakeRow % BAKE_PATTERN_ROWS;
	if (bakePatterns[patternIndex] == NULL)
	{
		bakePatterns[patternIndex] = (note_t *)calloc(BAKE_PATTERN_ROWS * MAX_CHANNELS,
			sizeof (note_t));
		if (bakePatterns[patternIndex] == NULL)
		{
			bakeCollisions++;
			return;
		}
	}

	note_t *destinationRow = &bakePatterns[patternIndex][patternRow * MAX_CHANNELS];
	uint32_t occupiedMask = 0;
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		if (!eventIsEmpty(&destinationRow[i]))
			occupiedMask |= UINT32_C(1) << i;
	}

	/* Repeated copies of the exact same logical event at the exact same XM
	** instant are one voice when duplicate merging is requested. Never merge
	** across different source tracks here: those are kept until the completed
	** channel trajectories can be compared safely. */
	if (bakeMergeExactDuplicates)
	{
		for (int32_t i = 0; i < MAX_CHANNELS; i++)
		{
			if ((occupiedMask & (UINT32_C(1) << i)) != 0 &&
				bakerChannelAllocatorOwner(&bakeAllocator, i) == channelIndex &&
				memcmp(&destinationRow[i], &flattened, sizeof (note_t)) == 0)
			{
				bakeMergedDuplicateEvents++;
				return;
			}
		}
	}

	bool relocated;
	const int32_t destinationChannel = bakerChannelAllocatorRoute(&bakeAllocator,
		channelIndex, occupiedMask, &relocated);
	if (destinationChannel < 0)
	{
		bakeCollisions++;
		return;
	}

	destinationRow[destinationChannel] = flattened;
	if (relocated)
		bakeRelocatedEvents++;
}

static void freeBakePatterns(void)
{
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		free(bakePatterns[i]);
		bakePatterns[i] = NULL;
	}
	bakerAssetsFree();
}

static void resetBakeCapture(bool tickResolution)
{
	sampleLauncherSetCaptureCallback(bakerCaptureSampleLauncherAction);
	freeBakePatterns();
	bakeRow = -1;
	bakeCollisions = 0;
	bakeUnsupportedSubTicks = 0;
	bakeRelocatedEvents = 0;
	bakeMergedDuplicateEvents = 0;
	bakeStrippedMicrotonalCommands = 0;
	bakePreservedMicrotonalCommands = 0;
	bakeOverflow = false;
	bakeSkippedSampleLaunches = 0;
	memset(bakeInvalidSampleTiles, 0, sizeof (bakeInvalidSampleTiles));
	memset(bakeSampleVoiceChannel, -1, sizeof (bakeSampleVoiceChannel));
	bakeTickResolution = tickResolution;
	bakeInitialBPM = song.BPM;
	bakeInitialSpeed = song.speed;
	bakerChannelAllocatorReset(&bakeAllocator, (uint8_t)song.numChannels);
	bakerAudibilitySnapshot(&bakeAudibility, editor.channelMuted,
		performanceMute, (uint8_t)song.numChannels);
}

static bool bakedChannelsIdentical(int32_t channel1, int32_t channel2, int32_t bakedRows)
{
	for (int32_t row = 0; row < bakedRows; row++)
	{
		const int32_t patternIndex = row / BAKE_PATTERN_ROWS;
		const int32_t patternRow = row % BAKE_PATTERN_ROWS;
		if (bakePatterns[patternIndex] == NULL)
			continue;

		const note_t *patternRowPtr =
			&bakePatterns[patternIndex][patternRow * MAX_CHANNELS];
		if (memcmp(&patternRowPtr[channel1], &patternRowPtr[channel2],
			sizeof (note_t)) != 0)
		{
			return false;
		}
	}

	return true;
}

static uint32_t clearBakedChannel(int32_t channelIndex, int32_t bakedRows)
{
	uint32_t clearedEvents = 0;
	for (int32_t row = 0; row < bakedRows; row++)
	{
		const int32_t patternIndex = row / BAKE_PATTERN_ROWS;
		const int32_t patternRow = row % BAKE_PATTERN_ROWS;
		if (bakePatterns[patternIndex] == NULL)
			continue;

		note_t *event = &bakePatterns[patternIndex]
			[patternRow * MAX_CHANNELS + channelIndex];
		if (!eventIsEmpty(event))
		{
			memset(event, 0, sizeof (note_t));
			clearedEvents++;
		}
	}

	return clearedEvents;
}

static int32_t compactBakeChannels(int32_t bakedRows)
{
	bool channelUsed[MAX_CHANNELS];
	memset(channelUsed, 0, sizeof (channelUsed));

	for (int32_t row = 0; row < bakedRows; row++)
	{
		const int32_t patternIndex = row / BAKE_PATTERN_ROWS;
		const int32_t patternRow = row % BAKE_PATTERN_ROWS;
		if (bakePatterns[patternIndex] == NULL)
			continue;

		const note_t *patternRowPtr =
			&bakePatterns[patternIndex][patternRow * MAX_CHANNELS];
		for (int32_t channelIndex = 0; channelIndex < MAX_CHANNELS; channelIndex++)
		{
			if (!eventIsEmpty(&patternRowPtr[channelIndex]))
				channelUsed[channelIndex] = true;
		}
	}

	/* Distinct source tracks are merged only if their entire completed XM
	** trajectories are byte-for-byte identical. This is deliberately stronger
	** than comparing one coincident note and cannot erase a later divergence. */
	if (bakeMergeExactDuplicates)
	{
		for (int32_t channel1 = 0; channel1 < MAX_CHANNELS; channel1++)
		{
			if (!channelUsed[channel1])
				continue;

			for (int32_t channel2 = channel1 + 1; channel2 < MAX_CHANNELS; channel2++)
			{
				if (channelUsed[channel2] &&
					bakedChannelsIdentical(channel1, channel2, bakedRows))
				{
					bakeMergedDuplicateEvents += clearBakedChannel(channel2, bakedRows);
					channelUsed[channel2] = false;
				}
			}
		}
	}

	int8_t channelMap[MAX_CHANNELS];
	memset(channelMap, -1, sizeof (channelMap));
	int32_t outputChannels = 0;
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		if (channelUsed[i])
			channelMap[i] = (int8_t)outputChannels++;
	}

	if (outputChannels == 0)
		outputChannels = 1;

	for (int32_t row = 0; row < bakedRows; row++)
	{
		const int32_t patternIndex = row / BAKE_PATTERN_ROWS;
		const int32_t patternRow = row % BAKE_PATTERN_ROWS;
		if (bakePatterns[patternIndex] == NULL)
			continue;

		note_t compactedRow[MAX_CHANNELS];
		memset(compactedRow, 0, sizeof (compactedRow));
		note_t *patternRowPtr =
			&bakePatterns[patternIndex][patternRow * MAX_CHANNELS];
		for (int32_t oldChannel = 0; oldChannel < MAX_CHANNELS; oldChannel++)
		{
			if (channelMap[oldChannel] >= 0)
				compactedRow[channelMap[oldChannel]] = patternRowPtr[oldChannel];
		}
		memcpy(patternRowPtr, compactedRow, sizeof (compactedRow));
	}

	return outputChannels;
}

static bool allocateLiveBakePatterns(void)
{
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		bakePatterns[i] = (note_t *)calloc(BAKE_PATTERN_ROWS * MAX_CHANNELS,
			sizeof (note_t));
		if (bakePatterns[i] == NULL)
		{
			freeBakePatterns();
			return false;
		}
	}

	return true;
}

static bool saveBakeResult(int32_t bakedRows)
{
	if (bakedRows <= 0 || bakeCollisions != 0 || bakeUnsupportedSubTicks != 0 || bakeOverflow)
		return false;
	if (!bakerAssetsInstall())
		return false;

	song_t savedSong = song;
	note_t *savedPatterns[MAX_PATTERNS];
	note_t *ownedBakePatterns[MAX_PATTERNS];
	int16_t savedPatternRows[MAX_PATTERNS];
	memcpy(savedPatterns, pattern, sizeof (savedPatterns));
	memcpy(ownedBakePatterns, bakePatterns, sizeof (ownedBakePatterns));
	memcpy(savedPatternRows, patternNumRows, sizeof (savedPatternRows));

	const int32_t outputChannels = compactBakeChannels(bakedRows);
	const int32_t patternCount = (bakedRows + BAKE_PATTERN_ROWS - 1) / BAKE_PATTERN_ROWS;
	song.songLength = (uint16_t)patternCount;
	song.songLoopStart = 0;
	song.numChannels = outputChannels;
	song.BPM = bakeInitialBPM;
	song.speed = bakeTickResolution ? 1 : bakeInitialSpeed;
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		pattern[i] = i < patternCount ? bakePatterns[i] : NULL;
		patternNumRows[i] = BAKE_PATTERN_ROWS;
		if (i < patternCount)
			song.orders[i] = (uint8_t)i;
	}
	patternNumRows[patternCount-1] = (int16_t)(((bakedRows - 1) % BAKE_PATTERN_ROWS) + 1);

	const bool saved = bakeOutputTarget == BAKER_OUTPUT_TAPEHEAD_XM ?
		saveXM(bakeFilenameU) : saveStandardXM(bakeFilenameU);

	for (int32_t i = 0; i < MAX_PATTERNS; i++)
		bakePatterns[i] = i < patternCount ? pattern[i] : ownedBakePatterns[i];
	memcpy(pattern, savedPatterns, sizeof (savedPatterns));
	memcpy(patternNumRows, savedPatternRows, sizeof (savedPatternRows));
	song = savedSong;
	bakerAssetsUninstall();
	return saved;
}

static int32_t bakeCompositionThread(void *unused)
{
	(void)unused;
	pauseAudio();

	song_t sourceSong = song;
	channel_t sourceChannels[MAX_CHANNELS];
	memcpy(sourceChannels, channel, sizeof (sourceChannels));
	fastTracksRuntimeState_t sourceFastTracks;
	fastTracksPOCGetRuntimeState(&sourceFastTracks);

	const int16_t sourceEditorSongPos = editor.songPos;
	const int16_t sourceEditorRow = editor.row;
	const uint16_t sourceEditorPattern = editor.editPattern;
	const bool sourceReachedEnd = editor.wavReachedEndFlag;
	const int8_t sourcePlayMode = playMode;
	const bool sourceSongPlaying = songPlaying;

	/* Preserve the proven row-resolution path for an ordinary composition.
	** Once any Fast Track is active, capture every replayer tick so private
	** crossings at ratios other than synchronized 1:1 become writable rows. */
	resetBakeCapture(compositionNeedsTickResolution(&sourceFastTracks));
	bakeState = BAKE_OFFLINE;

	editor.songPos = 0;
	editor.row = 0;
	editor.wavReachedEndFlag = false;
	startPlaying(PLAYMODE_SONG, 0);

	uint32_t ticks = 0;
	while (!editor.wavReachedEndFlag && bakeRow < BAKE_MAX_ROWS &&
		ticks++ < BAKE_TICK_SAFETY_LIMIT)
	{
		tickReplayer();
	}

	bakeState = BAKE_IDLE;
	playMode = PLAYMODE_IDLE;
	songPlaying = false;

	const bool hitSafetyLimit = ticks >= BAKE_TICK_SAFETY_LIMIT || bakeRow >= BAKE_MAX_ROWS;
	const int32_t bakedRows = bakeRow + 1;

	/* Restore every mutable performance state before exposing or saving the
	** flattened copy. The loaded Tapehead composition remains untouched. */
	song = sourceSong;
	memcpy(channel, sourceChannels, sizeof (sourceChannels));
	fastTracksPOCSetRuntimeState(&sourceFastTracks);
	editor.songPos = sourceEditorSongPos;
	editor.row = sourceEditorRow;
	editor.editPattern = sourceEditorPattern;
	editor.wavReachedEndFlag = sourceReachedEnd;
	playMode = sourcePlayMode;
	songPlaying = sourceSongPlaying;

	const bool saved = !hitSafetyLimit && saveBakeResult(bakedRows);
	const bakerAssetError_t assetError = bakerAssetsGetError();

	freeBakePatterns();
	resumeAudio();
	setMouseBusy(false);

	if (hitSafetyLimit)
	{
		okBoxThreadSafe(0, "Bake Module",
			"Bake stopped: the song did not reach its end or exceeded XM's 256-pattern limit.", NULL);
	}
	else if (assetError != BAKER_ASSET_OK)
	{
		const char *message = assetError == BAKER_ASSET_INSTRUMENT_LIMIT
			? "No file was written. Sample Morph needs another private instrument, but XM's 128-instrument limit is exhausted."
			: "No file was written. There was not enough memory to copy a Sample Morph instrument and sample.";
		okBoxThreadSafe(0, "Bake Module", message, NULL);
	}
	else if (bakeCollisions > 0 || bakeUnsupportedSubTicks > 0)
	{
		char message[256];
		snprintf(message, sizeof (message),
			"No file was written. XM could not represent %u event collisions and %u sub-row events losslessly.",
			bakeCollisions, bakeUnsupportedSubTicks);
		okBoxThreadSafe(0, "Bake Module", message, NULL);
	}
	else if (saved)
	{
		char message[256];
		if (bakeOutputTarget == BAKER_OUTPUT_TAPEHEAD_XM)
		{
			snprintf(message, sizeof (message),
				"Tapehead bake complete: %u relocated, %u duplicates merged, %u microtonal commands preserved, %u Sample Matrix launches skipped.",
				bakeRelocatedEvents, bakeMergedDuplicateEvents,
				bakePreservedMicrotonalCommands, bakeSkippedSampleLaunches);
		}
		else
		{
			snprintf(message, sizeof (message),
				"Standard XM bake complete: %u relocated, %u duplicates merged, %u microtonal commands stripped, %u Sample Matrix launches skipped.",
				bakeRelocatedEvents, bakeMergedDuplicateEvents,
				bakeStrippedMicrotonalCommands, bakeSkippedSampleLaunches);
		}
		okBoxThreadSafe(0, "Bake Module", message, NULL);
	}

	return 0;
}

void bakeComposition(UNICHAR *filenameU, bool mergeExactDuplicates,
	bakerOutputTarget_t outputTarget)
{
	if (bakeState != BAKE_IDLE)
		return;

	if (songPlaying)
	{
		okBox(0, "Bake Module", "Stop Song playback before fast baking a composition.", NULL);
		return;
	}
	memset(bakeInvalidSampleTiles, 0, sizeof (bakeInvalidSampleTiles));
	if (!sampleMatrixPreflight())
		return;

	UNICHAR_STRNCPY(bakeFilenameU, filenameU, PATH_MAX);
	bakeFilenameU[PATH_MAX] = '\0';
	bakeMergeExactDuplicates = mergeExactDuplicates;
	bakeOutputTarget = outputTarget;
	bakeState = BAKE_OFFLINE;
	mouseAnimOn();
	bakeThread = SDL_CreateThread(bakeCompositionThread, "composition bake thread", NULL);
	if (bakeThread == NULL)
	{
		bakeState = BAKE_IDLE;
		setMouseBusy(false);
		okBox(0, "Bake Module", "Couldn't create bake thread!", NULL);
		return;
	}

	SDL_DetachThread(bakeThread);
}

void armLiveCompositionBake(UNICHAR *filenameU, bool mergeExactDuplicates,
	bakerOutputTarget_t outputTarget)
{
	if (bakeState != BAKE_IDLE)
	{
		okBox(0, "Live Bake", "Another bake is already active.", NULL);
		return;
	}

	bakeMergeExactDuplicates = mergeExactDuplicates;
	bakeOutputTarget = outputTarget;
	/* Live Bake is specifically allowed to change Fast Tracks state after it is
	** armed, so it always records on the tick-resolution timeline. */
	resetBakeCapture(true);
	if (!sampleMatrixPreflight())
	{
		freeBakePatterns();
		return;
	}
	if (!allocateLiveBakePatterns())
	{
		okBox(0, "Live Bake", "Not enough memory to arm the live baker.", NULL);
		return;
	}

	UNICHAR_STRNCPY(bakeFilenameU, filenameU, PATH_MAX);
	bakeFilenameU[PATH_MAX] = '\0';
	bakeState = BAKE_LIVE_ARMED;
	if (songPlaying)
		bakerPlaybackStarted(playMode);
	else if (patternLauncherHasRouting() || polyMatrixHasAudioWork() ||
		sampleLauncherHasTransportWork())
	{
		bakerBeginManualRow();
		const int16_t qTile = sampleLauncherGetQCurrent();
		if (qTile >= 0)
			bakerCaptureSampleLauncherAction((uint16_t)qTile, 0, true);
		for (uint16_t tile = 0; tile < SAMPLE_LAUNCHER_MAX_TILES; tile++)
		{
			const int8_t slot = sampleLauncherGetPolySlot(tile);
			if (slot >= 0)
				bakerCaptureSampleLauncherAction(tile, (uint8_t)(slot + 1), true);
		}
	}
	okBox(0, "Live Bake", "Armed. Press Play Song/Pattern, or strum without transport. Press Stop to export.", NULL);
}

bool bakerAllowPlaybackStart(int8_t mode)
{
	if (bakeState == BAKE_LIVE_ARMED && mode != PLAYMODE_SONG &&
		mode != PLAYMODE_PATT && mode != PLAYMODE_RECPATT)
	{
		okBox(0, "Live Bake", "Live Bake supports Play Song or Play Pattern. Press Stop to cancel it.", NULL);
		return false;
	}

	return bakeState == BAKE_IDLE || bakeState == BAKE_LIVE_ARMED || bakeState == BAKE_OFFLINE;
}

void bakerPlaybackStarted(int8_t mode)
{
	if (bakeState == BAKE_LIVE_ARMED && (mode == PLAYMODE_SONG ||
		mode == PLAYMODE_PATT || mode == PLAYMODE_RECPATT))
	{
		bakeInitialBPM = song.BPM;
		bakeInitialSpeed = song.speed;
		bakeState = BAKE_LIVE_CAPTURING;
		showRecPlusOverlay("LIVE BAKE");
	}
}

void bakerFinishOrCancelLive(void)
{
	if (bakeState == BAKE_LIVE_ARMED)
	{
		bakeState = BAKE_IDLE;
		freeBakePatterns();
		okBox(0, "Live Bake", "Live Bake cancelled; no file was written.", NULL);
		return;
	}

	if (bakeState != BAKE_LIVE_CAPTURING)
	{
		if (bakeState != BAKE_PERFORMANCE_CAPTURING)
			return;
		const int32_t elapsedRow = performanceElapsedRow();
		if (elapsedRow > bakeRow)
			bakeRow = elapsedRow;
		if (bakeRow >= BAKE_MAX_ROWS)
			bakeOverflow = true;
	}

	bakeState = BAKE_LIVE_SAVING;
	mouseAnimOn();
	pauseAudio();

	const int32_t bakedRows = bakeRow + 1;
	const bool saved = saveBakeResult(bakedRows);
	const bakerAssetError_t assetError = bakerAssetsGetError();
	const bool overflow = bakeOverflow;
	const uint32_t collisions = bakeCollisions;
	const uint32_t unsupportedSubTicks = bakeUnsupportedSubTicks;

	freeBakePatterns();
	resumeAudio();
	setMouseBusy(false);
	bakeState = BAKE_IDLE;

	if (overflow)
	{
		okBox(0, "Live Bake", "No file was written. The performance exceeded XM's 256-pattern limit.", NULL);
	}
	else if (assetError != BAKER_ASSET_OK)
	{
		const char *message = assetError == BAKER_ASSET_INSTRUMENT_LIMIT
			? "No file was written. Sample Morph needs another private instrument, but XM's 128-instrument limit is exhausted."
			: "No file was written. There was not enough memory to copy a Sample Morph instrument and sample.";
		okBox(0, "Live Bake", message, NULL);
	}
	else if (collisions > 0 || unsupportedSubTicks > 0)
	{
		char message[256];
		snprintf(message, sizeof (message),
			"No file was written. XM could not represent %u event collisions and %u sub-row events losslessly.",
			collisions, unsupportedSubTicks);
		okBox(0, "Live Bake", message, NULL);
	}
	else if (bakedRows <= 0)
	{
		okBox(0, "Live Bake", "No file was written because no song rows were captured.", NULL);
	}
	else if (saved)
	{
		char message[256];
		if (bakeOutputTarget == BAKER_OUTPUT_TAPEHEAD_XM)
		{
			snprintf(message, sizeof (message),
				"Tapehead live bake complete: %u relocated, %u duplicates merged, %u microtonal commands preserved, %u Sample Matrix launches skipped.",
				bakeRelocatedEvents, bakeMergedDuplicateEvents,
				bakePreservedMicrotonalCommands, bakeSkippedSampleLaunches);
		}
		else
		{
			snprintf(message, sizeof (message),
				"Standard XM live bake complete: %u relocated, %u duplicates merged, %u microtonal commands stripped, %u Sample Matrix launches skipped.",
				bakeRelocatedEvents, bakeMergedDuplicateEvents,
				bakeStrippedMicrotonalCommands, bakeSkippedSampleLaunches);
		}
		okBox(0, "Live Bake", message, NULL);
	}
}
