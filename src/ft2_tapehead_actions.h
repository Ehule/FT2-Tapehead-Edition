#pragma once

#include <stdbool.h>
#include <stdint.h>

#define TAPEHEAD_TRACK_TRIM_MIN 0
#define TAPEHEAD_TRACK_TRIM_UNITY 256
#define TAPEHEAD_TRACK_TRIM_MAX 512

typedef enum tapeheadMatrixTarget_t
{
	TAPEHEAD_MATRIX_PATTERN = 0,
	TAPEHEAD_MATRIX_SAMPLE
} tapeheadMatrixTarget_t;

typedef enum tapeheadMatrixSequenceType_t
{
	TAPEHEAD_MATRIX_SEQUENCE_NONE = 0,
	TAPEHEAD_MATRIX_SEQUENCE_ROW,
	TAPEHEAD_MATRIX_SEQUENCE_COLUMN,
	TAPEHEAD_MATRIX_SEQUENCE_BANK
} tapeheadMatrixSequenceType_t;

/*
** Device-independent Tapehead performance actions.
**
** Track indices are zero-based. Actions reject tracks outside the current
** module and return true only when they changed runtime state.
*/
bool tapeheadActionTrackPerformanceMuteToggle(int32_t channelIndex);
bool tapeheadActionPerformanceMuteAll(void);
bool tapeheadActionPerformanceUnmuteAll(void);
bool tapeheadActionPerformanceMuteMaster(void);
bool tapeheadActionTrackPerformanceSoloToggle(int32_t channelIndex);
int32_t tapeheadActionGetPerformanceSoloTrack(void);
bool tapeheadActionPerformanceUnmuteNext(void);
bool tapeheadActionPerformanceMutePrevious(void);
uint8_t tapeheadActionPerformanceGetRevealHistoryCount(void);
bool tapeheadActionTrackMuteToggle(int32_t channelIndex);
bool tapeheadActionUnmuteAll(void);
bool tapeheadActionTrackTrimSet(int32_t channelIndex, int32_t trim);
bool tapeheadActionTrackSelect(int32_t channelIndex);
bool tapeheadActionTrackRecordArm(int32_t channelIndex);
int32_t tapeheadActionGetRecordArmTrack(void);
bool tapeheadActionCursorLeft(void);
bool tapeheadActionCursorRight(void);
bool tapeheadActionCursorUp(void);
bool tapeheadActionCursorDown(void);
bool tapeheadActionSongOrderPrevious(void);
bool tapeheadActionSongOrderNext(void);
void tapeheadActionSetShiftModifier(bool held);
bool tapeheadActionShiftModifierIsHeld(void);

bool tapeheadActionMasterVolumeSet(int32_t volume);
bool tapeheadActionTempoAdjust(int32_t delta);
bool tapeheadActionSpeedAdjust(int32_t delta);

bool tapeheadActionFastTrackToggle(int32_t channelIndex);
bool tapeheadActionFastTrackMasterToggle(void);
bool tapeheadActionFastTrackTransmissionClutchToggle(void);
bool tapeheadActionFastTrackGlobalReverseToggle(void);
bool tapeheadActionFastTrackGlobalModeToggle(void);
uint8_t tapeheadActionFastTrackGlobalModeState(void);
bool tapeheadActionFastTrackDirectionOrSongMode(int32_t channelIndex);
bool tapeheadActionFastTrackRatioAllNext(void);
bool tapeheadActionFastTrackRatioAllPrevious(void);
bool tapeheadActionFastTrackRatioAllOrMatrixBankNext(void);
bool tapeheadActionFastTrackRatioAllOrMatrixBankPrevious(void);
bool tapeheadActionFastTrackRatioSet(int32_t channelIndex, int32_t ratioIndex);
bool tapeheadActionFastTrackRatioNext(int32_t channelIndex);
bool tapeheadActionFastTrackRatioPrevious(int32_t channelIndex);
bool tapeheadActionFastTrackRatioReset(int32_t channelIndex);
bool tapeheadActionFastTrackResetAll(void);
bool tapeheadActionFastTrackReverseToggle(int32_t channelIndex);
bool tapeheadActionFastTrackClutchToggle(int32_t channelIndex);

tapeheadMatrixTarget_t tapeheadActionMatrixGetTarget(void);
bool tapeheadActionMatrixSetTarget(tapeheadMatrixTarget_t target);
bool tapeheadActionMatrixToggleTarget(void);
uint8_t tapeheadActionMatrixGetBank(tapeheadMatrixTarget_t target);
bool tapeheadActionMatrixBankSelect(uint8_t bank);
bool tapeheadActionMatrixLayerBankSelect(uint8_t bank);
bool tapeheadActionMatrixBankNext(void);
bool tapeheadActionMatrixBankPrevious(void);
bool tapeheadActionMatrixSlotTrigger(uint8_t localSlot);
bool tapeheadActionMatrixSlotScheduleStop(uint8_t localSlot);
bool tapeheadActionMatrixGridModeToggle(void);
bool tapeheadActionMatrixGridIsPoly(void);
bool tapeheadActionMatrixVisibilityToggle(void);
bool tapeheadActionMatrixSequenceRow(uint8_t row);
bool tapeheadActionMatrixSequenceColumn(uint8_t column);
bool tapeheadActionMatrixSequenceBank(void);
void tapeheadActionMatrixSequenceHandleBoundary(void);
void tapeheadActionMatrixSequenceCancel(void);
bool tapeheadActionMatrixSequenceIsActive(void);
tapeheadMatrixSequenceType_t tapeheadActionMatrixSequenceGetType(void);
int32_t tapeheadActionMatrixSequenceGetControlIndex(void);
bool tapeheadActionMatrixSequenceSlotIsPending(uint8_t localSlot);

bool tapeheadActionMatrixMasterVolumeSet(int32_t value);
bool tapeheadActionMatrixCrossfaderSet(int32_t value);
uint16_t tapeheadActionMatrixGetQGain(void);
uint16_t tapeheadActionMatrixGetPolyGain(void);

bool tapeheadActionTransportPlaySong(void);
bool tapeheadActionTransportPlayPattern(void);
bool tapeheadActionTransportPlaySongToggle(void);
bool tapeheadActionTransportPlayPatternToggle(void);
bool tapeheadActionTransportPlaySelectedMode(void);
bool tapeheadActionTransportModeToggle(void);
bool tapeheadActionTransportPatternModeIsSelected(void);
bool tapeheadActionTransportStop(void);
bool tapeheadActionTransportStopSong(void);
bool tapeheadActionTransportStopSelectedDeck(void);
bool tapeheadActionTransportStopDeck(void);
bool tapeheadActionTransportStopAll(void);
bool tapeheadActionTransportPunchPedal(bool pressed);
bool tapeheadActionTransportPunchIsFrozen(void);

bool tapeheadActionPatternJogRelative(int32_t delta);
bool tapeheadActionPatternJogAbsolute(int32_t value);
bool tapeheadActionPatternJogStopAudition(void);
void tapeheadActionPatternJogService(void);

void tapeheadActionsResetForLoadedModule(void);
