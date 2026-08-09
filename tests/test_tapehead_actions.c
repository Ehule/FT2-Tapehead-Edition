#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ft2_replayer.h"
#include "ft2_audio.h"
#include "ft2_structs.h"
#include "ft2_config.h"
#include "ft2_tapehead_actions.h"
#include "ft2_fasttracks.h"
#include "ft2_sample_launcher_state.h"

channel_t channel[MAX_CHANNELS];
uint16_t channelVolumeTrim[MAX_CHANNELS];
bool performanceMute[MAX_CHANNELS];
song_t song;
editor_t editor;
cursor_t cursor;
bool songPlaying;
int8_t playMode;
ui_t ui;
config_t config;
tapeheadConfig_t tapeheadConfig;
audio_t audio;
note_t *pattern[MAX_PATTERNS];
int16_t patternNumRows[MAX_PATTERNS];

static bool fastSelected[MAX_CHANNELS], fastReversed[MAX_CHANNELS];
static bool fastClutched[MAX_CHANNELS], fastMaster, transmissionClutch;
static fastTracksMode_t fastMode[MAX_CHANNELS];
static uint8_t fastRatio[MAX_CHANNELS];
static bool sampleDeck, patternExposed[MAX_PATTERNS];
static bool sampleLoaded[SAMPLE_LAUNCHER_MAX_TILES];
static uint8_t patternPage, sampleBank;
static int32_t lastPatternRequest, lastSampleRequest;
static bool patternEnabled, polyWork, sampleWork;
static uint32_t playSongCount, playPatternCount, stopSongCount;
static uint32_t stopDeckCount, stopAllCount;
static uint32_t cursorLeftCount, cursorRightCount, cursorUpCount, cursorDownCount;
static uint32_t patternQueueClearCount, sampleQueueClearCount;
static uint16_t audioQGain, audioPolyGain;
static bool standaloneShown;
static note_t testPattern[MAX_PATT_LEN * MAX_CHANNELS];
static uint32_t jogNoteOnCount, jogNoteOffCount;
static uint32_t jogOneShotCount, jogReverseOneShotCount;
static uint32_t manualBakeRows, manualBakeEvents;
static int32_t lastManualBakeChannel;
static uint8_t lastManualBakeEffect, lastManualBakeParameter;
static uint32_t microEffectCount;
static uint8_t lastMicroChannel, lastMicroEffect, lastMicroParameter;
static uint32_t stopVoicesCount, stopLauncherVoicesCount, punchResumeCount;
static bool punchResumeConsumed;

void redrawScopeChannel(int32_t channelIndex) { (void)channelIndex; }
void cursorLeft(void) { cursorLeftCount++; }
void cursorRight(void) { cursorRightCount++; }
void cursorTabLeft(void) { cursorLeftCount++; }
void cursorTabRight(void) { cursorRightCount++; }
void rowOneUpWrap(void) { cursorUpCount++; }
void rowOneDownWrap(void) { cursorDownCount++; }
void decSongPos(void) { if (editor.songPos > 0) editor.songPos--; }
void incSongPos(void)
{
	if (editor.songPos + 1 < song.songLength) editor.songPos++;
}
void bakerBeginManualRow(void) { manualBakeRows++; }
void bakerCaptureManualEvent(int32_t channelIndex, const note_t *event)
{
	if (event->note || event->instr || event->vol || event->efx || event->efxData)
	{
		manualBakeEvents++;
		lastManualBakeChannel = channelIndex;
		lastManualBakeEffect = event->efx;
		lastManualBakeParameter = event->efxData;
	}
}
bool bakerLiveIsCapturing(void) { return false; }
bool bakerLiveIsArmed(void) { return false; }
void setAudioAmp(int16_t amp, int16_t masterVol, bool bitDepth32Flag)
{ (void)amp; (void)masterVol; (void)bitDepth32Flag; }
void pbBPMUp(void) { if (song.BPM < MAX_BPM) song.BPM++; }
void pbBPMDown(void) { if (song.BPM > MIN_BPM) song.BPM--; }
void pbSpeedUp(void) { if (song.speed < MAX_SPEED) song.speed++; }
void pbSpeedDown(void) { if (song.speed > 0) song.speed--; }
void sampleMorphResetForLoadedModule(void) { }
void audioSetMatrixMixerGains(uint16_t qGain, uint16_t polyGain)
{ audioQGain = qGain; audioPolyGain = polyGain; }
void lockAudio(void) { audio.locked = true; }
void unlockAudio(void) { audio.locked = false; }
void resetSyncQueues(void) { }
void stopVoice(int32_t voiceIndex) { (void)voiceIndex; }
void stopVoices(void) { stopVoicesCount++; }
void audioSampleLauncherStopAll(void) { stopLauncherVoicesCount++; }
void tapeheadReplayerResumeTransportPunch(bool consumed)
{ punchResumeCount++; punchResumeConsumed = consumed; }
void tapeheadReplayerBeginTransportPunch(void) { }
void applyChannelMicrotonalEffect(uint8_t channelIndex, uint8_t effect, uint8_t parameter)
{
	microEffectCount++;
	lastMicroChannel = channelIndex;
	lastMicroEffect = effect;
	lastMicroParameter = parameter;
}
void playTone(uint8_t chNum, uint8_t insNum, uint8_t note, int8_t vol,
	uint16_t midiVibDepth, uint16_t midiPitch)
{
	(void)chNum; (void)insNum; (void)vol; (void)midiVibDepth; (void)midiPitch;
	if (note == NOTE_OFF) jogNoteOffCount++;
	else jogNoteOnCount++;
}
void playToneOneShot(uint8_t chNum, uint8_t insNum, uint8_t note, int8_t vol,
	uint16_t midiVibDepth, uint16_t midiPitch, bool reverse)
{
	playTone(chNum, insNum, note, vol, midiVibDepth, midiPitch);
	jogOneShotCount++;
	if (reverse) jogReverseOneShotCount++;
}
void setNewSongPos(int32_t pos)
{
	if (pos < 0) pos = 0;
	if (pos >= song.songLength) pos = song.songLength - 1;
	song.songPos = editor.songPos = (int16_t)pos;
	song.pattNum = song.orders[pos];
	editor.editPattern = (uint16_t)song.pattNum;
	song.row = editor.row = 0;
}
void tapeheadReplayerSetTransportPunchSongPos(int32_t pos)
{ setNewSongPos(pos); }

void setChannelMute(int32_t channelIndex, bool off)
{
	channel[channelIndex].channelOff = off;
}

void jumpToChannel(uint8_t channelIndex)
{
	cursor.ch = channelIndex;
}

bool fastTracksPOCMasterIsEnabled(void) { return fastMaster; }
void fastTracksPOCSetMasterEnabled(bool enabled) { fastMaster = enabled; }
bool fastTracksPOCIsSelected(int32_t i) { return fastSelected[i]; }
void fastTracksPOCSetTrackEnabled(int32_t i, bool enabled)
{
	fastSelected[i] = enabled;
}
uint8_t fastTracksPOCGetRatioIndex(int32_t i) { return fastRatio[i]; }
uint8_t fastTracksPOCGetRatioCount(void) { return 17; }
void fastTracksPOCSetRatioIndex(int32_t i, uint8_t ratioIndex)
{
	fastRatio[i] = ratioIndex;
}
void fastTracksPOCResetAllRatios(void)
{
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		if (fastSelected[i]) fastRatio[i] = FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX;
}
bool fastTracksPOCIsReversed(int32_t i) { return fastReversed[i]; }
void fastTracksPOCToggleDirection(int32_t i) { fastReversed[i] ^= 1; }
bool fastTracksPOCIsClutched(int32_t i) { return fastClutched[i]; }
void fastTracksPOCSetClutch(int32_t i, bool engaged)
{
	fastClutched[i] = engaged;
}
bool fastTracksPOCTransmissionClutchIsLatched(void) { return transmissionClutch; }
void fastTracksPOCTransmissionClutchToggle(void) { transmissionClutch ^= 1; }
fastTracksMode_t fastTracksPOCGetMode(int32_t i) { return fastMode[i]; }
void fastTracksPOCSetMode(int32_t i, fastTracksMode_t mode)
{ fastMode[i] = mode; fastSelected[i] = mode != FAST_TRACKS_MODE_STANDARD; }

bool patternLauncherDeckIsSample(void) { return sampleDeck; }
void patternLauncherSetDeckMode(bool enabled) { sampleDeck = enabled; }
uint8_t patternLauncherGetPage(void) { return patternPage; }
void patternLauncherSetPage(uint8_t page) { patternPage = page; }
void patternLauncherClearQueue(void) { patternQueueClearCount++; }
void patternLauncherForceRedraw(void) { }
bool patternLauncherPatternIsExposed(uint8_t patternNum)
{
	return patternExposed[patternNum];
}
void patternLauncherRequest(uint8_t patternNum, bool ctrl, bool shift)
{
	lastPatternRequest = patternNum;
	(void)ctrl;
	(void)shift;
}
bool patternLauncherIsEnabled(void) { return patternEnabled; }
void patternLauncherSetEnabled(bool enabled) { patternEnabled = enabled; }
bool patternLauncherScheduleStop(uint8_t patternNum)
{ lastPatternRequest = patternNum; return patternEnabled; }
void patternLauncherStopDeckQ(void)
{
	patternEnabled = false;
	stopDeckCount++;
}
uint8_t sampleLauncherGetBank(void) { return sampleBank; }
void sampleLauncherSetBank(uint8_t bank) { sampleBank = bank; }
void sampleLauncherClearQQueue(void) { sampleQueueClearCount++; }
bool sampleLauncherTileIsLoaded(uint16_t tile)
{ return tile < SAMPLE_LAUNCHER_MAX_TILES && sampleLoaded[tile]; }
bool sampleLauncherRequestQ(uint16_t tile)
{
	lastSampleRequest = tile;
	return sampleWork;
}
bool sampleLauncherTogglePoly(uint16_t tile)
{ lastSampleRequest = tile; return sampleWork; }
bool sampleLauncherScheduleStop(uint16_t tile)
{ lastSampleRequest = tile; return sampleWork; }
bool sampleLauncherStopQ(void)
{ const bool changed = sampleWork; sampleWork = false; return changed; }
bool sampleLauncherStopPoly(void)
{ const bool changed = sampleWork; sampleWork = false; return changed; }
bool sampleLauncherHasTransportWork(void) { return sampleWork; }
void sampleLauncherReset(void) { sampleWork = false; }
bool polyMatrixHasAudioWork(void) { return polyWork; }
void polyMatrixReset(void) { polyWork = false; }
bool polyMatrixTogglePattern(uint8_t patternNum, bool immediate)
{ lastPatternRequest = patternNum; (void)immediate; return polyWork; }
bool polyMatrixSchedulePatternStop(uint8_t patternNum)
{ lastPatternRequest = patternNum; return polyWork; }
bool polyMatrixOwnsDestination(int32_t destinationChannel)
{ (void)destinationChannel; return false; }
bool patternLauncherStandaloneIsShown(void) { return standaloneShown; }
void patternLauncherSetStandaloneShown(bool shown) { standaloneShown = shown; }
void pbPlaySong(void)
{ playSongCount++; songPlaying = true; playMode = PLAYMODE_SONG; }
void pbPlayPtn(void)
{ playPatternCount++; songPlaying = true; playMode = PLAYMODE_PATT; }
void pbRecPtn(void) { songPlaying = true; playMode = PLAYMODE_RECPATT; }
void stopPlayingKeepPoly(void) { stopSongCount++; songPlaying = false; }
void stopPlaying(void)
{
	stopAllCount++;
	songPlaying = patternEnabled = polyWork = false;
}

static void resetFixture(int32_t numChannels)
{
	memset(channel, 0, sizeof (channel));
	memset(performanceMute, 0, sizeof (performanceMute));
	memset(&editor, 0, sizeof (editor));
	memset(&cursor, 0, sizeof (cursor));
	memset(fastSelected, 0, sizeof (fastSelected));
	memset(fastReversed, 0, sizeof (fastReversed));
	memset(fastClutched, 0, sizeof (fastClutched));
	memset(fastMode, 0, sizeof (fastMode));
	memset(fastRatio, FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX, sizeof (fastRatio));
	memset(patternExposed, 1, sizeof (patternExposed));
	memset(sampleLoaded, 1, sizeof (sampleLoaded));
	memset(pattern, 0, sizeof (pattern));
	memset(patternNumRows, 0, sizeof (patternNumRows));
	memset(testPattern, 0, sizeof (testPattern));
	pattern[0] = testPattern;
	patternNumRows[0] = 64;
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		channelVolumeTrim[i] = TAPEHEAD_TRACK_TRIM_UNITY;

	memset(&song, 0, sizeof (song));
	song.numChannels = numChannels;
	sampleDeck = fastMaster = songPlaying = transmissionClutch = false;
	standaloneShown = false;
	playMode = PLAYMODE_IDLE;
	memset(&ui, 0, sizeof (ui));
	memset(&config, 0, sizeof (config));
	memset(&tapeheadConfig, 0, sizeof (tapeheadConfig));
	tapeheadConfig.trackTrimMaxPercent = 200;
	tapeheadConfig.patternJogAudition = TAPEHEAD_PATTERN_JOG_AUDITION_LATCHED;
	memset(&audio, 0, sizeof (audio));
	config.masterVol = 128;
	song.BPM = 125;
	song.speed = 6;
	patternEnabled = polyWork = sampleWork = false;
	patternPage = sampleBank = 0;
	lastPatternRequest = lastSampleRequest = -1;
	playSongCount = playPatternCount = stopSongCount = 0;
	stopDeckCount = stopAllCount = 0;
	cursorLeftCount = cursorRightCount = cursorUpCount = cursorDownCount = 0;
	patternQueueClearCount = sampleQueueClearCount = 0;
	audioQGain = audioPolyGain = 256;
	jogNoteOnCount = jogNoteOffCount = 0;
	jogOneShotCount = jogReverseOneShotCount = 0;
	manualBakeRows = manualBakeEvents = 0;
	lastManualBakeChannel = -1;
	lastManualBakeEffect = lastManualBakeParameter = 0;
	microEffectCount = 0;
	lastMicroChannel = lastMicroEffect = lastMicroParameter = 0;
	stopVoicesCount = stopLauncherVoicesCount = punchResumeCount = 0;
	punchResumeConsumed = false;
	tapeheadActionsResetForLoadedModule();
}

static void testPerformanceMuteToggleUsesQuickRamp(void)
{
	resetFixture(8);
	editor.channelMuted[2] = true;
	channel[2].status = CS_TRIGGER_VOICE;

	assert(tapeheadActionTrackPerformanceMuteToggle(2));
	assert(performanceMute[2]);
	assert(editor.channelMuted[2]);
	assert(channel[2].status ==
		(CS_TRIGGER_VOICE | CS_UPDATE_VOL | CS_USE_QUICK_VOLRAMP));

	channel[2].status = 0;
	assert(tapeheadActionTrackPerformanceMuteToggle(2));
	assert(!performanceMute[2]);
	assert(editor.channelMuted[2]);
	assert(channel[2].status == (CS_UPDATE_VOL | CS_USE_QUICK_VOLRAMP));
}

static void testTrackBoundsRejectInactiveChannels(void)
{
	resetFixture(8);
	assert(!tapeheadActionTrackPerformanceMuteToggle(-1));
	assert(!tapeheadActionTrackPerformanceMuteToggle(8));
	assert(!tapeheadActionTrackTrimSet(-1, 0));
	assert(!tapeheadActionTrackTrimSet(8, 0));

	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		assert(!performanceMute[i]);
		assert(channelVolumeTrim[i] == TAPEHEAD_TRACK_TRIM_UNITY);
		assert(channel[i].status == 0);
	}

	/* A malformed module count must never make index MAX_CHANNELS valid. */
	song.numChannels = MAX_CHANNELS + 1;
	assert(!tapeheadActionTrackPerformanceMuteToggle(MAX_CHANNELS));
	assert(!tapeheadActionTrackTrimSet(MAX_CHANNELS, 0));
}

static void testUnmuteAllKeepsOrdinaryMutesSeparate(void)
{
	resetFixture(4);
	editor.channelMuted[1] = true;
	editor.channelMuted[3] = true;
	performanceMute[1] = true;
	performanceMute[3] = true;
	performanceMute[12] = true; /* stale state beyond the current module */
	channel[1].status = CS_TRIGGER_VOICE;

	assert(tapeheadActionPerformanceUnmuteAll());
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		assert(!performanceMute[i]);

	assert(editor.channelMuted[1]);
	assert(editor.channelMuted[3]);
	assert(channel[1].status ==
		(CS_TRIGGER_VOICE | CS_UPDATE_VOL | CS_USE_QUICK_VOLRAMP));
	assert(channel[3].status == (CS_UPDATE_VOL | CS_USE_QUICK_VOLRAMP));
	assert(channel[12].status == 0);
	assert(!tapeheadActionPerformanceUnmuteAll());
}

static void testTrackTrimClampsAndRefreshesVolume(void)
{
	resetFixture(4);
	channel[1].status = CS_TRIGGER_VOICE;

	assert(tapeheadActionTrackTrimSet(1, 300));
	assert(channelVolumeTrim[1] == 300);
	assert(channel[1].status == (CS_TRIGGER_VOICE | CS_UPDATE_VOL));
	assert((channel[1].status & CS_USE_QUICK_VOLRAMP) == 0);

	channel[1].status = 0;
	assert(tapeheadActionTrackTrimSet(1, -100));
	assert(channelVolumeTrim[1] == TAPEHEAD_TRACK_TRIM_MIN);
	assert(channel[1].status == CS_UPDATE_VOL);

	channel[1].status = 0;
	assert(tapeheadActionTrackTrimSet(1, 900));
	assert(channelVolumeTrim[1] == TAPEHEAD_TRACK_TRIM_MAX);
	assert(channel[1].status == CS_UPDATE_VOL);

	channel[1].status = 0;
	assert(!tapeheadActionTrackTrimSet(1, TAPEHEAD_TRACK_TRIM_MAX));
	assert(channel[1].status == 0);

	/* Mouse-wheel and controller paths share this setter and ceiling. */
	tapeheadConfig.trackTrimMaxPercent = 100;
	assert(tapeheadActionTrackTrimSet(1, 900));
	assert(channelVolumeTrim[1] == TAPEHEAD_TRACK_TRIM_UNITY);
}

static void testRevealHistoryIsDeterministic(void)
{
	resetFixture(6);
	for (int32_t i = 1; i < 6; i++) performanceMute[i] = true;
	editor.channelMuted[2] = true;

	assert(tapeheadActionPerformanceUnmuteNext());
	assert(!performanceMute[1]);
	assert(performanceMute[2]);
	assert(tapeheadActionPerformanceUnmuteNext());
	assert(!performanceMute[3]); /* ordinary-muted track 3 was skipped */

	assert(tapeheadActionPerformanceMutePrevious());
	assert(performanceMute[3]);
	assert(tapeheadActionPerformanceMutePrevious());
	assert(performanceMute[1]);
	assert(!tapeheadActionPerformanceMutePrevious());

	/* Manual remuting removes a revealed channel from the reverse stack. */
	assert(tapeheadActionPerformanceUnmuteNext());
	assert(tapeheadActionTrackPerformanceMuteToggle(1));
	assert(!tapeheadActionPerformanceMutePrevious());
}

static void testOrdinaryMuteAndUnmuteAllRemainExplicit(void)
{
	resetFixture(4);
	assert(tapeheadActionTrackMuteToggle(2));
	assert(editor.channelMuted[2] && channel[2].channelOff);
	performanceMute[1] = true;
	assert(tapeheadActionUnmuteAll());
	assert(!editor.channelMuted[2] && !channel[2].channelOff);
	assert(!performanceMute[1]);
	assert(!tapeheadActionUnmuteAll());
}

static void testTrackSelectionAndFastTracksActions(void)
{
	resetFixture(8);
	assert(tapeheadActionTrackSelect(5));
	assert(cursor.ch == 5);
	assert(!tapeheadActionTrackSelect(5));

	assert(tapeheadActionFastTrackToggle(2));
	assert(fastSelected[2]);
	assert(tapeheadActionFastTrackMasterToggle());
	assert(fastMaster);
	assert(tapeheadActionFastTrackRatioSet(2, 16));
	assert(!tapeheadActionFastTrackRatioNext(2)); /* clamps at top */
	assert(tapeheadActionFastTrackRatioPrevious(2));
	assert(fastRatio[2] == 15);
	assert(tapeheadActionFastTrackRatioReset(2));
	assert(fastRatio[2] == FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX);
	assert(tapeheadActionFastTrackReverseToggle(2));
	assert(fastReversed[2]);
	assert(tapeheadActionFastTrackClutchToggle(2));
	assert(fastClutched[2]);
	assert(!tapeheadActionFastTrackReverseToggle(3));
}

static void testMatrixFocusBanksAndSlotsAreIndependent(void)
{
	resetFixture(8);
	assert(tapeheadActionMatrixBankSelect(4));
	assert(patternPage == 4 && sampleBank == 0);
	assert(tapeheadActionMatrixSlotTrigger(7));
	assert(lastPatternRequest == (4 * 32) + 7);

	assert(tapeheadActionMatrixToggleTarget());
	assert(tapeheadActionMatrixGetTarget() == TAPEHEAD_MATRIX_SAMPLE);
	assert(tapeheadActionMatrixBankSelect(2));
	assert(patternPage == 4 && sampleBank == 2);
	sampleWork = true;
	assert(tapeheadActionMatrixSlotTrigger(31));
	assert(lastSampleRequest == (2 * 32) + 31);

	assert(tapeheadActionMatrixSetTarget(TAPEHEAD_MATRIX_PATTERN));
	assert(tapeheadActionMatrixGetBank(TAPEHEAD_MATRIX_PATTERN) == 4);
	assert(!tapeheadActionMatrixBankSelect(8));
	patternExposed[(4 * 32) + 1] = false;
	assert(!tapeheadActionMatrixSlotTrigger(1));
	assert(!tapeheadActionMatrixSlotTrigger(32));
}

static void testTransportActionsRemainDistinct(void)
{
	resetFixture(8);
	assert(tapeheadActionTransportPlaySong());
	assert(playSongCount == 1);
	assert(tapeheadActionTransportStopSong());
	assert(stopSongCount == 1);
	assert(tapeheadActionTransportPlayPattern());
	assert(playPatternCount == 1);

	patternEnabled = polyWork = sampleWork = true;
	assert(!tapeheadActionTransportStopSong());
	assert(tapeheadActionTransportStopDeck());
	assert(stopDeckCount == 1 && !patternEnabled && !polyWork && !sampleWork);

	songPlaying = sampleWork = true;
	assert(tapeheadActionTransportStopAll());
	assert(stopAllCount == 1 && !songPlaying && !sampleWork);
}

static void testModuleLoadClearsPerformanceRuntime(void)
{
	resetFixture(4);
	performanceMute[1] = performanceMute[3] = true;
	channel[1].status = CS_TRIGGER_VOICE;
	tapeheadActionsResetForLoadedModule();
	assert(!performanceMute[1] && !performanceMute[3]);
	assert(channel[1].status ==
		(CS_TRIGGER_VOICE | CS_UPDATE_VOL | CS_USE_QUICK_VOLRAMP));
	assert(channel[3].status == (CS_UPDATE_VOL | CS_USE_QUICK_VOLRAMP));
	assert(!tapeheadActionPerformanceMutePrevious());
}

static void testPerformanceSoloAndMasterModifierPreserveState(void)
{
	resetFixture(4);
	performanceMute[1] = true;
	assert(tapeheadActionTrackPerformanceSoloToggle(2));
	assert(performanceMute[0] && performanceMute[1] && !performanceMute[2] && performanceMute[3]);
	assert(tapeheadActionTrackPerformanceSoloToggle(2));
	assert(!performanceMute[0] && performanceMute[1] && !performanceMute[2] && !performanceMute[3]);

	assert(tapeheadActionPerformanceMuteMaster());
	for (int32_t i = 0; i < 4; i++) assert(performanceMute[i]);
	/* Phase 4.2 makes the normal Master press self-contained: a second press
	** restores every Performance Mute without requiring Shift. */
	assert(tapeheadActionPerformanceMuteMaster());
	for (int32_t i = 0; i < 4; i++) assert(!performanceMute[i]);
	tapeheadActionSetShiftModifier(true);
	assert(tapeheadActionPerformanceMuteMaster());
	for (int32_t i = 0; i < 4; i++) assert(performanceMute[i]);
	tapeheadActionSetShiftModifier(false);
}

static void testAPCPerformanceEngineActions(void)
{
	resetFixture(8);
	fastTracksPOCSetTrackEnabled(0, true);
	fastMode[0] = FAST_TRACKS_MODE_PATTERN;
	assert(tapeheadActionFastTrackDirectionOrSongMode(0));
	assert(fastReversed[0]);
	tapeheadActionSetShiftModifier(true);
	assert(tapeheadActionFastTrackDirectionOrSongMode(0));
	assert(fastMode[0] == FAST_TRACKS_MODE_SONG);
	tapeheadActionSetShiftModifier(false);
	assert(tapeheadActionFastTrackTransmissionClutchToggle());
	assert(transmissionClutch);
	assert(tapeheadActionFastTrackGlobalReverseToggle());
	assert(!fastReversed[0]);

	fastRatio[0] = FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX;
	assert(tapeheadActionFastTrackRatioAllNext());
	assert(fastRatio[0] == FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX + 1);
	assert(tapeheadActionFastTrackRatioAllPrevious());
	assert(fastRatio[0] == FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX);
}

static void testCursorTempoVolumeRecordAndModes(void)
{
	resetFixture(8);
	assert(tapeheadActionCursorLeft() && tapeheadActionCursorRight());
	assert(tapeheadActionCursorUp() && tapeheadActionCursorDown());
	assert(cursorLeftCount == 1 && cursorRightCount == 1 &&
		cursorUpCount == 1 && cursorDownCount == 1);
	songPlaying = true;
	assert(!tapeheadActionCursorUp() && !tapeheadActionCursorDown());
	assert(cursorUpCount == 1 && cursorDownCount == 1);
	songPlaying = false;
	assert(tapeheadActionMasterVolumeSet(256) && config.masterVol == 256);
	assert(tapeheadActionTempoAdjust(2) && song.BPM == 127);
	assert(tapeheadActionSpeedAdjust(-1) && song.speed == 5);

	assert(tapeheadActionTrackRecordArm(5));
	assert(cursor.ch == 5 && playMode == PLAYMODE_RECPATT);
	assert(tapeheadActionTrackRecordArm(5));
	assert(playMode == PLAYMODE_PATT);
	assert(tapeheadActionTransportModeToggle());
	assert(tapeheadActionTransportPatternModeIsSelected());
	assert(tapeheadActionMatrixVisibilityToggle() && standaloneShown);
}

static void testLayerBanksSequencesAndMatrixMixer(void)
{
	resetFixture(8);
	assert(tapeheadActionMatrixLayerBankSelect(3));
	assert(!sampleDeck && patternPage == 3);
	tapeheadActionSetShiftModifier(true);
	assert(tapeheadActionMatrixLayerBankSelect(5));
	assert(sampleDeck && sampleBank == 5);
	tapeheadActionSetShiftModifier(false);
	assert(tapeheadActionMatrixSetTarget(TAPEHEAD_MATRIX_PATTERN));
	assert(tapeheadActionMatrixBankSelect(0));

	assert(tapeheadActionMatrixSequenceRow(1));
	assert(tapeheadActionMatrixSequenceIsActive());
	assert(tapeheadActionMatrixSequenceGetType() ==
		TAPEHEAD_MATRIX_SEQUENCE_ROW);
	assert(tapeheadActionMatrixSequenceGetControlIndex() == 1);
	assert(lastPatternRequest == 8);
	assert(tapeheadActionMatrixSequenceSlotIsPending(8));
	tapeheadActionMatrixSequenceHandleBoundary();
	assert(lastPatternRequest == 9);
	assert(tapeheadActionMatrixSlotTrigger(2));
	assert(!tapeheadActionMatrixSequenceIsActive());
	assert(patternQueueClearCount >= 2);

	assert(tapeheadActionMatrixMasterVolumeSet(128));
	assert(tapeheadActionMatrixCrossfaderSet(0));
	assert(tapeheadActionMatrixGetQGain() == 128);
	assert(tapeheadActionMatrixGetPolyGain() == 0);
	assert(audioQGain == 128 && audioPolyGain == 0);
	assert(tapeheadActionMatrixCrossfaderSet(64));
	assert(tapeheadActionMatrixGetQGain() == 128);
	assert(tapeheadActionMatrixGetPolyGain() == 128);
	assert(tapeheadActionMatrixCrossfaderSet(127));
	assert(tapeheadActionMatrixGetQGain() == 0);
	assert(tapeheadActionMatrixGetPolyGain() == 128);
}

static void testTransportButtonsAreTrueToggles(void)
{
	resetFixture(8);
	assert(tapeheadActionTransportPlaySongToggle());
	assert(playSongCount == 1 && songPlaying && playMode == PLAYMODE_SONG);
	assert(tapeheadActionTransportPlaySongToggle());
	assert(stopSongCount == 1 && !songPlaying);
	assert(tapeheadActionTransportPlayPatternToggle());
	assert(playPatternCount == 1 && songPlaying && playMode == PLAYMODE_PATT);
	assert(tapeheadActionTransportPlayPatternToggle());
	assert(stopSongCount == 2 && !songPlaying);
}

static void testActiveQCannotDefeatTransportToggles(void)
{
	resetFixture(8);
	patternEnabled = songPlaying = true;
	playMode = PLAYMODE_PATT;
	assert(tapeheadActionTransportPlaySongToggle());
	assert(!patternEnabled);
	assert(playSongCount == 1 && stopSongCount == 1);
	assert(songPlaying && playMode == PLAYMODE_SONG);
	assert(tapeheadActionTransportPlaySongToggle());
	assert(stopSongCount == 2 && !songPlaying);
}

static void testFastTracksGlobalModeAndSelectedLayerStop(void)
{
	resetFixture(8);
	fastTracksPOCSetTrackEnabled(0, true);
	fastTracksPOCSetTrackEnabled(1, true);
	fastMode[0] = FAST_TRACKS_MODE_PATTERN;
	fastMode[1] = FAST_TRACKS_MODE_SONG;
	assert(tapeheadActionFastTrackGlobalModeState() == 2);
	assert(tapeheadActionFastTrackGlobalModeToggle());
	assert(fastMode[0] == FAST_TRACKS_MODE_SONG &&
		fastMode[1] == FAST_TRACKS_MODE_SONG);
	assert(tapeheadActionFastTrackGlobalModeState() == 1);
	assert(tapeheadActionFastTrackGlobalModeToggle());
	assert(fastMode[0] == FAST_TRACKS_MODE_PATTERN &&
		fastMode[1] == FAST_TRACKS_MODE_PATTERN);

	patternEnabled = polyWork = true;
	assert(tapeheadActionTransportStopSelectedDeck());
	assert(!patternEnabled && polyWork);
	assert(tapeheadActionMatrixGridModeToggle());
	assert(tapeheadActionTransportStopSelectedDeck());
	assert(!polyWork);
}

static void testShiftPadSchedulesBothLayersAndNeverLaunches(void)
{
	resetFixture(8);
	patternEnabled = polyWork = true;
	tapeheadActionSetShiftModifier(true);
	assert(tapeheadActionMatrixSlotTrigger(5));
	assert(lastPatternRequest == 5);
	tapeheadActionSetShiftModifier(false);
	assert(tapeheadActionMatrixSlotTrigger(6));
	assert(lastPatternRequest == 6);
}

static void testPatternJogLatchesAndMainStopPreservesMatrix(void)
{
	resetFixture(8);
	editor.editPattern = 0;
	editor.curInstr = 1;
	editor.row = song.row = 0;
	testPattern[(63 * MAX_CHANNELS) + 0].note = 48;
	testPattern[(63 * MAX_CHANNELS) + 0].instr = 1;
	testPattern[(1 * MAX_CHANNELS) + 0].note = NOTE_OFF;

	assert(tapeheadActionPatternJogRelative(-1));
	assert(editor.row == 63 && jogNoteOnCount == 1);
	assert(tapeheadActionPatternJogRelative(1));
	assert(editor.row == 0 && jogNoteOffCount == 0); /* blank row sustains */

	polyWork = sampleWork = true;
	assert(tapeheadActionTransportStop());
	assert(jogNoteOffCount == 1);
	assert(polyWork && sampleWork);

	songPlaying = true;
	playMode = PLAYMODE_PATT;
	song.currNumRows = 64;
	song.row = 10;
	song.tick = 6;
	assert(tapeheadActionPatternJogRelative(-2));
	assert(song.row == 8 && editor.row == 8 && song.tick == 1);
	assert(jogNoteOnCount == 1);
	assert(manualBakeRows == 4); /* both wrapped stopped rows + two playing rows */
	assert(manualBakeEvents == 1); /* note at row 63 */
}

static void testAbsoluteJogHandoffAndShiftStopAllIsolation(void)
{
	resetFixture(8);
	editor.editPattern = 0;
	editor.curInstr = 1;
	editor.row = song.row = 10;
	testPattern[(63 * MAX_CHANNELS) + 0].note = 48;
	testPattern[(63 * MAX_CHANNELS) + 0].instr = 1;

	assert(tapeheadActionPatternJogAbsolute(127));
	assert(editor.row == 63 && jogNoteOnCount == 1);
	assert(tapeheadActionPatternJogRelative(-1));
	assert(editor.row == 62);
	assert(tapeheadActionPatternJogAbsolute(126));
	assert(editor.row == 63 && jogNoteOnCount == 2);
	assert(manualBakeRows == 55); /* 53-row sweep, one reverse, one forward */
	assert(manualBakeEvents == 2);

	patternEnabled = polyWork = sampleWork = songPlaying = true;
	playMode = PLAYMODE_PATT;
	tapeheadActionSetShiftModifier(true);
	assert(tapeheadActionTransportStopDeck());
	assert(jogNoteOffCount == 1);
	assert(songPlaying && patternEnabled && polyWork && sampleWork);

	tapeheadActionSetShiftModifier(false);
	assert(tapeheadActionTransportStopDeck());
	assert(songPlaying);
	assert(!patternEnabled && !polyWork && !sampleWork);
}

static void testBothJogControlsApplyMicrotonalRows(void)
{
	resetFixture(8);
	editor.editPattern = 0;
	editor.curInstr = 1;
	editor.row = song.row = 0;
	testPattern[(1 * MAX_CHANNELS) + 2].note = 48;
	testPattern[(1 * MAX_CHANNELS) + 2].instr = 1;
	testPattern[(1 * MAX_CHANNELS) + 2].efx = TAPEHEAD_EFX_MICROTUNE;
	testPattern[(1 * MAX_CHANNELS) + 2].efxData = 0x87;
	testPattern[(2 * MAX_CHANNELS) + 2].efx = TAPEHEAD_EFX_MICRODRIFT;
	testPattern[(2 * MAX_CHANNELS) + 2].efxData = 0x03;

	/* Cue encoder / relative jog. */
	assert(tapeheadActionPatternJogRelative(1));
	assert(microEffectCount == 1);
	assert(lastMicroChannel == 2 && lastMicroEffect == TAPEHEAD_EFX_MICROTUNE);
	assert(lastMicroParameter == 0x87 && jogNoteOnCount == 1);
	assert(lastManualBakeChannel == 2 &&
		lastManualBakeEffect == TAPEHEAD_EFX_MICROTUNE &&
		lastManualBakeParameter == 0x87);
	assert(tapeheadActionPatternJogRelative(1));
	assert(microEffectCount == 2);
	assert(lastMicroEffect == TAPEHEAD_EFX_MICRODRIFT && lastMicroParameter == 0x03);
	assert(lastManualBakeChannel == 2 &&
		lastManualBakeEffect == TAPEHEAD_EFX_MICRODRIFT &&
		lastManualBakeParameter == 0x03);

	/* Crossfader / absolute jog reaches the same shared audition path. */
	resetFixture(8);
	editor.editPattern = 0;
	editor.curInstr = 1;
	editor.row = song.row = 62;
	testPattern[(63 * MAX_CHANNELS) + 5].note = 48;
	testPattern[(63 * MAX_CHANNELS) + 5].instr = 1;
	testPattern[(63 * MAX_CHANNELS) + 5].efx = TAPEHEAD_EFX_MICROTUNE;
	testPattern[(63 * MAX_CHANNELS) + 5].efxData = 0x79;
	assert(tapeheadActionPatternJogAbsolute(127));
	assert(microEffectCount == 1);
	assert(lastMicroChannel == 5 && lastMicroEffect == TAPEHEAD_EFX_MICROTUNE);
	assert(lastMicroParameter == 0x79 && jogNoteOnCount == 1);
	assert(lastManualBakeChannel == 5 &&
		lastManualBakeEffect == TAPEHEAD_EFX_MICROTUNE &&
		lastManualBakeParameter == 0x79);
}

static void testPatternJogSkipsFastTracksOwnedChannels(void)
{
	resetFixture(8);
	editor.editPattern = 0;
	editor.curInstr = 1;
	editor.row = song.row = 0;

	/* This row is a miniature version of the distributed scale diagnostic:
	** the ordinary channel must strum while the two private-head channels are
	** neither auditioned nor captured by Live Bake. */
	for (int32_t ch = 0; ch < 3; ch++)
	{
		testPattern[(1 * MAX_CHANNELS) + ch].note = (uint8_t)(48 + ch);
		testPattern[(1 * MAX_CHANNELS) + ch].instr = 1;
		testPattern[(1 * MAX_CHANNELS) + ch].efx = TAPEHEAD_EFX_MICROTUNE;
		testPattern[(1 * MAX_CHANNELS) + ch].efxData = (uint8_t)(0x80 + ch);
	}
	fastMode[1] = FAST_TRACKS_MODE_PATTERN;
	fastMode[2] = FAST_TRACKS_MODE_SONG;

	assert(tapeheadActionPatternJogRelative(1));
	assert(jogNoteOnCount == 1);
	assert(microEffectCount == 1 && lastMicroChannel == 0);
	assert(manualBakeRows == 1 && manualBakeEvents == 1);

	/* The absolute crossfader reaches the same filtered shared path. */
	assert(tapeheadActionPatternJogAbsolute(0));
	assert(tapeheadActionPatternJogAbsolute(2)); /* 64 rows: CC 2 -> row 1 */
	assert(jogNoteOnCount == 2);
	assert(microEffectCount == 2 && lastMicroChannel == 0);
	assert(manualBakeEvents == 2);

	/* Include makes both jog controls strum and Live-Bake-capture the same
	** ordinary row on FastTracks-assigned channels without changing modes. */
	tapeheadConfig.patternJogIncludeFastTracks = true;
	assert(tapeheadActionPatternJogAbsolute(0));
	assert(tapeheadActionPatternJogRelative(1));
	assert(jogNoteOnCount == 5 && microEffectCount == 5);
	assert(manualBakeEvents == 5 && lastManualBakeChannel == 2);
	assert(fastMode[1] == FAST_TRACKS_MODE_PATTERN);
	assert(fastMode[2] == FAST_TRACKS_MODE_SONG);

	assert(tapeheadActionPatternJogAbsolute(0));
	assert(tapeheadActionPatternJogAbsolute(2));
	assert(jogNoteOnCount == 8 && microEffectCount == 8);
	assert(manualBakeEvents == 8 && lastManualBakeChannel == 2);
}

static void testSongOrderActionsStopAtBoundaries(void)
{
	resetFixture(8);
	song.songLength = 16;
	editor.songPos = 0;
	assert(!tapeheadActionSongOrderPrevious());
	assert(tapeheadActionSongOrderNext() && editor.songPos == 1);

	/* Shift measures the song-order timeline with the current edit step. All
	** entries deliberately point at the same pattern: positions, not pattern
	** numbers, are the navigation identity. */
	editor.editRowSkip = 4;
	editor.songPos = song.songPos = 0;
	tapeheadActionSetShiftModifier(true);
	assert(tapeheadActionSongOrderNext() && editor.songPos == 4);
	assert(tapeheadActionSongOrderNext() && editor.songPos == 8);
	assert(tapeheadActionSongOrderNext() && editor.songPos == 12);
	assert(tapeheadActionSongOrderNext() && editor.songPos == 15);
	assert(!tapeheadActionSongOrderNext());
	assert(tapeheadActionSongOrderPrevious() && editor.songPos == 11);
	tapeheadActionSetShiftModifier(false);
}

static void testOneShotAndManualPingPongStrumDirections(void)
{
	resetFixture(8);
	editor.editPattern = 0;
	editor.curInstr = 1;
	editor.row = song.row = 1;
	testPattern[0].note = 48;
	testPattern[0].instr = 1;

	/* Momentary is forward-only even while the tape hand moves backward. */
	tapeheadConfig.patternJogAudition =
		TAPEHEAD_PATTERN_JOG_AUDITION_MOMENTARY;
	assert(tapeheadActionPatternJogRelative(-1));
	assert(jogOneShotCount == 1 && jogReverseOneShotCount == 0);
	tapeheadActionPatternJogStopAudition();

	/* Manual PingPong hands the individual sample direction to the gesture. */
	editor.row = song.row = 1;
	tapeheadConfig.patternJogAudition =
		TAPEHEAD_PATTERN_JOG_AUDITION_MANUAL_PINGPONG;
	assert(tapeheadActionPatternJogRelative(-1));
	assert(jogOneShotCount == 2 && jogReverseOneShotCount == 1);
	editor.row = song.row = 63;
	assert(tapeheadActionPatternJogRelative(1));
	assert(jogOneShotCount == 3 && jogReverseOneShotCount == 1);
}

static void testTransportPunchToggleHoldCutAndNavigationConsumption(void)
{
	resetFixture(8);
	song.songLength = 4;
	song.currNumRows = 64;
	song.pattNum = 0;
	songPlaying = true;
	playMode = PLAYMODE_SONG;
	testPattern[0].note = 48;
	testPattern[0].instr = 1;

	/* Toggle sustain: the release edge leaves the scheduler punched out. */
	assert(tapeheadActionTransportPunchPedal(true));
	assert(tapeheadActionTransportPunchIsFrozen());
	assert(!tapeheadActionTransportPunchPedal(false));
	assert(tapeheadActionTransportPunchIsFrozen());
	assert(stopVoicesCount == 0 && stopLauncherVoicesCount == 0);

	/* By default, the row heard at the instant of freeze is consumed. */
	assert(tapeheadActionTransportPunchPedal(true));
	assert(!tapeheadActionTransportPunchIsFrozen());
	assert(punchResumeCount == 1 && punchResumeConsumed);
	assert(!tapeheadActionTransportPunchPedal(false));

	/* Retrigger intentionally restores the freeze/unfreeze double strike. */
	tapeheadConfig.transportFreezeResumeRetrigger = true;
	assert(tapeheadActionTransportPunchPedal(true));
	assert(!tapeheadActionTransportPunchPedal(false));
	assert(tapeheadActionTransportPunchPedal(true));
	assert(punchResumeCount == 2 && !punchResumeConsumed);
	assert(!tapeheadActionTransportPunchPedal(false));
	tapeheadConfig.transportFreezeResumeRetrigger = false;

	/* Silent tape-head movement makes the new row pending on resume. */
	tapeheadConfig.patternJogAudition = TAPEHEAD_PATTERN_JOG_AUDITION_OFF;
	assert(tapeheadActionTransportPunchPedal(true));
	assert(!tapeheadActionTransportPunchPedal(false));
	assert(tapeheadActionPatternJogRelative(1));
	assert(tapeheadActionTransportPunchPedal(true));
	assert(punchResumeCount == 3 && !punchResumeConsumed);
	assert(!tapeheadActionTransportPunchPedal(false));
	tapeheadConfig.patternJogAudition = TAPEHEAD_PATTERN_JOG_AUDITION_LATCHED;

	assert(tapeheadActionTransportPunchPedal(true));
	assert(!tapeheadActionTransportPunchPedal(false));

	/* Silent relocation is not consumed; row 00 must sound on resume. */
	assert(tapeheadActionSongOrderNext() && editor.songPos == 1);
	assert(jogNoteOnCount == 0);
	assert(tapeheadActionTransportPunchPedal(true));
	assert(!tapeheadActionTransportPunchIsFrozen());
	assert(punchResumeCount == 4 && !punchResumeConsumed);
	assert(!tapeheadActionTransportPunchPedal(false));

	/* Audition relocation sounds row 00 and consumes it exactly once. */
	tapeheadConfig.transportFreezeNavigationAudition = true;
	tapeheadConfig.patternJogAudition = TAPEHEAD_PATTERN_JOG_AUDITION_OFF;
	assert(tapeheadActionTransportPunchPedal(true));
	assert(!tapeheadActionTransportPunchPedal(false));
	assert(tapeheadActionSongOrderNext() && editor.songPos == 2);
	assert(jogNoteOnCount == 1 && jogOneShotCount == 1);
	assert(tapeheadActionTransportPunchPedal(true));
	assert(punchResumeCount == 5 && punchResumeConsumed);
	assert(!tapeheadActionTransportPunchPedal(false));

	/* Hold+Cut freezes only while down and silences every voice pool once. */
	tapeheadConfig.transportFreezePedalHold = true;
	tapeheadConfig.transportFreezeAudioCut = true;
	assert(tapeheadActionTransportPunchPedal(true));
	assert(tapeheadActionTransportPunchIsFrozen());
	assert(stopVoicesCount == 1 && stopLauncherVoicesCount == 1);
	assert(tapeheadActionTransportPunchPedal(false));
	assert(!tapeheadActionTransportPunchIsFrozen());
}

int main(void)
{
	testPerformanceMuteToggleUsesQuickRamp();
	testTrackBoundsRejectInactiveChannels();
	testUnmuteAllKeepsOrdinaryMutesSeparate();
	testTrackTrimClampsAndRefreshesVolume();
	testRevealHistoryIsDeterministic();
	testOrdinaryMuteAndUnmuteAllRemainExplicit();
	testTrackSelectionAndFastTracksActions();
	testMatrixFocusBanksAndSlotsAreIndependent();
	testTransportActionsRemainDistinct();
	testModuleLoadClearsPerformanceRuntime();
	testPerformanceSoloAndMasterModifierPreserveState();
	testAPCPerformanceEngineActions();
	testCursorTempoVolumeRecordAndModes();
	testLayerBanksSequencesAndMatrixMixer();
	testTransportButtonsAreTrueToggles();
	testActiveQCannotDefeatTransportToggles();
	testFastTracksGlobalModeAndSelectedLayerStop();
	testShiftPadSchedulesBothLayersAndNeverLaunches();
	testPatternJogLatchesAndMainStopPreservesMatrix();
	testAbsoluteJogHandoffAndShiftStopAllIsolation();
	testBothJogControlsApplyMicrotonalRows();
	testPatternJogSkipsFastTracksOwnedChannels();
	testSongOrderActionsStopAtBoundaries();
	testOneShotAndManualPingPongStrumDirections();
	testTransportPunchToggleHoldCutAndNavigationConsumption();
	puts("24 native Tapehead action groups passed.");
	return 0;
}
