#include "ft2_tapehead_actions.h"
#include <string.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_baker.h"
#include "ft2_config.h"
#include "ft2_fasttracks.h"
#include "ft2_pattern_ed.h"
#include "ft2_pattern_launcher.h"
#include "ft2_pattern_launcher_ui.h"
#include "ft2_poly_matrix.h"
#include "ft2_replayer.h"
#include "ft2_sample_launcher.h"
#include "ft2_sample_morph.h"
#include "ft2_structs.h"
#include "ft2_undo.h"
#include "scopes/ft2_scopes.h"

#define PATTERN_JOG_VISUAL_HOLD_MS 120

static uint8_t revealHistory[MAX_CHANNELS];
static uint8_t revealHistoryCount;
static bool shiftModifierHeld, matrixGridPoly, transportPatternMode;
static bool soloRestore[MAX_CHANNELS];
static int8_t performanceSoloTrack = -1;
static int8_t recordArmTrack = -1;
static bool jogAuditionHeld[MAX_CHANNELS];
static uint8_t jogAuditionInstrument[MAX_CHANNELS];
static bool jogVisualActive;
static uint32_t jogVisualLastTick;
static uint16_t jogVisualPattern, jogVisualRow;
static volatile bool transportPunchFrozen;
static bool transportPunchPedalDown, transportPunchPedalLatched;
static bool transportPunchKeyboardLatched;
static bool transportPunchConsumedRow;

typedef struct matrixSequence_t
{
	bool active;
	tapeheadMatrixTarget_t target;
	tapeheadMatrixSequenceType_t type;
	uint8_t bank, controlIndex, count, next;
	uint16_t item[32];
} matrixSequence_t;

static matrixSequence_t matrixSequence;
static uint16_t matrixMasterGain = 256;
static uint8_t matrixCrossfader = 64;
static uint16_t matrixQGain = 256, matrixPolyGain = 256;

static void emitJogRow(uint8_t patternNum, uint16_t row, int32_t direction,
	bool forceAudition);

static bool channelIndexIsActive(int32_t channelIndex)
{
	return channelIndex >= 0 && channelIndex < song.numChannels &&
		channelIndex < MAX_CHANNELS;
}

static void removeRevealHistoryChannel(int32_t channelIndex)
{
	for (uint8_t i = 0; i < revealHistoryCount;)
	{
		if (revealHistory[i] != channelIndex)
		{
			i++;
			continue;
		}

		for (uint8_t j = i + 1; j < revealHistoryCount; j++)
			revealHistory[j-1] = revealHistory[j];
		revealHistoryCount--;
	}
}

static void setPerformanceMute(int32_t channelIndex, bool muted)
{
	performanceMute[channelIndex] = muted;
	channel[channelIndex].status |= CS_UPDATE_VOL | CS_USE_QUICK_VOLRAMP;
	redrawScopeChannel(channelIndex);
}

bool tapeheadActionTrackPerformanceMuteToggle(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex))
		return false;

	setPerformanceMute(channelIndex, !performanceMute[channelIndex]);
	if (performanceMute[channelIndex])
		removeRevealHistoryChannel(channelIndex);
	return true;
}

bool tapeheadActionPerformanceMuteAll(void)
{
	bool changed = false;
	performanceSoloTrack = -1;
	for (int32_t i = 0; i < song.numChannels && i < MAX_CHANNELS; i++)
	{
		if (performanceMute[i])
			continue;
		setPerformanceMute(i, true);
		changed = true;
	}
	revealHistoryCount = 0;
	return changed;
}

bool tapeheadActionPerformanceUnmuteAll(void)
{
	bool changed = false;

	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		if (!performanceMute[i])
			continue;

		if (i < song.numChannels)
			setPerformanceMute(i, false);
		else
			performanceMute[i] = false;
		changed = true;
	}
	revealHistoryCount = 0;
	performanceSoloTrack = -1;

	return changed;
}

bool tapeheadActionPerformanceMuteMaster(void)
{
	bool allMuted = song.numChannels > 0;
	for (int32_t i = 0; i < song.numChannels && i < 8; i++)
		allMuted &= performanceMute[i];

	bool changed = false;
	const bool muted = !allMuted;
	for (int32_t i = 0; i < song.numChannels && i < 8; i++)
	{
		if (performanceMute[i] == muted)
			continue;
		setPerformanceMute(i, muted);
		changed = true;
	}
	performanceSoloTrack = -1;
	revealHistoryCount = 0;
	return changed;
}

bool tapeheadActionTrackPerformanceSoloToggle(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex))
		return false;

	if (performanceSoloTrack == channelIndex)
	{
		for (int32_t i = 0; i < song.numChannels && i < MAX_CHANNELS; i++)
			setPerformanceMute(i, soloRestore[i]);
		performanceSoloTrack = -1;
		return true;
	}

	if (performanceSoloTrack < 0)
	{
		for (int32_t i = 0; i < MAX_CHANNELS; i++)
			soloRestore[i] = performanceMute[i];
	}

	for (int32_t i = 0; i < song.numChannels && i < MAX_CHANNELS; i++)
		setPerformanceMute(i, i != channelIndex);
	performanceSoloTrack = (int8_t)channelIndex;
	revealHistoryCount = 0;
	return true;
}

int32_t tapeheadActionGetPerformanceSoloTrack(void)
{
	return performanceSoloTrack;
}

bool tapeheadActionPerformanceUnmuteNext(void)
{
	for (int32_t i = 0; i < song.numChannels && i < MAX_CHANNELS; i++)
	{
		/* An ordinary mute still owns silence. Skipping it ensures that every
		** reveal step adds an actually audible performance layer. */
		if (!performanceMute[i] || editor.channelMuted[i])
			continue;

		setPerformanceMute(i, false);
		removeRevealHistoryChannel(i);
		revealHistory[revealHistoryCount++] = (uint8_t)i;
		return true;
	}

	return false;
}

bool tapeheadActionPerformanceMutePrevious(void)
{
	while (revealHistoryCount > 0)
	{
		const int32_t channelIndex = revealHistory[--revealHistoryCount];
		if (!channelIndexIsActive(channelIndex) || performanceMute[channelIndex])
			continue;

		setPerformanceMute(channelIndex, true);
		return true;
	}

	return false;
}

uint8_t tapeheadActionPerformanceGetRevealHistoryCount(void)
{
	return revealHistoryCount;
}

bool tapeheadActionTrackMuteToggle(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex))
		return false;

	editor.channelMuted[channelIndex] = !editor.channelMuted[channelIndex];
	setChannelMute(channelIndex, editor.channelMuted[channelIndex]);
	redrawScopeChannel(channelIndex);
	return true;
}

bool tapeheadActionUnmuteAll(void)
{
	bool changed = tapeheadActionPerformanceUnmuteAll();
	for (int32_t i = 0; i < song.numChannels && i < MAX_CHANNELS; i++)
	{
		if (!editor.channelMuted[i])
			continue;

		editor.channelMuted[i] = false;
		setChannelMute(i, false);
		redrawScopeChannel(i);
		changed = true;
	}

	return changed;
}

bool tapeheadActionTrackTrimSet(int32_t channelIndex, int32_t trim)
{
	if (!channelIndexIsActive(channelIndex))
		return false;

	const uint16_t ceiling = tapeheadTrackTrimCeiling(
		tapeheadConfig.trackTrimMaxPercent);
	const uint16_t clampedTrim = tapeheadTrackTrimClamp(trim, ceiling);
	if (channelVolumeTrim[channelIndex] == clampedTrim)
		return false;

	channelVolumeTrim[channelIndex] = clampedTrim;
	channel[channelIndex].status |= CS_UPDATE_VOL;
	redrawScopeChannel(channelIndex);
	return true;
}

bool tapeheadActionTrackSelect(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex))
		return false;

	const uint8_t oldChannel = cursor.ch;
	jumpToChannel((uint8_t)channelIndex);
	return cursor.ch != oldChannel;
}

bool tapeheadActionTrackRecordArm(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex))
		return false;

	if (recordArmTrack == channelIndex && playMode == PLAYMODE_RECPATT)
	{
		playMode = PLAYMODE_PATT;
		recordArmTrack = -1;
		ui.updatePosSections = true;
		return true;
	}

	tapeheadActionTrackSelect(channelIndex);
	if (playMode != PLAYMODE_RECPATT)
		pbRecPtn();
	recordArmTrack = (int8_t)channelIndex;
	return true;
}

int32_t tapeheadActionGetRecordArmTrack(void)
{
	return recordArmTrack;
}

bool tapeheadActionCursorLeft(void) { cursorTabLeft(); return true; }
bool tapeheadActionCursorRight(void) { cursorTabRight(); return true; }
bool tapeheadActionCursorUp(void)
{
	if (songPlaying) return false;
	rowOneUpWrap();
	return true;
}
bool tapeheadActionCursorDown(void)
{
	if (songPlaying) return false;
	rowOneDownWrap();
	return true;
}
static bool navigateSongOrder(int32_t direction)
{
	if (direction == 0 || song.songLength == 0)
		return false;

	int32_t distance = shiftModifierHeld ? editor.editRowSkip : 1;
	if (distance < 1)
		distance = 1;
	int32_t target = editor.songPos + (direction < 0 ? -distance : distance);
	if (target < 0) target = 0;
	if (target >= song.songLength) target = song.songLength - 1;
	if (target == editor.songPos)
		return false;

	if (transportPunchFrozen)
		tapeheadReplayerSetTransportPunchSongPos(target);
	else
		setNewSongPos(target);
	if (transportPunchFrozen)
	{
		transportPunchConsumedRow = false;
		if (tapeheadConfig.transportFreezeNavigationAudition)
		{
			/* auditionJogRow() is defined below. Defer through the shared row
			** emitter so Live Bake and microtonal state see the same event. */
			emitJogRow((uint8_t)song.pattNum, (uint16_t)song.row, direction,
				true);
		}
	}
	return true;
}

bool tapeheadActionSongOrderPrevious(void) { return navigateSongOrder(-1); }
bool tapeheadActionSongOrderNext(void) { return navigateSongOrder(1); }

void tapeheadActionSetShiftModifier(bool held)
{
	shiftModifierHeld = held;
}

bool tapeheadActionShiftModifierIsHeld(void)
{
	return shiftModifierHeld;
}

bool tapeheadActionMasterVolumeSet(int32_t volume)
{
	if (volume < 0) volume = 0;
	if (volume > 256) volume = 256;
	if (config.masterVol == volume)
		return false;
	config.masterVol = (int16_t)volume;
	setAudioAmp(config.boostLevel, config.masterVol,
		!!(config.specialFlags & BITDEPTH_32));
	return true;
}

bool tapeheadActionTempoAdjust(int32_t delta)
{
	const uint16_t oldBPM = song.BPM;
	while (delta > 0) { pbBPMUp(); delta--; }
	while (delta < 0) { pbBPMDown(); delta++; }
	return song.BPM != oldBPM;
}

bool tapeheadActionSpeedAdjust(int32_t delta)
{
	const uint16_t oldSpeed = song.speed;
	while (delta > 0) { pbSpeedUp(); delta--; }
	while (delta < 0) { pbSpeedDown(); delta++; }
	return song.speed != oldSpeed;
}

bool tapeheadActionFastTrackToggle(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex))
		return false;

	const bool oldState = fastTracksPOCIsSelected(channelIndex);
	fastTracksPOCSetTrackEnabled(channelIndex, !oldState);
	return fastTracksPOCIsSelected(channelIndex) != oldState;
}

bool tapeheadActionFastTrackMasterToggle(void)
{
	const bool oldState = fastTracksPOCMasterIsEnabled();
	fastTracksPOCSetMasterEnabled(!oldState);
	return fastTracksPOCMasterIsEnabled() != oldState;
}

bool tapeheadActionFastTrackTransmissionClutchToggle(void)
{
	const bool oldState = fastTracksPOCTransmissionClutchIsLatched();
	fastTracksPOCTransmissionClutchToggle();
	return fastTracksPOCTransmissionClutchIsLatched() != oldState;
}

bool tapeheadActionFastTrackGlobalReverseToggle(void)
{
	bool hasSelected = false;
	bool targetReverse = false;
	for (int32_t i = 0; i < song.numChannels && i < 8; i++)
	{
		if (!fastTracksPOCIsSelected(i))
			continue;
		hasSelected = true;
		if (!fastTracksPOCIsReversed(i))
			targetReverse = true;
	}
	if (!hasSelected)
		return false;

	for (int32_t i = 0; i < song.numChannels && i < 8; i++)
	{
		if (fastTracksPOCIsSelected(i) &&
			fastTracksPOCIsReversed(i) != targetReverse)
		{
			fastTracksPOCToggleDirection(i);
		}
	}
	return true;
}

uint8_t tapeheadActionFastTrackGlobalModeState(void)
{
	bool hasPattern = false, hasSong = false;
	for (int32_t i = 0; i < song.numChannels && i < MAX_CHANNELS; i++)
	{
		if (!fastTracksPOCIsSelected(i))
			continue;
		if (fastTracksPOCGetMode(i) == FAST_TRACKS_MODE_SONG)
			hasSong = true;
		else
			hasPattern = true;
	}
	if (hasPattern && hasSong) return 2;
	return hasSong ? 1 : 0;
}

bool tapeheadActionFastTrackGlobalModeToggle(void)
{
	bool hasSelected = false, allSong = true;
	for (int32_t i = 0; i < song.numChannels && i < MAX_CHANNELS; i++)
	{
		if (!fastTracksPOCIsSelected(i))
			continue;
		hasSelected = true;
		allSong &= fastTracksPOCGetMode(i) == FAST_TRACKS_MODE_SONG;
	}
	if (!hasSelected)
		return false;

	const fastTracksMode_t target = allSong ? FAST_TRACKS_MODE_PATTERN :
		FAST_TRACKS_MODE_SONG;
	for (int32_t i = 0; i < song.numChannels && i < MAX_CHANNELS; i++)
	{
		if (fastTracksPOCIsSelected(i))
			fastTracksPOCSetMode(i, target);
	}
	return true;
}

bool tapeheadActionFastTrackDirectionOrSongMode(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex) ||
		!fastTracksPOCIsSelected(channelIndex))
	{
		return false;
	}

	if (!shiftModifierHeld)
		return tapeheadActionFastTrackReverseToggle(channelIndex);

	const fastTracksMode_t oldMode = fastTracksPOCGetMode(channelIndex);
	fastTracksPOCSetMode(channelIndex, oldMode == FAST_TRACKS_MODE_SONG
		? FAST_TRACKS_MODE_PATTERN : FAST_TRACKS_MODE_SONG);
	return fastTracksPOCGetMode(channelIndex) != oldMode;
}

static bool stepAllRatios(int32_t delta)
{
	bool changed = false;
	for (int32_t i = 0; i < song.numChannels && i < 8; i++)
	{
		if (!fastTracksPOCIsSelected(i))
			continue;
		changed |= delta > 0 ? tapeheadActionFastTrackRatioNext(i) :
			tapeheadActionFastTrackRatioPrevious(i);
	}
	return changed;
}

bool tapeheadActionFastTrackRatioAllNext(void) { return stepAllRatios(1); }
bool tapeheadActionFastTrackRatioAllPrevious(void) { return stepAllRatios(-1); }

bool tapeheadActionFastTrackRatioAllOrMatrixBankNext(void)
{
	return shiftModifierHeld ? tapeheadActionMatrixBankNext() :
		tapeheadActionFastTrackRatioAllNext();
}

bool tapeheadActionFastTrackRatioAllOrMatrixBankPrevious(void)
{
	return shiftModifierHeld ? tapeheadActionMatrixBankPrevious() :
		tapeheadActionFastTrackRatioAllPrevious();
}

bool tapeheadActionFastTrackRatioSet(int32_t channelIndex, int32_t ratioIndex)
{
	if (!channelIndexIsActive(channelIndex) || ratioIndex < 0 ||
		ratioIndex >= fastTracksPOCGetRatioCount())
	{
		return false;
	}

	const uint8_t oldIndex = fastTracksPOCGetRatioIndex(channelIndex);
	fastTracksPOCSetRatioIndex(channelIndex, (uint8_t)ratioIndex);
	return fastTracksPOCGetRatioIndex(channelIndex) != oldIndex;
}

uint16_t tapeheadActionTrackLengthFromController(uint8_t value)
{
	const uint16_t maximum = CLAMP(tapeheadConfig.trackLengthControlMax, 1,
		MAX_PATT_LEN);
	if (value == 0)
		return 0; /* the absolute encoder's bottom stop is LEN OFF */
	if (maximum == 1)
		return 1;

	/* Reserve CC zero for OFF, then distribute 1..127 across 1..maximum.
	** Lowering TrackLengthControlMax therefore improves useful resolution. */
	return (uint16_t)(1 + ((((uint32_t)value - 1) * (maximum - 1) + 63) /
		126));
}

bool tapeheadActionTrackLengthSet(int32_t channelIndex, uint16_t length)
{
	if (!channelIndexIsActive(channelIndex))
		return false;

	length = MIN(length, MAX_PATT_LEN);
	const uint16_t oldLength = fastTracksPOCGetTrackLength(editor.editPattern,
		channelIndex);
	if (oldLength == length)
		return false;
	if (!undoPatternBegin(editor.editPattern, "Set track length"))
		return false;

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();
	fastTracksPOCSetTrackLength(editor.editPattern, channelIndex, length);
	setSongModifiedFlag();
	undoPatternCommit();
	if (audioWasntLocked)
		unlockAudio();

	ui.updatePatternEditor = true;
	return true;
}

bool tapeheadActionFastTrackRatioNext(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex))
		return false;

	const int32_t oldIndex = fastTracksPOCGetRatioIndex(channelIndex);
	const int32_t lastIndex = fastTracksPOCGetRatioCount() - 1;
	return tapeheadActionFastTrackRatioSet(channelIndex,
		oldIndex < lastIndex ? oldIndex + 1 : lastIndex);
}

bool tapeheadActionFastTrackRatioPrevious(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex))
		return false;

	const int32_t oldIndex = fastTracksPOCGetRatioIndex(channelIndex);
	return tapeheadActionFastTrackRatioSet(channelIndex,
		oldIndex > 0 ? oldIndex - 1 : 0);
}

bool tapeheadActionFastTrackRatioReset(int32_t channelIndex)
{
	return tapeheadActionFastTrackRatioSet(channelIndex,
		FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX);
}

bool tapeheadActionFastTrackResetAll(void)
{
	/* APC40 mkII Device Lock keeps its ordinary reset on the unshifted press;
	** Shift turns the same physical control into the global LEN clutch. */
	if (shiftModifierHeld)
		return tapeheadActionTrackLengthBypassToggle();

	bool hasSelectedTrack = false;
	for (int32_t i = 0; i < song.numChannels && i < MAX_CHANNELS; i++)
		hasSelectedTrack |= fastTracksPOCIsSelected(i);

	if (!hasSelectedTrack)
		return false;

	fastTracksPOCResetAllRatios();
	return true;
}

bool tapeheadActionTrackLengthBypassToggle(void)
{
	const bool oldState = fastTracksPOCLengthTopologyIsBypassed();
	fastTracksPOCToggleLengthTopologyBypass();
	return fastTracksPOCLengthTopologyIsBypassed() != oldState;
}

bool tapeheadActionFastTrackReverseToggle(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex) ||
		!fastTracksPOCIsSelected(channelIndex))
	{
		return false;
	}

	const bool oldState = fastTracksPOCIsReversed(channelIndex);
	fastTracksPOCToggleDirection(channelIndex);
	return fastTracksPOCIsReversed(channelIndex) != oldState;
}

bool tapeheadActionFastTrackClutchToggle(int32_t channelIndex)
{
	if (!channelIndexIsActive(channelIndex) ||
		!fastTracksPOCIsSelected(channelIndex))
	{
		return false;
	}

	const bool oldState = fastTracksPOCIsClutched(channelIndex);
	fastTracksPOCSetClutch(channelIndex, !oldState);
	return fastTracksPOCIsClutched(channelIndex) != oldState;
}

tapeheadMatrixTarget_t tapeheadActionMatrixGetTarget(void)
{
	return patternLauncherDeckIsSample() ? TAPEHEAD_MATRIX_SAMPLE :
		TAPEHEAD_MATRIX_PATTERN;
}

bool tapeheadActionMatrixSetTarget(tapeheadMatrixTarget_t target)
{
	if (target != TAPEHEAD_MATRIX_PATTERN && target != TAPEHEAD_MATRIX_SAMPLE)
		return false;

	const tapeheadMatrixTarget_t oldTarget = tapeheadActionMatrixGetTarget();
	patternLauncherSetDeckMode(target == TAPEHEAD_MATRIX_SAMPLE);
	return target != oldTarget;
}

bool tapeheadActionMatrixToggleTarget(void)
{
	return tapeheadActionMatrixSetTarget(tapeheadActionMatrixGetTarget() ==
		TAPEHEAD_MATRIX_PATTERN ? TAPEHEAD_MATRIX_SAMPLE :
		TAPEHEAD_MATRIX_PATTERN);
}

uint8_t tapeheadActionMatrixGetBank(tapeheadMatrixTarget_t target)
{
	return target == TAPEHEAD_MATRIX_SAMPLE ? sampleLauncherGetBank() :
		patternLauncherGetPage();
}

bool tapeheadActionMatrixBankSelect(uint8_t bank)
{
	if (bank >= 8)
		return false;

	const tapeheadMatrixTarget_t target = tapeheadActionMatrixGetTarget();
	if (tapeheadActionMatrixGetBank(target) == bank)
		return false;

	if (target == TAPEHEAD_MATRIX_SAMPLE)
		sampleLauncherSetBank(bank);
	else
		patternLauncherSetPage(bank);
	patternLauncherForceRedraw();
	return true;
}

bool tapeheadActionMatrixLayerBankSelect(uint8_t bank)
{
	if (bank >= 8)
		return false;

	const tapeheadMatrixTarget_t target = shiftModifierHeld
		? TAPEHEAD_MATRIX_SAMPLE : TAPEHEAD_MATRIX_PATTERN;
	const bool targetChanged = tapeheadActionMatrixSetTarget(target);
	return tapeheadActionMatrixBankSelect(bank) || targetChanged;
}

bool tapeheadActionMatrixBankNext(void)
{
	const tapeheadMatrixTarget_t target = tapeheadActionMatrixGetTarget();
	const uint8_t bank = tapeheadActionMatrixGetBank(target);
	return bank < 7 && tapeheadActionMatrixBankSelect(bank + 1);
}

bool tapeheadActionMatrixBankPrevious(void)
{
	const tapeheadMatrixTarget_t target = tapeheadActionMatrixGetTarget();
	const uint8_t bank = tapeheadActionMatrixGetBank(target);
	return bank > 0 && tapeheadActionMatrixBankSelect(bank - 1);
}

static void clearSequenceQueue(void)
{
	if (matrixSequence.target == TAPEHEAD_MATRIX_SAMPLE)
		sampleLauncherClearQQueue();
	else
		patternLauncherClearQueue();
}

void tapeheadActionMatrixSequenceCancel(void)
{
	if (!matrixSequence.active)
		return;
	clearSequenceQueue();
	memset(&matrixSequence, 0, sizeof (matrixSequence));
	patternLauncherForceRedraw();
}

static bool matrixItemIsAvailable(tapeheadMatrixTarget_t target,
	uint16_t item)
{
	if (target == TAPEHEAD_MATRIX_SAMPLE)
		return sampleLauncherTileIsLoaded(item);
	return item < MAX_PATTERNS &&
		patternLauncherTileIsLaunchable((uint8_t)item);
}

static bool queueNextSequenceItem(void)
{
	while (matrixSequence.next < matrixSequence.count)
	{
		const uint16_t item = matrixSequence.item[matrixSequence.next++];
		if (!matrixItemIsAvailable(matrixSequence.target, item))
			continue;

		if (matrixSequence.target == TAPEHEAD_MATRIX_SAMPLE)
		{
			if (!sampleLauncherRequestQ(item))
				continue;
		}
		else
		{
			patternLauncherRequest((uint8_t)item, false, false);
		}
		return true;
	}

	return false;
}

static bool startMatrixSequence(tapeheadMatrixSequenceType_t type,
	uint8_t controlIndex)
{
	const tapeheadMatrixTarget_t target = tapeheadActionMatrixGetTarget();
	if (target == TAPEHEAD_MATRIX_PATTERN)
		(void)tapeheadActionPatternJogStopAudition();
	const uint8_t bank = tapeheadActionMatrixGetBank(target);
	if (matrixSequence.active && matrixSequence.target == target &&
		matrixSequence.bank == bank && matrixSequence.type == type &&
		matrixSequence.controlIndex == controlIndex)
	{
		tapeheadActionMatrixSequenceCancel();
		return true;
	}

	if (matrixSequence.active)
		clearSequenceQueue();
	if (target == TAPEHEAD_MATRIX_SAMPLE)
		sampleLauncherClearQQueue();
	else
		patternLauncherClearQueue();

	memset(&matrixSequence, 0, sizeof (matrixSequence));
	matrixSequence.active = true;
	matrixSequence.target = target;
	matrixSequence.type = type;
	matrixSequence.bank = bank;
	matrixSequence.controlIndex = controlIndex;

	if (type == TAPEHEAD_MATRIX_SEQUENCE_ROW)
	{
		if (controlIndex >= 4) goto fail;
		for (uint8_t column = 0; column < 8; column++)
			matrixSequence.item[matrixSequence.count++] =
				(uint16_t)((bank * 32) + (controlIndex * 8) + column);
	}
	else if (type == TAPEHEAD_MATRIX_SEQUENCE_COLUMN)
	{
		if (controlIndex >= 8) goto fail;
		for (uint8_t row = 0; row < 4; row++)
			matrixSequence.item[matrixSequence.count++] =
				(uint16_t)((bank * 32) + (row * 8) + controlIndex);
	}
	else if (type == TAPEHEAD_MATRIX_SEQUENCE_BANK)
	{
		for (uint8_t localSlot = 0; localSlot < 32; localSlot++)
			matrixSequence.item[matrixSequence.count++] =
				(uint16_t)((bank * 32) + localSlot);
	}
	else
	{
		goto fail;
	}

	/* Remove unavailable cells before the sequence starts. This lets one
	** controller press remain deterministic even if the bank is edited later. */
	uint8_t write = 0;
	for (uint8_t read = 0; read < matrixSequence.count; read++)
	{
		if (matrixItemIsAvailable(target, matrixSequence.item[read]))
			matrixSequence.item[write++] = matrixSequence.item[read];
	}
	matrixSequence.count = write;
	if (matrixSequence.count == 0 || !queueNextSequenceItem())
		goto fail;

	patternLauncherForceRedraw();
	return true;

fail:
	memset(&matrixSequence, 0, sizeof (matrixSequence));
	patternLauncherForceRedraw();
	return false;
}

bool tapeheadActionMatrixSequenceRow(uint8_t row)
{
	return startMatrixSequence(TAPEHEAD_MATRIX_SEQUENCE_ROW, row);
}

bool tapeheadActionMatrixSequenceColumn(uint8_t column)
{
	return startMatrixSequence(TAPEHEAD_MATRIX_SEQUENCE_COLUMN, column);
}

bool tapeheadActionMatrixSequenceBank(void)
{
	return startMatrixSequence(TAPEHEAD_MATRIX_SEQUENCE_BANK, 0);
}

void tapeheadActionMatrixSequenceHandleBoundary(void)
{
	if (!matrixSequence.active)
		return;

	if (!queueNextSequenceItem())
		matrixSequence.active = false;
	patternLauncherForceRedraw();
}

bool tapeheadActionMatrixSequenceIsActive(void)
{
	return matrixSequence.active;
}

tapeheadMatrixSequenceType_t tapeheadActionMatrixSequenceGetType(void)
{
	return matrixSequence.active ? matrixSequence.type :
		TAPEHEAD_MATRIX_SEQUENCE_NONE;
}

int32_t tapeheadActionMatrixSequenceGetControlIndex(void)
{
	return matrixSequence.active ? matrixSequence.controlIndex : -1;
}

bool tapeheadActionMatrixSequenceSlotIsPending(uint8_t localSlot)
{
	if (!matrixSequence.active || localSlot >= 32 ||
		matrixSequence.target != tapeheadActionMatrixGetTarget() ||
		matrixSequence.bank != tapeheadActionMatrixGetBank(matrixSequence.target))
	{
		return false;
	}

	const uint16_t absolute = (uint16_t)((matrixSequence.bank * 32) + localSlot);
	const uint8_t first = matrixSequence.next > 0 ? matrixSequence.next - 1 : 0;
	for (uint8_t i = first; i < matrixSequence.count; i++)
	{
		if (matrixSequence.item[i] == absolute)
			return true;
	}
	return false;
}

bool tapeheadActionMatrixSlotTrigger(uint8_t localSlot)
{
	if (localSlot >= 32)
		return false;
	if (shiftModifierHeld)
		return tapeheadActionMatrixSlotScheduleStop(localSlot);

	tapeheadActionMatrixSequenceCancel();

	bool changed;
	if (tapeheadActionMatrixGetTarget() == TAPEHEAD_MATRIX_SAMPLE)
	{
		const uint16_t tile = (sampleLauncherGetBank() * 32) + localSlot;
		changed = matrixGridPoly ? sampleLauncherTogglePoly(tile) :
			sampleLauncherRequestQ(tile);
	}
	else
	{
		const uint8_t patternNum =
			(uint8_t)((patternLauncherGetPage() * 32) + localSlot);
		if (!patternLauncherTileIsLaunchable(patternNum))
			return false;
		if (matrixGridPoly)
			changed = polyMatrixTogglePattern(patternNum, false);
		else
		{
			(void)tapeheadActionPatternJogStopAudition();
			patternLauncherRequest(patternNum, false, false);
			changed = true;
		}
	}

	if (changed)
		patternLauncherForceRedraw();
	return changed;
}

bool tapeheadActionMatrixSlotScheduleStop(uint8_t localSlot)
{
	if (localSlot >= 32)
		return false;

	/* Automatic Q material must not be able to re-launch a tile after its
	** universal Shift+pad stop has been armed. */
	tapeheadActionMatrixSequenceCancel();

	bool changed = false;
	if (tapeheadActionMatrixGetTarget() == TAPEHEAD_MATRIX_SAMPLE)
	{
		const uint16_t tile = (sampleLauncherGetBank() * 32) + localSlot;
		changed = sampleLauncherScheduleStop(tile);
	}
	else
	{
		const uint8_t patternNum =
			(uint8_t)((patternLauncherGetPage() * 32) + localSlot);
		changed = patternLauncherScheduleStop(patternNum);
		changed |= polyMatrixSchedulePatternStop(patternNum);
	}

	if (changed)
		patternLauncherForceRedraw();
	return changed;
}

bool tapeheadActionMatrixGridModeToggle(void)
{
	matrixGridPoly ^= 1;
	return true;
}

bool tapeheadActionMatrixGridIsPoly(void)
{
	return matrixGridPoly;
}

bool tapeheadActionMatrixVisibilityToggle(void)
{
	patternLauncherSetStandaloneShown(!patternLauncherStandaloneIsShown());
	return true;
}

static void updateMatrixMixerGains(void)
{
	uint16_t qCross, polyCross;
	if (matrixCrossfader <= 64)
	{
		qCross = 256;
		polyCross = (uint16_t)(matrixCrossfader * 4);
	}
	else
	{
		qCross = (uint16_t)(((127 - matrixCrossfader) * 256 + 31) / 63);
		polyCross = 256;
	}

	matrixQGain = (uint16_t)((matrixMasterGain * qCross + 128) / 256);
	matrixPolyGain = (uint16_t)((matrixMasterGain * polyCross + 128) / 256);
	audioSetMatrixMixerGains(matrixQGain, matrixPolyGain);
}

bool tapeheadActionMatrixMasterVolumeSet(int32_t value)
{
	if (value < 0) value = 0;
	if (value > 256) value = 256;
	if (matrixMasterGain == value)
		return false;
	matrixMasterGain = (uint16_t)value;
	updateMatrixMixerGains();
	return true;
}

bool tapeheadActionMatrixCrossfaderSet(int32_t value)
{
	if (value < 0) value = 0;
	if (value > 127) value = 127;
	if (matrixCrossfader == value)
		return false;
	matrixCrossfader = (uint8_t)value;
	updateMatrixMixerGains();
	return true;
}

uint16_t tapeheadActionMatrixGetQGain(void) { return matrixQGain; }
uint16_t tapeheadActionMatrixGetPolyGain(void) { return matrixPolyGain; }

bool tapeheadActionPatternJogStopAudition(void)
{
	bool changed = false;
	if (jogVisualActive)
	{
		jogVisualActive = false;
		ui.updatePatternEditor = true;
	}
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		if (!jogAuditionHeld[i])
			continue;
		/* A Poly tunnel may have claimed this destination after the jog note
		** began. Its own event has already replaced that voice, so do not send a
		** late note-off into independently running Poly material. */
		if (!polyMatrixOwnsDestination(i))
		{
			playTone((uint8_t)i, 0, NOTE_OFF, -1, 0, 0);
			const bool audioWasntLocked = !audio.locked;
			if (audioWasntLocked)
				lockAudio();
			if (!polyMatrixOwnsDestination(i))
			{
				stopVoice(i); /* guaranteed silence for an infinite sample loop */
				channel[i].realVol = channel[i].outVol = 0;
				channel[i].status |= CS_UPDATE_VOL | CS_USE_QUICK_VOLRAMP;
			}
			if (audioWasntLocked)
				unlockAudio();
		}
		jogAuditionHeld[i] = false;
		changed = true;
	}
	return changed;
}

void tapeheadActionPatternJogService(void)
{
	/* Kept as a stable UI service hook. Momentary is now a true one-shot, so
	** releasing the physical control never truncates the sample decay. */
}

static bool channelAcceptsPatternJog(int32_t channelIndex)
{
	/* Include is an intentional performance override: strumming reads the
	** ordinary pattern row on every channel without changing any FastTracks
	** assignment. The private head resumes ownership on its next event. */
	if (tapeheadConfig.patternJogIncludeFastTracks)
		return true;

	/* Ignore preserves the isolated-strum behavior: a FastTracks-owned channel
	** continues following its private head while Pattern Jog moves only the
	** ordinary head. This mode check deliberately does not depend on the
	** FastTracks master switch; assignment is the ownership boundary. */
	return fastTracksPOCGetMode(channelIndex) == FAST_TRACKS_MODE_STANDARD;
}

bool tapeheadActionPatternJogTrackParticipates(int32_t channelIndex)
{
	return channelIndexIsActive(channelIndex) &&
		channelAcceptsPatternJog(channelIndex);
}

static void notePatternJogVisualActivity(uint16_t patternNum, uint16_t row)
{
	jogVisualPattern = patternNum;
	jogVisualRow = row;
	jogVisualLastTick = SDL_GetTicks();
	jogVisualActive = true;
	ui.updatePatternEditor = true;
}

bool tapeheadActionPatternJogGetVisualPosition(uint16_t *patternNum,
	uint16_t *row)
{
	if (!jogVisualActive)
		return false;

	if ((uint32_t)(SDL_GetTicks() - jogVisualLastTick) >
		PATTERN_JOG_VISUAL_HOLD_MS)
	{
		jogVisualActive = false;
		ui.updatePatternEditor = true;
		return false;
	}

	if (patternNum != NULL)
		*patternNum = jogVisualPattern;
	if (row != NULL)
		*row = jogVisualRow;
	return true;
}

static bool jogEventCutsVoice(const note_t *event)
{
	return event->note == NOTE_OFF || event->vol == 0x10 ||
		(event->efx == 0x0C && event->efxData == 0) ||
		(event->efx == 0x0E && event->efxData == 0xC0);
}

static void auditionJogRow(uint8_t patternNum, uint16_t row,
	int32_t direction, bool forceAudition)
{
	uint8_t auditionMode = tapeheadConfig.patternJogAudition;
	if (forceAudition && auditionMode == TAPEHEAD_PATTERN_JOG_AUDITION_OFF)
		auditionMode = TAPEHEAD_PATTERN_JOG_AUDITION_MOMENTARY;
	if (auditionMode == TAPEHEAD_PATTERN_JOG_AUDITION_OFF ||
		pattern[patternNum] == NULL)
	{
		return;
	}

	for (int32_t ch = 0; ch < song.numChannels && ch < MAX_CHANNELS; ch++)
	{
		if (!channelAcceptsPatternJog(ch))
			continue;

		const note_t *event =
			&pattern[patternNum][(row * MAX_CHANNELS) + ch];

		/* Cue-encoder and crossfader strums share this path. Pitch extensions
		** are channel state, so effect-only rows update already ringing voices
		** and note rows inherit the new center/depth before playTone(). */
		if (microtonalEffectIsPitchExtension(event->efx))
			applyChannelMicrotonalEffect((uint8_t)ch, event->efx, event->efxData);

		if (event->instr != 0)
			jogAuditionInstrument[ch] = event->instr;

		if (jogEventCutsVoice(event))
		{
			if (jogAuditionHeld[ch] && !polyMatrixOwnsDestination(ch))
				playTone((uint8_t)ch, 0, NOTE_OFF, -1, 0, 0);
			jogAuditionHeld[ch] = false;
			continue;
		}
		if (event->note == 0 || event->note > 96 ||
			polyMatrixOwnsDestination(ch))
		{
			continue;
		}

		uint8_t instrument = jogAuditionInstrument[ch];
		if (instrument == 0)
			instrument = editor.curInstr;
		if (instrument == 0)
			continue;

		if (auditionMode == TAPEHEAD_PATTERN_JOG_AUDITION_LATCHED)
		{
			playTone((uint8_t)ch, instrument, event->note, -1, 0, 0);
		}
		else
		{
			const bool reverse = auditionMode ==
				TAPEHEAD_PATTERN_JOG_AUDITION_MANUAL_PINGPONG &&
				direction < 0;
			playToneOneShot((uint8_t)ch, instrument, event->note, -1,
				0, 0, reverse);
		}
		jogAuditionInstrument[ch] = instrument;
		jogAuditionHeld[ch] = true;
	}
}

static void emitJogRow(uint8_t patternNum, uint16_t row, int32_t direction,
	bool forceAudition)
{
	const bool audible = forceAudition || tapeheadConfig.patternJogAudition !=
		TAPEHEAD_PATTERN_JOG_AUDITION_OFF;
	if (audible)
	{
		bakerBeginManualRow();
		if (pattern[patternNum] != NULL)
		{
			for (int32_t ch = 0; ch < song.numChannels && ch < MAX_CHANNELS; ch++)
			{
				if (!channelAcceptsPatternJog(ch))
					continue;

				const note_t *event = &pattern[patternNum]
					[(row * MAX_CHANNELS) + ch];
				bakerCaptureManualEvent(ch, event);
			}
		}
	}
	auditionJogRow(patternNum, row, direction, forceAudition);
	if (transportPunchFrozen && audible)
		transportPunchConsumedRow = true;
}

static uint16_t wrapJogRow(uint16_t row, int32_t direction, uint16_t rows)
{
	if (direction < 0)
		return row == 0 ? rows - 1 : row - 1;
	return row + 1 >= rows ? 0 : row + 1;
}

bool tapeheadActionPatternJogRelative(int32_t delta)
{
	if (delta == 0)
		return false;

	if (songPlaying)
	{
		const uint16_t rows = song.currNumRows;
		if (rows == 0 || rows > MAX_PATT_LEN)
			return false;

		const bool audioWasntLocked = !audio.locked;
		if (audioWasntLocked)
			lockAudio();
		if (transportPunchFrozen)
			transportPunchConsumedRow = false;
		while (delta != 0)
		{
			song.row = wrapJogRow(song.row, delta, rows);
			emitJogRow(song.pattNum, song.row, delta, false);
			delta += delta < 0 ? 1 : -1;
		}
		/* Tick 1 makes the next audio callback a true tick-zero read of the
		** selected row, giving the encoder its deliberate scratch/retrigger. */
		song.tick = 1;
		editor.row = song.row;
		notePatternJogVisualActivity(song.pattNum, song.row);
		if (audioWasntLocked)
			unlockAudio();
		ui.updatePosSections = true;
		ui.updatePatternEditor = true;
		return true;
	}

	const uint8_t patternNum = editor.editPattern;
	const uint16_t rows = patternNumRows[patternNum];
	if (rows == 0 || rows > MAX_PATT_LEN)
		return false;

	while (delta != 0)
	{
		editor.row = wrapJogRow(editor.row, delta, rows);
		song.row = editor.row;
		emitJogRow(patternNum, editor.row, delta, false);
		delta += delta < 0 ? 1 : -1;
	}
	notePatternJogVisualActivity(patternNum, editor.row);
	ui.updatePatternEditor = true;
	return true;
}

bool tapeheadActionPatternJogAbsolute(int32_t value)
{
	if (value < 0) value = 0;
	if (value > 127) value = 127;

	if (songPlaying)
	{
		const uint16_t rows = song.currNumRows;
		if (rows == 0 || rows > MAX_PATT_LEN)
			return false;

		const uint16_t target =
			(uint16_t)((value * (rows - 1) + 63) / 127);
		notePatternJogVisualActivity(song.pattNum, target);
		if (song.row == target)
			return false;

		const bool audioWasntLocked = !audio.locked;
		if (audioWasntLocked)
			lockAudio();
		const int32_t direction = target > song.row ? 1 : -1;
		if (transportPunchFrozen)
			transportPunchConsumedRow = false;
		while (song.row != target)
		{
			song.row = wrapJogRow(song.row, direction, rows);
			emitJogRow(song.pattNum, song.row, direction, false);
		}
		/* Match the relative jog: the next callback performs a tick-zero read
		** from the absolute crossfader row. */
		song.tick = 1;
		editor.row = target;
		if (audioWasntLocked)
			unlockAudio();
		ui.updatePosSections = true;
		ui.updatePatternEditor = true;
		return true;
	}

	const uint8_t patternNum = editor.editPattern;
	const uint16_t rows = patternNumRows[patternNum];
	if (rows == 0 || rows > MAX_PATT_LEN)
		return false;

	const uint16_t target =
		(uint16_t)((value * (rows - 1) + 63) / 127);
	notePatternJogVisualActivity(patternNum, target);
	if (editor.row == target)
		return false;

	const int32_t direction = target > editor.row ? 1 : -1;
	while (editor.row != target)
	{
		editor.row = wrapJogRow(editor.row, direction, rows);
		song.row = editor.row;
		emitJogRow(patternNum, editor.row, direction, false);
	}
	ui.updatePatternEditor = true;
	return true;
}

bool tapeheadActionTransportPlaySong(void)
{
	pbPlaySong();
	return true;
}

bool tapeheadActionTransportPlayPattern(void)
{
	pbPlayPtn();
	return true;
}

static bool ordinaryModeIsPlaying(bool patternMode)
{
	if (!songPlaying || patternLauncherIsEnabled())
		return false;
	if (patternMode)
		return playMode == PLAYMODE_PATT || playMode == PLAYMODE_RECPATT;
	return playMode == PLAYMODE_SONG || playMode == PLAYMODE_RECSONG;
}

static void prepareOrdinaryTransport(void)
{
	(void)tapeheadActionPatternJogStopAudition();
	if (patternLauncherIsEnabled())
	{
		/* Clear Q ownership before starting the requested ordinary mode. The
		** ordinary note-off pass leaves independently owned Poly tunnels alone. */
		patternLauncherSetEnabled(false);
		stopPlayingKeepPoly();
	}
	(void)sampleLauncherStopQ();
	tapeheadActionMatrixSequenceCancel();
}

bool tapeheadActionTransportPlaySongToggle(void)
{
	if (ordinaryModeIsPlaying(false))
	{
		stopPlayingKeepPoly();
		return true;
	}
	prepareOrdinaryTransport();
	pbPlaySong();
	return true;
}

bool tapeheadActionTransportPlayPatternToggle(void)
{
	if (ordinaryModeIsPlaying(true))
	{
		stopPlayingKeepPoly();
		return true;
	}
	prepareOrdinaryTransport();
	pbPlayPtn();
	return true;
}

bool tapeheadActionTransportPlaySelectedMode(void)
{
	if (transportPatternMode)
		pbPlayPtn();
	else
		pbPlaySong();
	return true;
}

bool tapeheadActionTransportModeToggle(void)
{
	transportPatternMode ^= 1;
	if (songPlaying)
	{
		if (playMode == PLAYMODE_SONG) playMode = PLAYMODE_PATT;
		else if (playMode == PLAYMODE_PATT) playMode = PLAYMODE_SONG;
		else if (playMode == PLAYMODE_RECSONG) playMode = PLAYMODE_RECPATT;
		else if (playMode == PLAYMODE_RECPATT) playMode = PLAYMODE_RECSONG;
		ui.updatePosSections = true;
	}
	return true;
}

bool tapeheadActionTransportPatternModeIsSelected(void)
{
	return transportPatternMode;
}

bool tapeheadActionTransportStop(void)
{
	/* A Live/Performance Bake owns Main Stop even when no FT2 transport is
	** running. stopPlaying() performs the common export/finalize handoff. */
	if (bakerLiveIsCapturing() || bakerLiveIsArmed())
	{
		(void)tapeheadActionPatternJogStopAudition();
		stopPlaying();
		return true;
	}

	if (shiftModifierHeld)
	{
		const bool hadJog = tapeheadActionPatternJogStopAudition();
		const bool hadWork = songPlaying || patternLauncherIsEnabled() ||
			polyMatrixHasAudioWork() || sampleLauncherHasTransportWork() || hadJog;
		stopPlaying();
		stopVoices();
		sampleLauncherReset();
		tapeheadActionMatrixSequenceCancel();
		return hadWork;
	}

	bool changed = tapeheadActionPatternJogStopAudition();
	/* Pattern Q uses songPlaying as its clock, but belongs to the Matrix stop
	** hierarchy. Main Stop must leave it running. */
	if (songPlaying && !patternLauncherIsEnabled())
	{
		stopPlayingKeepPoly();
		changed = true;
	}
	return changed;
}

bool tapeheadActionTransportStopSong(void)
{
	if (!songPlaying || patternLauncherIsEnabled())
		return false;

	stopPlayingKeepPoly();
	return true;
}

bool tapeheadActionTransportStopDeck(void)
{
	/* The APC40 mkII has Stop All Clips but no ordinary transport Stop.
	** Shift turns that physical button into an isolated jog-voice release. */
	if (shiftModifierHeld)
		return tapeheadActionPatternJogStopAudition();
	if (bakerLiveIsCapturing() || bakerLiveIsArmed())
	{
		(void)tapeheadActionPatternJogStopAudition();
		stopPlaying();
		return true;
	}

	const bool hadWork = patternLauncherIsEnabled() || polyMatrixHasAudioWork() ||
		sampleLauncherHasTransportWork();
	patternLauncherStopDeckQ();
	polyMatrixReset();
	sampleLauncherReset();
	tapeheadActionMatrixSequenceCancel();
	return hadWork;
}

bool tapeheadActionTransportStopSelectedDeck(void)
{
	if (tapeheadActionMatrixGetTarget() == TAPEHEAD_MATRIX_SAMPLE)
	{
		const bool changed = matrixGridPoly ? sampleLauncherStopPoly() :
			sampleLauncherStopQ();
		if (!matrixGridPoly && matrixSequence.active &&
			matrixSequence.target == TAPEHEAD_MATRIX_SAMPLE)
			tapeheadActionMatrixSequenceCancel();
		return changed;
	}

	if (matrixGridPoly)
	{
		const bool hadWork = polyMatrixHasAudioWork();
		polyMatrixReset();
		return hadWork;
	}

	const bool hadWork = patternLauncherIsEnabled();
	patternLauncherStopDeckQ();
	if (matrixSequence.active &&
		matrixSequence.target == TAPEHEAD_MATRIX_PATTERN)
		tapeheadActionMatrixSequenceCancel();
	return hadWork;
}

bool tapeheadActionTransportStopAll(void)
{
	const bool hadJog = tapeheadActionPatternJogStopAudition();
	const bool hadWork = songPlaying || patternLauncherIsEnabled() ||
		polyMatrixHasAudioWork() || sampleLauncherHasTransportWork() || hadJog;
	stopPlaying();
	sampleLauncherReset();
	tapeheadActionMatrixSequenceCancel();
	return hadWork;
}

bool tapeheadActionTransportPunchIsFrozen(void)
{
	return transportPunchFrozen;
}

static bool setTransportPunchFrozen(bool frozen)
{
	if (transportPunchFrozen == frozen)
		return false;

	if (frozen)
	{
		/* Publish the freeze first so the audio callback cannot advance while
		** its visible row is being latched as the manual starting point. */
		transportPunchFrozen = true;
		tapeheadReplayerBeginTransportPunch();
		/* The row visible when the pedal punches out has already sounded. The
		** default continuation therefore consumes it and resumes at the next
		** row. Retrigger deliberately restores the earlier double-strike. A
		** later silent relocation clears this flag; an auditioned/manual row
		** sets it again. */
		transportPunchConsumedRow =
			!tapeheadConfig.transportFreezeResumeRetrigger;
		if (tapeheadConfig.transportFreezeAudioCut)
		{
			(void)tapeheadActionPatternJogStopAudition();
			stopVoices();
			const bool audioWasntLocked = !audio.locked;
			if (audioWasntLocked)
				lockAudio();
			audioSampleLauncherStopAll();
			if (audioWasntLocked)
				unlockAudio();
		}
	}
	else
	{
		/* Prepare the exact tick/row to resume while the audio callback still
		** sees the scheduler as frozen, then release it. */
		tapeheadReplayerResumeTransportPunch(transportPunchConsumedRow);
		transportPunchConsumedRow = false;
		transportPunchFrozen = false;
	}
	ui.updatePosSections = true;
	ui.updatePatternEditor = true;
	return true;
}

bool tapeheadActionTransportPunchPedal(bool pressed)
{
	if (transportPunchPedalDown == pressed)
		return false;
	transportPunchPedalDown = pressed;

	if (tapeheadConfig.transportFreezePedalHold)
		return setTransportPunchFrozen(transportPunchKeyboardLatched || pressed);

	/* Toggle mode changes only the pedal-owned latch. The keyboard-owned latch
	** remains authoritative until Shift+Space toggles it off. */
	if (!pressed)
		return false;
	transportPunchPedalLatched = !transportPunchPedalLatched;
	return setTransportPunchFrozen(transportPunchKeyboardLatched ||
		transportPunchPedalLatched);
}

bool tapeheadActionTransportPunchKeyboardToggle(void)
{
	transportPunchKeyboardLatched = !transportPunchKeyboardLatched;

	const bool pedalOwnsFreeze = tapeheadConfig.transportFreezePedalHold
		? transportPunchPedalDown : transportPunchPedalLatched;
	return setTransportPunchFrozen(transportPunchKeyboardLatched ||
		pedalOwnsFreeze);
}

void tapeheadActionsResetForLoadedModule(void)
{
	revealHistoryCount = 0;
	shiftModifierHeld = matrixGridPoly = transportPatternMode = false;
	performanceSoloTrack = recordArmTrack = -1;
	memset(jogAuditionHeld, 0, sizeof (jogAuditionHeld));
	memset(jogAuditionInstrument, 0, sizeof (jogAuditionInstrument));
	jogVisualActive = false;
	jogVisualLastTick = 0;
	jogVisualPattern = jogVisualRow = 0;
	transportPunchFrozen = transportPunchPedalDown = false;
	transportPunchPedalLatched = transportPunchKeyboardLatched = false;
	transportPunchConsumedRow = false;
	memset(&matrixSequence, 0, sizeof (matrixSequence));
	matrixMasterGain = matrixQGain = matrixPolyGain = 256;
	matrixCrossfader = 64;
	audioSetMatrixMixerGains(matrixQGain, matrixPolyGain);
	sampleMorphResetForLoadedModule();
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		if (!performanceMute[i])
			continue;

		performanceMute[i] = false;
		if (i < song.numChannels)
			channel[i].status |= CS_UPDATE_VOL | CS_USE_QUICK_VOLRAMP;
	}
}
