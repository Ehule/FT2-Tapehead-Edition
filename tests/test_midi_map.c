#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL_atomic.h>
#include "ft2_midi_map.h"
#include "ft2_config.h"
#include "ft2_fasttracks.h"
#include "ft2_replayer.h"
#include "ft2_audio.h"
#include "ft2_structs.h"
#include "ft2_tapehead_actions.h"

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

uint32_t SDL_GetTicks(void) { return 0; }

static bool fastSelected[MAX_CHANNELS], fastReversed[MAX_CHANNELS];
static bool fastClutched[MAX_CHANNELS], fastMaster, transmissionClutch;
static bool lengthTopologyBypassed;
static fastTracksMode_t fastMode[MAX_CHANNELS];
static uint8_t fastRatio[MAX_CHANNELS];
static uint16_t fastTrackLength[MAX_CHANNELS];
static int8_t fastControlTrack;
static bool sampleDeck, patternExposed[MAX_PATTERNS];
static uint8_t patternPage, sampleBank;
static int32_t lastPatternRequest, lastSampleRequest;
static bool patternEnabled, polyWork, sampleWork;
static uint32_t playSongCount, playPatternCount, stopSongCount;
static uint32_t stopDeckCount, stopAllCount;
static bool standaloneShown, sampleMorphArmed;
static note_t testPattern[MAX_PATT_LEN * MAX_CHANNELS];
static int32_t jogToneCount;

void redrawScopeChannel(int32_t channelIndex) { (void)channelIndex; }
void cursorLeft(void) { }
void cursorRight(void) { }
void cursorTabLeft(void) { }
void cursorTabRight(void) { }
void rowOneUpWrap(void) { }
void rowOneDownWrap(void) { }
void decSongPos(void) { if (editor.songPos > 0) editor.songPos--; }
void incSongPos(void)
{
	if (editor.songPos + 1 < song.songLength) editor.songPos++;
}
void bakerBeginManualRow(void) { }
void bakerCaptureManualEvent(int32_t channelIndex, const note_t *event)
{ (void)channelIndex; (void)event; }
bool bakerLiveIsCapturing(void) { return false; }
bool bakerLiveIsArmed(void) { return false; }
void setAudioAmp(int16_t amp, int16_t masterVol, bool bitDepth32Flag)
{ (void)amp; (void)masterVol; (void)bitDepth32Flag; }
void pbBPMUp(void) { if (song.BPM < MAX_BPM) song.BPM++; }
void pbBPMDown(void) { if (song.BPM > MIN_BPM) song.BPM--; }
void pbSpeedUp(void) { if (song.speed < MAX_SPEED) song.speed++; }
void pbSpeedDown(void) { if (song.speed > 0) song.speed--; }
bool sampleMorphToggleArmed(void) { sampleMorphArmed ^= 1; return true; }
bool sampleMorphSetFromController(int32_t channelIndex, uint8_t value)
{ (void)channelIndex; (void)value; return sampleMorphArmed; }
bool sampleMorphStepAll(int32_t delta)
{ (void)delta; return sampleMorphArmed; }
void sampleMorphResetForLoadedModule(void) { sampleMorphArmed = false; }
void audioSetMatrixMixerGains(uint16_t qGain, uint16_t polyGain)
{ (void)qGain; (void)polyGain; }
void lockAudio(void) { audio.locked = true; }
void unlockAudio(void) { audio.locked = false; }
bool undoPatternBegin(uint16_t patternNum, const char *description)
{ (void)patternNum; (void)description; return true; }
void undoPatternCommit(void) { }
void setSongModifiedFlag(void) { song.isModified = true; }
void resetSyncQueues(void) { }
void stopVoice(int32_t voiceIndex) { (void)voiceIndex; }
void stopVoices(void) { }
void audioSampleLauncherStopAll(void) { }
void tapeheadReplayerResumeTransportPunch(bool consumed) { (void)consumed; }
void tapeheadReplayerBeginTransportPunch(void) { }
void applyChannelMicrotonalEffect(uint8_t channelIndex, uint8_t effect, uint8_t parameter)
{ (void)channelIndex; (void)effect; (void)parameter; }
void playTone(uint8_t chNum, uint8_t insNum, uint8_t note, int8_t vol,
	uint16_t midiVibDepth, uint16_t midiPitch)
{
	(void)chNum; (void)insNum; (void)note; (void)vol;
	(void)midiVibDepth; (void)midiPitch; jogToneCount++;
}
void playToneOneShot(uint8_t chNum, uint8_t insNum, uint8_t note, int8_t vol,
	uint16_t midiVibDepth, uint16_t midiPitch, bool reverse)
{
	(void)reverse;
	playTone(chNum, insNum, note, vol, midiVibDepth, midiPitch);
}
void setNewSongPos(int32_t pos)
{
	song.songPos = editor.songPos = (int16_t)pos;
	song.pattNum = song.orders[pos];
	song.row = editor.row = 0;
}
void tapeheadReplayerSetTransportPunchSongPos(int32_t pos)
{ setNewSongPos(pos); }

void setChannelMute(int32_t channelIndex, bool off)
{
	channel[channelIndex].channelOff = off;
}
void jumpToChannel(uint8_t channelIndex) { cursor.ch = channelIndex; }
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
bool fastTracksPOCLengthTopologyIsBypassed(void)
{ return lengthTopologyBypassed; }
void fastTracksPOCToggleLengthTopologyBypass(void)
{ lengthTopologyBypassed ^= 1; }
uint16_t fastTracksPOCGetTrackLength(uint16_t patternNumber, int32_t i)
{ (void)patternNumber; return fastTrackLength[i]; }
void fastTracksPOCSetTrackLength(uint16_t patternNumber, int32_t i,
	uint16_t length)
{ (void)patternNumber; fastTrackLength[i] = length; }
int8_t fastTracksPOCGetControlTrack(uint16_t patternNumber)
{ (void)patternNumber; return fastControlTrack; }
void fastTracksPOCSetControlTrack(uint16_t patternNumber, int32_t channelIndex)
{ (void)patternNumber; fastControlTrack = (int8_t)channelIndex; }
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
void patternLauncherClearQueue(void) { }
void patternLauncherForceRedraw(void) { }
bool patternLauncherPatternIsExposed(uint8_t patternNum)
{
	return patternExposed[patternNum];
}
bool patternLauncherTileIsLaunchable(uint8_t patternNum)
{
	return patternLauncherPatternIsExposed(patternNum);
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
void sampleLauncherClearQQueue(void) { }
bool sampleLauncherTileIsLoaded(uint16_t tile) { return tile < 256; }
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

/* The production build uses SDL's platform atomics. These test-local stubs
** keep the native suite independent from an installed SDL development lib. */
void SDLCALL SDL_AtomicLock(SDL_SpinLock *lock)
{
	while (__sync_lock_test_and_set(lock, 1))
	{
	}
}

void SDLCALL SDL_AtomicUnlock(SDL_SpinLock *lock)
{
	__sync_lock_release(lock);
}

static void resetFixture(void)
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
	memset(fastTrackLength, 0, sizeof (fastTrackLength));
	fastControlTrack = -1;
	memset(patternExposed, 1, sizeof (patternExposed));
	memset(pattern, 0, sizeof (pattern));
	memset(patternNumRows, 0, sizeof (patternNumRows));
	memset(testPattern, 0, sizeof (testPattern));
	pattern[0] = testPattern;
	patternNumRows[0] = 64;
	memset(&song, 0, sizeof (song));
	song.numChannels = 8;

	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		channelVolumeTrim[i] = TAPEHEAD_TRACK_TRIM_UNITY;
	sampleDeck = fastMaster = songPlaying = transmissionClutch = false;
	lengthTopologyBypassed = false;
	standaloneShown = sampleMorphArmed = false;
	playMode = PLAYMODE_IDLE;
	memset(&ui, 0, sizeof (ui));
	memset(&config, 0, sizeof (config));
	memset(&tapeheadConfig, 0, sizeof (tapeheadConfig));
	tapeheadConfig.trackTrimMaxPercent = 200;
	tapeheadConfig.trackTrimDisplayWidth = 2;
	tapeheadConfig.trackLengthControlMax = MAX_PATT_LEN;
	tapeheadConfig.controlTrackLeftStart = 1;
	tapeheadConfig.controlTrackRightStart = 8;
	tapeheadConfig.controlTrackNavigationWrap = true;
	tapeheadConfig.patternJogAudition = TAPEHEAD_PATTERN_JOG_AUDITION_LATCHED;
	memset(&audio, 0, sizeof (audio));
	song.BPM = 125;
	song.speed = 6;
	patternEnabled = polyWork = sampleWork = false;
	patternPage = sampleBank = 0;
	lastPatternRequest = lastSampleRequest = -1;
	playSongCount = playPatternCount = stopSongCount = 0;
	stopDeckCount = stopAllCount = 0;
	jogToneCount = 0;
	tapeheadActionsResetForLoadedModule();

	tapeheadMidiMapReset();
}

static void addTestBindings(void)
{
	assert(tapeheadMidiMapAddBinding("NoteOn.1.48",
		"TrackPerformanceMuteToggle:1"));
	assert(tapeheadMidiMapAddBinding("Note.1.49", "PerformanceUnmuteAll"));
	assert(tapeheadMidiMapAddBinding("CC.1.7", "TrackTrim:1"));
	assert(tapeheadMidiMapGetBindingCount() == 3);
}

static void testParserRejectsAmbiguousOrInvalidMappings(void)
{
	resetFixture();
	assert(!tapeheadMidiMapAddBinding("NoteOn.48", "PerformanceUnmuteAll"));
	assert(!tapeheadMidiMapAddBinding("NoteOn.0.48", "PerformanceUnmuteAll"));
	assert(!tapeheadMidiMapAddBinding("NoteOn.17.48", "PerformanceUnmuteAll"));
	assert(!tapeheadMidiMapAddBinding("NoteOff.1.48", "PerformanceUnmuteAll"));
	assert(!tapeheadMidiMapAddBinding("CC.1.128", "TrackTrim:1"));
	assert(!tapeheadMidiMapAddBinding("CC.1.7", "TrackTrim:0"));
	assert(!tapeheadMidiMapAddBinding("CC.1.7", "TrackTrim:33"));
	assert(!tapeheadMidiMapAddBinding("CC.1.7", "TrackPerformanceMuteToggle:1"));
	assert(tapeheadMidiMapGetBindingCount() == 0);
}

static void testDisabledAndUnmappedMessagesPassThrough(void)
{
	resetFixture();
	addTestBindings();

	assert(!tapeheadMidiMapHandleMessage(0x90, 48, 100));
	assert(tapeheadMidiMapGetPendingCount() == 0);

	tapeheadMidiMapSetEnabled(true);
	assert(!tapeheadMidiMapHandleMessage(0x91, 48, 100));
	assert(!tapeheadMidiMapHandleMessage(0x90, 50, 100));
	assert(!tapeheadMidiMapHandleMessage(0xE0, 48, 100));
	assert(tapeheadMidiMapGetPendingCount() == 0);
}

static void testNoteActionsUsePressEdgeAndMainThreadDrain(void)
{
	resetFixture();
	addTestBindings();
	tapeheadMidiMapSetEnabled(true);

	assert(tapeheadMidiMapHandleMessage(0x90, 48, 100));
	assert(!performanceMute[0]); /* callback-side injection cannot mutate state */
	assert(tapeheadMidiMapGetPendingCount() == 1);

	assert(tapeheadMidiMapHandleMessage(0x80, 48, 0));
	assert(tapeheadMidiMapGetPendingCount() == 1);
	tapeheadMidiMapProcessPending();
	assert(performanceMute[0]);
	assert(channel[0].status & CS_UPDATE_VOL);
	assert(channel[0].status & CS_USE_QUICK_VOLRAMP);

	channel[0].status = 0;
	assert(tapeheadMidiMapHandleMessage(0x90, 48, 0));
	tapeheadMidiMapProcessPending();
	assert(performanceMute[0]);
	assert(channel[0].status == 0);

	performanceMute[1] = true;
	assert(tapeheadMidiMapHandleMessage(0x90, 49, 127));
	tapeheadMidiMapProcessPending();
	assert(!performanceMute[0]);
	assert(!performanceMute[1]);
}

static void testAPCPressReleaseSequencesRemainTwoState(void)
{
	resetFixture();
	assert(tapeheadMidiMapAddBinding("NoteOn.1.48",
		"TrackPerformanceSoloToggle:1"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.49",
		"FastTrackClutchToggle:1"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.50",
		"FastTrackMasterToggle"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.51", "FastTrackResetAll"));
	tapeheadMidiMapSetEnabled(true);
	song.numChannels = 3;
	fastSelected[0] = true;
	performanceMute[0] = true;
	performanceMute[1] = false;
	performanceMute[2] = true;

	/* A press dispatches once; both legal release encodings are consumed and
	** never enqueue an action. The next press restores the exact solo snapshot. */
	assert(tapeheadMidiMapHandleMessage(0x90, 48, 127));
	assert(tapeheadMidiMapHandleMessage(0x80, 48, 64));
	tapeheadMidiMapProcessPending();
	assert(!performanceMute[0] && performanceMute[1] && performanceMute[2]);
	assert(tapeheadMidiMapHandleMessage(0x90, 48, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 48, 0));
	tapeheadMidiMapProcessPending();
	assert(performanceMute[0] && !performanceMute[1] && performanceMute[2]);

	for (int32_t i = 0; i < 10; i++)
	{
		assert(tapeheadMidiMapHandleMessage(0x90, 49, 127));
		assert(tapeheadMidiMapHandleMessage((i & 1) ? 0x80 : 0x90,
			49, 0));
		tapeheadMidiMapProcessPending();
		assert(fastClutched[0] == ((i & 1) == 0));
	}

	/* The master toggle remains responsive with the lowest (1:2) ratio. */
	fastRatio[0] = 0;
	for (int32_t i = 0; i < 10; i++)
	{
		assert(tapeheadMidiMapHandleMessage(0x90, 50, 127));
		assert(tapeheadMidiMapHandleMessage(0x80, 50, 0));
		tapeheadMidiMapProcessPending();
		assert(fastMaster == ((i & 1) == 0));
	}

	fastRatio[0] = 16;
	assert(tapeheadMidiMapHandleMessage(0x90, 51, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 51, 0));
	tapeheadMidiMapProcessPending();
	assert(fastRatio[0] == FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX);
}

static void testAbsoluteEncoderMovement(void)
{
	resetFixture();
	assert(tapeheadMidiMapAddBinding("CC.1.48", "FastTrackRatio:1"));
	assert(tapeheadMidiMapAddBinding("CC.1.16", "SampleMorphSelect:1"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.41", "ShiftModifier"));
	tapeheadMidiMapSetEnabled(true);
	song.numChannels = 1;
	fastSelected[0] = true;

	/* The first physical message after a ring refresh, including a value equal
	** to the displayed value, remains valid input. Output itself is tested at
	** the APC surface boundary and never calls this input handler. */
	assert(tapeheadMidiMapHandleMessage(0xB0, 48, 56));
	assert(tapeheadMidiMapGetPendingCount() == 1);
	tapeheadMidiMapProcessPending();
	assert(fastRatio[0] == FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX);

	/* Each distinct absolute position selects exactly one of all 17 ratios. */
	for (uint8_t ratio = 0; ratio < 17; ratio++)
	{
		const uint8_t value = (uint8_t)((ratio * 127 + 8) / 16);
		assert(tapeheadMidiMapHandleMessage(0xB0, 48, value));
		tapeheadMidiMapProcessPending();
		assert(fastRatio[0] == ratio);
	}

	/* Descending one-message-at-a-time input is equally responsive. */
	for (int32_t ratio = 16; ratio >= 0; ratio--)
	{
		const uint8_t value = (uint8_t)((ratio * 127 + 8) / 16);
		assert(tapeheadMidiMapHandleMessage(0xB0, 48, value));
		tapeheadMidiMapProcessPending();
		assert(fastRatio[0] == ratio);
	}

	/* Playback state must not alter dispatch: all ratios remain reachable in
	** both directions while the transport is running. */
	songPlaying = true;
	for (uint8_t ratio = 0; ratio < 17; ratio++)
	{
		const uint8_t value = (uint8_t)((ratio * 127 + 8) / 16);
		assert(tapeheadMidiMapHandleMessage(0xB0, 48, value));
		tapeheadMidiMapProcessPending();
		assert(fastRatio[0] == ratio);
	}
	for (int32_t ratio = 16; ratio >= 0; ratio--)
	{
		const uint8_t value = (uint8_t)((ratio * 127 + 8) / 16);
		assert(tapeheadMidiMapHandleMessage(0xB0, 48, value));
		tapeheadMidiMapProcessPending();
		assert(fastRatio[0] == ratio);
	}

	/* Returning from 1:2 to centered 1:1 takes the first movement. */
	assert(tapeheadMidiMapHandleMessage(0xB0, 48, 0));
	tapeheadMidiMapProcessPending();
	assert(fastRatio[0] == 0);
	assert(tapeheadMidiMapHandleMessage(0xB0, 48, 56));
	tapeheadMidiMapProcessPending();
	assert(fastRatio[0] == FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX);

	/* Shift temporarily gives this same absolute track control to song-wide
	** LEN. Zero is OFF; the top stop follows the configured ceiling. */
	tapeheadConfig.trackLengthControlMax = 64;
	assert(tapeheadMidiMapHandleMessage(0x90, 41, 127));
	tapeheadMidiMapProcessPending();
	assert(tapeheadActionShiftModifierIsHeld());
	const uint8_t heldRatio = fastRatio[0];
	assert(tapeheadMidiMapHandleMessage(0xB0, 48, 1));
	tapeheadMidiMapProcessPending();
	assert(fastTrackLength[0] == 1 && fastRatio[0] == heldRatio);
	assert(tapeheadMidiMapHandleMessage(0xB0, 48, 127));
	tapeheadMidiMapProcessPending();
	assert(fastTrackLength[0] == 64 && fastRatio[0] == heldRatio);
	assert(tapeheadMidiMapHandleMessage(0xB0, 48, 0));
	tapeheadMidiMapProcessPending();
	assert(fastTrackLength[0] == 0 && fastRatio[0] == heldRatio);
	assert(tapeheadMidiMapHandleMessage(0x80, 41, 0));
	tapeheadMidiMapProcessPending();
	assert(!tapeheadActionShiftModifierIsHeld());
	assert(tapeheadMidiMapHandleMessage(0xB0, 48, 127));
	tapeheadMidiMapProcessPending();
	assert(fastRatio[0] == 16);

	/* APC encoder touch is a Note message, not a CC value. With only the CC
	** movement bound, touch-on/off cannot cycle either selector. */
	assert(!tapeheadMidiMapHandleMessage(0x90, 53, 127));
	assert(!tapeheadMidiMapHandleMessage(0x80, 53, 0));
	assert(tapeheadMidiMapGetPendingCount() == 0);
	assert(tapeheadMidiMapHandleMessage(0xB0, 16, 64));
	assert(tapeheadMidiMapGetPendingCount() == 1);
	tapeheadMidiMapProcessPending();
}

static void testCCFloodCoalescesToNewestValue(void)
{
	resetFixture();
	addTestBindings();
	tapeheadMidiMapSetEnabled(true);

	for (uint8_t value = 0; value < 127; value++)
		assert(tapeheadMidiMapHandleMessage(0xB0, 7, value));
	assert(tapeheadMidiMapHandleMessage(0xB0, 7, 127));

	assert(tapeheadMidiMapGetPendingCount() == 1);
	assert(tapeheadMidiMapGetDroppedEventCount() == 0);
	assert(channelVolumeTrim[0] == TAPEHEAD_TRACK_TRIM_UNITY);
	tapeheadMidiMapProcessPending();
	assert(channelVolumeTrim[0] == TAPEHEAD_TRACK_TRIM_MAX);
	assert(channel[0].status & CS_UPDATE_VOL);
}

static void testQueueIsBoundedAndDropsWithoutLeaking(void)
{
	resetFixture();
	assert(tapeheadMidiMapAddBinding("NoteOn.1.48",
		"TrackPerformanceMuteToggle:1"));
	tapeheadMidiMapSetEnabled(true);

	for (size_t i = 0; i < TAPEHEAD_MIDI_EVENT_QUEUE_CAPACITY + 7; i++)
		assert(tapeheadMidiMapHandleMessage(0x90, 48, 127));

	assert(tapeheadMidiMapGetPendingCount() ==
		TAPEHEAD_MIDI_EVENT_QUEUE_CAPACITY);
	assert(tapeheadMidiMapGetDroppedEventCount() == 7);
	tapeheadMidiMapProcessPending();
	assert(tapeheadMidiMapGetPendingCount() == 0);
	assert(!performanceMute[0]); /* 128 accepted toggles */
}

static void testDuplicateInputUsesLastBinding(void)
{
	resetFixture();
	assert(tapeheadMidiMapAddBinding("NoteOn.1.48",
		"TrackPerformanceMuteToggle:1"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.48",
		"TrackPerformanceMuteToggle:2"));
	assert(tapeheadMidiMapGetBindingCount() == 1);
	tapeheadMidiMapSetEnabled(true);

	assert(tapeheadMidiMapHandleMessage(0x90, 48, 100));
	tapeheadMidiMapProcessPending();
	assert(!performanceMute[0]);
	assert(performanceMute[1]);
}

static void testExtendedPerformanceCommandVocabulary(void)
{
	resetFixture();
	const struct
	{
		const char *input, *action;
	} bindings[] =
	{
		{ "NoteOn.1.60", "TrackSelect:4" },
		{ "NoteOn.1.61", "TrackMuteToggle:2" },
		{ "NoteOn.1.62", "PerformanceUnmuteNext" },
		{ "NoteOn.1.63", "PerformanceMutePrevious" },
		{ "NoteOn.1.64", "FastTrackToggle:3" },
		{ "NoteOn.1.65", "FastTrackMasterToggle" },
		{ "NoteOn.1.66", "FastTrackRatioNext:3" },
		{ "NoteOn.1.67", "FastTrackRatioPrevious:3" },
		{ "NoteOn.1.68", "FastTrackRatioReset:3" },
		{ "NoteOn.1.69", "FastTrackResetAll" },
		{ "NoteOn.1.70", "FastTrackReverseToggle:3" },
		{ "NoteOn.1.71", "FastTrackClutchToggle:3" },
		{ "NoteOn.1.72", "MatrixModeSample" },
		{ "NoteOn.1.73", "MatrixModePattern" },
		{ "NoteOn.1.74", "MatrixModeToggle" },
		{ "NoteOn.1.75", "MatrixBankSelect:4" },
		{ "NoteOn.1.76", "MatrixBankNext" },
		{ "NoteOn.1.77", "MatrixBankPrevious" },
		{ "NoteOn.1.78", "MatrixSlotTrigger:32" },
		{ "NoteOn.1.79", "TransportPlaySong" },
		{ "NoteOn.1.80", "TransportPlayPattern" },
		{ "NoteOn.1.81", "TransportStopSong" },
		{ "NoteOn.1.82", "TransportStopDeck" },
		{ "NoteOn.1.83", "TransportStopAll" },
		{ "NoteOn.1.84", "UnmuteAll" },
		{ "CC.1.48", "FastTrackRatio:3" }
	};

	for (size_t i = 0; i < sizeof (bindings) / sizeof (bindings[0]); i++)
		assert(tapeheadMidiMapAddBinding(bindings[i].input, bindings[i].action));
	assert(tapeheadMidiMapGetBindingCount() ==
		sizeof (bindings) / sizeof (bindings[0]));

	assert(!tapeheadMidiMapAddBinding("NoteOn.1.85", "MatrixBankSelect:0"));
	assert(!tapeheadMidiMapAddBinding("NoteOn.1.85", "MatrixBankSelect:9"));
	assert(!tapeheadMidiMapAddBinding("NoteOn.1.85", "MatrixSlotTrigger:33"));
	assert(!tapeheadMidiMapAddBinding("NoteOn.1.85", "FastTrackRatio:1"));
	assert(!tapeheadMidiMapAddBinding("CC.1.49", "MatrixModeToggle"));

	tapeheadMidiMapSetEnabled(true);
	assert(tapeheadMidiMapHandleMessage(0x90, 60, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 61, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 64, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 65, 127));
	assert(tapeheadMidiMapHandleMessage(0xB0, 48, 127));
	tapeheadMidiMapProcessPending();
	assert(cursor.ch == 3);
	assert(editor.channelMuted[1] && channel[1].channelOff);
	assert(fastSelected[2] && fastMaster && fastRatio[2] == 16);

	performanceMute[0] = true;
	assert(tapeheadMidiMapHandleMessage(0x90, 62, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 63, 127));
	tapeheadMidiMapProcessPending();
	assert(performanceMute[0]);

	assert(tapeheadMidiMapHandleMessage(0x90, 72, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 75, 127));
	tapeheadMidiMapProcessPending();
	assert(sampleDeck && sampleBank == 3);
	sampleWork = true;
	assert(tapeheadMidiMapHandleMessage(0x90, 78, 127));
	tapeheadMidiMapProcessPending();
	assert(lastSampleRequest == 127);

	assert(tapeheadMidiMapHandleMessage(0x90, 79, 127));
	tapeheadMidiMapProcessPending();
	assert(playSongCount == 1 && songPlaying);
	assert(tapeheadMidiMapHandleMessage(0x90, 81, 127));
	tapeheadMidiMapProcessPending();
	assert(stopSongCount == 1 && !songPlaying);
}

static void testPhase42VocabularyAndDispatch(void)
{
	resetFixture();
	assert(tapeheadMidiMapAddBinding("NoteOn.1.40",
		"MatrixLayerBankSelect:3"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.41", "ShiftModifier"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.42",
		"MatrixSequenceRow:1"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.43",
		"MatrixSequenceColumn:8"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.44",
		"MatrixSequenceBank"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.45",
		"TransportPlaySongToggle"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.46",
		"TransportPlayPatternToggle"));
	assert(tapeheadMidiMapAddBinding("CC.1.15", "MatrixCrossfader"));
	assert(tapeheadMidiMapAddBinding("CC.1.47", "MatrixMasterVolume"));

	tapeheadMidiMapSetEnabled(true);
	assert(tapeheadMidiMapHandleMessage(0x90, 40, 127));
	tapeheadMidiMapProcessPending();
	assert(!sampleDeck && patternPage == 2);

	assert(tapeheadMidiMapHandleMessage(0x90, 41, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 40, 127));
	tapeheadMidiMapProcessPending();
	assert(sampleDeck && sampleBank == 2);
	assert(tapeheadMidiMapHandleMessage(0x80, 41, 0));
	tapeheadMidiMapProcessPending();

	assert(tapeheadMidiMapHandleMessage(0xB0, 15, 0));
	assert(tapeheadMidiMapHandleMessage(0xB0, 47, 64));
	tapeheadMidiMapProcessPending();
	assert(tapeheadActionMatrixGetQGain() == 129);
	assert(tapeheadActionMatrixGetPolyGain() == 0);

	assert(tapeheadMidiMapHandleMessage(0x90, 45, 127));
	tapeheadMidiMapProcessPending();
	assert(songPlaying && playMode == PLAYMODE_SONG);
	assert(tapeheadMidiMapHandleMessage(0x90, 45, 127));
	tapeheadMidiMapProcessPending();
	assert(!songPlaying);
}

static void testPhase431JogAndModeEventsAreNotCollapsed(void)
{
	resetFixture();
	assert(tapeheadMidiMapAddBinding("CC.1.47", "PatternJogRelative"));
	assert(tapeheadMidiMapAddBinding("CC.1.15", "PatternJogAbsolute"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.92", "TransportStop"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.99",
		"FastTrackGlobalModeToggle"));
	tapeheadMidiMapSetEnabled(true);

	editor.editPattern = 0;
	editor.row = song.row = 10;
	assert(tapeheadMidiMapHandleMessage(0xB0, 47, 127));
	assert(tapeheadMidiMapHandleMessage(0xB0, 47, 127));
	assert(tapeheadMidiMapGetPendingCount() == 2);
	tapeheadMidiMapProcessPending();
	assert(editor.row == 8); /* every relative detent survived the queue */

	/* Absolute crossfader messages also survive. The relative encoder takes
	** over from the new row, and the next crossfader move reasserts its
	** physical absolute position. */
	assert(tapeheadMidiMapHandleMessage(0xB0, 15, 127));
	assert(tapeheadMidiMapHandleMessage(0xB0, 47, 127));
	assert(tapeheadMidiMapHandleMessage(0xB0, 15, 126));
	assert(tapeheadMidiMapGetPendingCount() == 3);
	tapeheadMidiMapProcessPending();
	assert(editor.row == 63);

	fastTracksPOCSetTrackEnabled(0, true);
	fastMode[0] = FAST_TRACKS_MODE_PATTERN;
	assert(tapeheadMidiMapHandleMessage(0x90, 99, 127));
	tapeheadMidiMapProcessPending();
	assert(fastMode[0] == FAST_TRACKS_MODE_SONG);

	songPlaying = true;
	playMode = PLAYMODE_SONG;
	assert(tapeheadMidiMapHandleMessage(0x90, 92, 127));
	tapeheadMidiMapProcessPending();
	assert(!songPlaying && stopSongCount == 1);
}

static void testTransportPunchEdgesAndShiftedClutchSafety(void)
{
	resetFixture();
	assert(tapeheadMidiMapAddBinding("CC.1.64", "TransportPunch"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.41", "ShiftModifier"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.71",
		"FastTrackClutchToggle:3"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.63", "FastTrackResetAll"));
	tapeheadMidiMapSetEnabled(true);
	songPlaying = true;
	playMode = PLAYMODE_SONG;

	/* A switch CC is not a fader: press and release must both survive the
	** queue or Toggle/Hold behavior becomes nondeterministic. */
	assert(tapeheadMidiMapHandleMessage(0xB0, 64, 127));
	assert(tapeheadMidiMapHandleMessage(0xB0, 64, 0));
	assert(tapeheadMidiMapGetPendingCount() == 2);
	tapeheadMidiMapProcessPending();
	assert(tapeheadActionTransportPunchIsFrozen());
	assert(tapeheadMidiMapHandleMessage(0xB0, 64, 127));
	assert(tapeheadMidiMapHandleMessage(0xB0, 64, 0));
	tapeheadMidiMapProcessPending();
	assert(!tapeheadActionTransportPunchIsFrozen());

	fastTracksPOCSetTrackEnabled(2, true);
	assert(tapeheadMidiMapHandleMessage(0x90, 41, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 71, 127));
	tapeheadMidiMapProcessPending();
	assert(!fastClutched[2]); /* Shift + Record Arm is deliberately unassigned. */
	assert(tapeheadMidiMapHandleMessage(0x80, 41, 0));
	assert(tapeheadMidiMapHandleMessage(0x90, 71, 127));
	tapeheadMidiMapProcessPending();
	assert(fastClutched[2]);

	/* APC Shift + Device Lock reaches the same runtime LEN bypass as the
	** keyboard action. Releasing Shift restores Device Lock's original ratio
	** reset without altering the bypass state. */
	fastTracksPOCSetTrackEnabled(0, true);
	fastRatio[0] = 16;
	assert(tapeheadMidiMapHandleMessage(0x90, 41, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 63, 127));
	assert(tapeheadMidiMapHandleMessage(0x80, 63, 0));
	tapeheadMidiMapProcessPending();
	assert(lengthTopologyBypassed);
	assert(fastRatio[0] == 16);

	assert(tapeheadMidiMapHandleMessage(0x80, 41, 0));
	assert(tapeheadMidiMapHandleMessage(0x90, 63, 127));
	assert(tapeheadMidiMapHandleMessage(0x80, 63, 0));
	tapeheadMidiMapProcessPending();
	assert(lengthTopologyBypassed);
	assert(fastRatio[0] == FAST_TRACKS_ONE_TO_ONE_RATIO_INDEX);

	assert(tapeheadMidiMapHandleMessage(0x90, 41, 127));
	assert(tapeheadMidiMapHandleMessage(0x90, 63, 127));
	assert(tapeheadMidiMapHandleMessage(0x80, 63, 0));
	assert(tapeheadMidiMapHandleMessage(0x80, 41, 0));
	tapeheadMidiMapProcessPending();
	assert(!lengthTopologyBypassed);
}

static void testControlTrackNavigationVocabularyAndDispatch(void)
{
	resetFixture();
	assert(tapeheadMidiMapAddBinding("NoteOn.1.96",
		"TrackLengthControlNext"));
	assert(tapeheadMidiMapAddBinding("NoteOn.1.97",
		"TrackLengthControlPrevious"));
	tapeheadMidiMapSetEnabled(true);

	assert(tapeheadMidiMapHandleMessage(0x90, 96, 127));
	tapeheadMidiMapProcessPending();
	assert(fastControlTrack == 7);
	assert(tapeheadMidiMapHandleMessage(0x90, 96, 127));
	tapeheadMidiMapProcessPending();
	assert(fastControlTrack == 0);
	assert(tapeheadMidiMapHandleMessage(0x90, 97, 127));
	tapeheadMidiMapProcessPending();
	assert(fastControlTrack == 7);

	fastControlTrack = -1;
	assert(tapeheadMidiMapHandleMessage(0x90, 97, 127));
	tapeheadMidiMapProcessPending();
	assert(fastControlTrack == 0);
}

int main(void)
{
	testParserRejectsAmbiguousOrInvalidMappings();
	testDisabledAndUnmappedMessagesPassThrough();
	testNoteActionsUsePressEdgeAndMainThreadDrain();
	testAPCPressReleaseSequencesRemainTwoState();
	testAbsoluteEncoderMovement();
	testCCFloodCoalescesToNewestValue();
	testQueueIsBoundedAndDropsWithoutLeaking();
	testDuplicateInputUsesLastBinding();
	testExtendedPerformanceCommandVocabulary();
	testPhase42VocabularyAndDispatch();
	testPhase431JogAndModeEventsAreNotCollapsed();
	testTransportPunchEdgesAndShiftedClutchSafety();
	testControlTrackNavigationVocabularyAndDispatch();
	puts("Tapehead generic MIDI map tests passed.");
	return 0;
}
