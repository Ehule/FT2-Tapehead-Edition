// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "scopes/ft2_scopes.h"
#include "ft2_video.h"
#include "ft2_gui.h"
#include "ft2_midi.h"
#include "ft2_wav_renderer.h"
#include "ft2_tables.h"
#include "ft2_structs.h"
#include "ft2_audioselector.h"
#include "ft2_jack.h"
#include "ft2_pattern_launcher.h"
#include "ft2_poly_matrix.h"
#include "ft2_sample_launcher.h"
#include "mixer/ft2_mix.h"
#include "mixer/ft2_silence_mix.h"

// hide POSIX warnings
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#define INITIAL_DITHER_SEED 0x12345000

static uint32_t oldAudioFreq, tickTimeLenInt, randSeed = INITIAL_DITHER_SEED;
static uint64_t tickTimeLenFrac;
static float fSqrtPanningTable[256+1], fAudioNormalizeMul;
static float fPrngState[TAPEHEAD_MAX_OUTPUT_BUSES * 2];
static voice_t voice[MAX_CHANNELS * 2];
#define SAMPLE_LAUNCHER_AUDIO_VOICES 5
static voice_t sampleLauncherVoice[SAMPLE_LAUNCHER_AUDIO_VOICES];
static uint8_t sampleLauncherOutputBus[SAMPLE_LAUNCHER_AUDIO_VOICES];
static float sampleLauncherBaseVolume[SAMPLE_LAUNCHER_AUDIO_VOICES];
static uint16_t sampleLauncherAppliedGain[SAMPLE_LAUNCHER_AUDIO_VOICES];
static volatile uint16_t matrixQGain = 256, matrixPolyGain = 256;
static bool jackDiagnosticsEnabled;
static int8_t jackDiagnosticToneBus = -1;
static uint32_t jackDiagnosticFrames;
static double jackDiagnosticTonePhase;
static float jackDiagnosticPeak[TAPEHEAD_MAX_OUTPUT_BUSES * 2];

// globalized
audio_t audio;
pattSyncData_t *pattSyncEntry;
chSyncData_t *chSyncEntry;
chSync_t chSync;
pattSync_t pattSync;
volatile bool pattQueueClearing, chQueueClearing;

void audioSetMatrixMixerGains(uint16_t qGain, uint16_t polyGain)
{
	if (qGain > 256) qGain = 256;
	if (polyGain > 256) polyGain = 256;
	matrixQGain = qGain;
	matrixPolyGain = polyGain;

	/* The audio thread owns the actual ramp. Mark every tracker voice dirty so
	** currently sounding Q/Poly channels acquire the new target without
	** touching normal Song volume. Sample-deck voices are refreshed in their
	** mixer loop from the same two integer targets. */
	for (int32_t i = 0; i < song.numChannels && i < MAX_CHANNELS; i++)
		channel[i].status |= CS_UPDATE_VOL | CS_USE_QUICK_VOLRAMP;
}

void stopVoice(int32_t i)
{
	voice_t *v;

	v = &voice[i];
	memset(v, 0, sizeof (voice_t));
	v->panning = 128;

	// clear "fade out" voice too

	v = &voice[MAX_CHANNELS + i];
	memset(v, 0, sizeof (voice_t));
	v->panning = 128;
}

bool audioVoiceUsesSample(int32_t i, const sample_t *sample)
{
	if (i < 0 || i >= MAX_CHANNELS || sample == NULL || sample->dataPtr == NULL)
		return false;

	const bool sample16Bit = !!(sample->flags & SAMPLE_16BIT);
	for (int32_t offset = 0; offset <= MAX_CHANNELS; offset += MAX_CHANNELS)
	{
		const voice_t *v = &voice[offset + i];
		if (!v->active)
			continue;
		if (sample16Bit)
		{
			if (v->base16 == (const int16_t *)sample->dataPtr)
				return true;
		}
		else if (v->base8 == sample->dataPtr)
		{
			return true;
		}
	}

	return false;
}

void audioSampleLauncherStop(uint8_t voiceIndex)
{
	if (voiceIndex >= SAMPLE_LAUNCHER_AUDIO_VOICES)
		return;

	memset(&sampleLauncherVoice[voiceIndex], 0, sizeof (voice_t));
	sampleLauncherVoice[voiceIndex].panning = 128;
	sampleLauncherBaseVolume[voiceIndex] = 0.0f;
	sampleLauncherAppliedGain[voiceIndex] = UINT16_MAX;
}

void audioSampleLauncherStopAll(void)
{
	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_AUDIO_VOICES; i++)
		audioSampleLauncherStop(i);
}

void audioSampleLauncherSetOutputBus(uint8_t voiceIndex, uint8_t outputBus)
{
	if (voiceIndex < SAMPLE_LAUNCHER_AUDIO_VOICES)
		sampleLauncherOutputBus[voiceIndex] = outputBus;
}

void audioSampleLauncherTrigger(uint8_t voiceIndex, const sample_t *sample,
	uint8_t outputBus)
{
	if (voiceIndex >= SAMPLE_LAUNCHER_AUDIO_VOICES || sample == NULL ||
		sample->dataPtr == NULL || sample->length < 1 || audio.freq == 0)
	{
		audioSampleLauncherStop(voiceIndex);
		return;
	}

	voice_t *v = &sampleLauncherVoice[voiceIndex];
	memset(v, 0, sizeof (*v));

	const bool sample16Bit = !!(sample->flags & SAMPLE_16BIT);
	if (sample16Bit)
		v->base16 = (const int16_t *)sample->dataPtr;
	else
		v->base8 = sample->dataPtr;

	/* Sample-deck loops deliberately use the complete native sample and the
	** interpolation-free mixer. The deck does not rewrite XM loop metadata;
	** its Q/Poly voices remain independent even though file management now
	** lives in the module's ordinary instrument pool. */
	v->loopType = LOOP_FORWARD;
	v->sampleEnd = sample->length;
	v->loopStart = 0;
	v->loopLength = sample->length;
	v->position = 0;
	v->positionFrac = 0;
	v->panning = sample->panning;
	sampleLauncherBaseVolume[voiceIndex] =
		sample->volume * (1.0f / 64.0f);
	const uint16_t gain = voiceIndex == 0 ? matrixQGain : matrixPolyGain;
	sampleLauncherAppliedGain[voiceIndex] = gain;
	v->fVolume = sampleLauncherBaseVolume[voiceIndex] *
		(gain * (1.0f / 256.0f));
	if (audio.monoOutputMode)
	{
		v->fCurrVolumeL = v->fTargetVolumeL = v->fVolume;
		v->fCurrVolumeR = v->fTargetVolumeR = 0.0f;
	}
	else
	{
		v->fCurrVolumeL = v->fTargetVolumeL =
			v->fVolume * fSqrtPanningTable[256-v->panning];
		v->fCurrVolumeR = v->fTargetVolumeR =
			v->fVolume * fSqrtPanningTable[v->panning];
	}
	v->fCurrVolumeMono = v->fTargetVolumeMono = v->fVolume;

	const uint32_t naturalRate = (uint32_t)getSampleC4Hz((sample_t *)sample);
	v->delta = ((uint64_t)naturalRate << MIXER_FRAC_BITS) / audio.freq;
	if (v->delta == 0)
		v->delta = 1;

	/* 15 routines per bit depth, interpolation type zero, forward loop. */
	v->mixFuncOffset = (sample16Bit ? 15 : 0) + LOOP_FORWARD;
	v->active = true;
	audioSampleLauncherSetOutputBus(voiceIndex, outputBus);
}

bool setNewAudioSettings(void) // only call this from the main input/video thread
{
	pauseAudio();

	if (!setupAudio(CONFIG_HIDE_ERRORS))
	{
		// set back old known working settings

		config.audioFreq = audio.lastWorkingAudioFreq;
		config.specialFlags &= ~(BITDEPTH_16 + BITDEPTH_32 + BUFFSIZE_512 + BUFFSIZE_1024 + BUFFSIZE_2048);
		config.specialFlags |= audio.lastWorkingAudioBits;

		if (audio.lastWorkingAudioDeviceName != NULL)
		{
			if (audio.currOutputDevice != NULL)
			{
				free(audio.currOutputDevice);
				audio.currOutputDevice = NULL;
			}

			audio.currOutputDevice = strdup(audio.lastWorkingAudioDeviceName);
		}

		// also update config audio radio buttons if we're on that screen at the moment
		if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
			setConfigAudioRadioButtonStates();

		// if it didn't work to use the old settings again, then something is seriously wrong...
		if (!setupAudio(CONFIG_HIDE_ERRORS))
			okBox(0, "System message", "Couldn't find a working audio mode... You'll get no sound / replayer timer!", NULL);

		resumeAudio();
		return false;
	}

	resumeAudio();

	setWavRenderFrequency(audio.freq);
	setWavRenderBitDepth((config.specialFlags & BITDEPTH_32) ? 32 : 16);
	return true;
}

// amp = 1..32, masterVol = 0..256
void setAudioAmp(int16_t amp, int16_t masterVol, bool bitDepth32Flag)
{
	amp = CLAMP(amp, 1, 32);
	masterVol = CLAMP(masterVol, 0, 256);

	float fAmp = (amp * masterVol) / (32.0f * 256.0f);
	if (!bitDepth32Flag)
		fAmp *= 32768.0f;

	fAudioNormalizeMul = fAmp;
}

void decreaseMasterVol(void)
{
	if (config.masterVol >= 16)
		config.masterVol -= 16;
	else
		config.masterVol = 0;

	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));

	// if Config -> Audio is open, update master volume scrollbar
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
		drawScrollBar(SB_MASTERVOL_SCROLL);
}

void increaseMasterVol(void)
{
	if (config.masterVol < (256-16))
		config.masterVol += 16;
	else
		config.masterVol = 256;

	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));

	// if Config -> Audio is open, update master volume scrollbar
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
		drawScrollBar(SB_MASTERVOL_SCROLL);
}

void setNewAudioFreq(uint32_t freq) // for song-to-WAV rendering
{
	if (freq == 0)
		return;

	oldAudioFreq = audio.freq;
	audio.freq = freq;

	calcReplayerVars(audio.freq, audio.freq);
}

void setBackOldAudioFreq(void) // for song-to-WAV rendering
{
	audio.freq = oldAudioFreq;
	calcReplayerVars(FT2_REF_AUDIO_RATE, audio.freq);
}

void setMixerBPM(int32_t bpm)
{
	if (bpm < MIN_BPM || bpm > MAX_BPM)
		return;

	int32_t i = bpm - MIN_BPM;

	audio.samplesPerTickInt = audio.samplesPerTickIntTab[i];
	audio.samplesPerTickFrac = audio.samplesPerTickFracTab[i];
	audio.fSamplesPerTickIntMul = (float)(1.0 / (double)audio.samplesPerTickInt);

	// for audio/video sync timestamp
	tickTimeLenInt = audio.tickTimeIntTab[i];
	tickTimeLenFrac = audio.tickTimeFracTab[i];
}

void audioSetVolRamp(bool volRamp)
{
	lockMixerCallback();
	audio.volumeRampingFlag = volRamp;
	unlockMixerCallback();
}

void audioSetInterpolationType(uint8_t interpolationType)
{
	lockMixerCallback();
	audio.interpolationType = interpolationType;

	audio.sincInterpolation = false;

	// set sinc LUT pointers
	if (config.interpolation == INTERPOLATION_SINC8)
	{
		for (int32_t i = 0; i < SINC_KERNELS; i++)
			fSinc[i] = fSinc8[i];

		audio.sincInterpolation = true;
	}
	else if (config.interpolation == INTERPOLATION_SINC16)
	{
		for (int32_t i = 0; i < SINC_KERNELS; i++)
			fSinc[i] = fSinc16[i];

		audio.sincInterpolation = true;
	}

	unlockMixerCallback();
}

void calcPanningTable(void)
{
	for (int32_t i = 0; i <= 256; i++)
	{
		// bit-accurate to how FT2 calculates it
		int32_t ft2SqrtPan = (int32_t)round(65536.0 * sqrt(i / 256.0));

		// scale to 0.0 .. 1.0f for our float mixer
		fSqrtPanningTable[i] = (float)ft2SqrtPan / 65536.0f;
	}
}

static void voiceUpdateVolumes(int32_t i, uint8_t status)
{
	voice_t *v = &voice[i];

	v->fTargetVolumeL = v->fVolume * fSqrtPanningTable[256-v->panning];
	v->fTargetVolumeR = v->fVolume * fSqrtPanningTable[    v->panning];
	v->fTargetVolumeMono = v->fVolume;

	if (!audio.volumeRampingFlag)
	{
		// volume ramping is disabled, set volume directly
		v->fCurrVolumeL = v->fTargetVolumeL;
		v->fCurrVolumeR = v->fTargetVolumeR;
		v->fCurrVolumeMono = v->fTargetVolumeMono;
		v->volumeRampLength = 0;
		return;
	}

	// now we need to handle volume ramping

	const bool voiceTriggerFlag = !!(status & CS_TRIGGER_VOICE);
	if (voiceTriggerFlag)
	{
		// voice is about to start, ramp out/in at the same time

		if (v->fCurrVolumeL > 0.0f || v->fCurrVolumeR > 0.0f ||
			v->fCurrVolumeMono > 0.0f)
		{
			// setup fadeout voice

			voice_t *f = &voice[MAX_CHANNELS+i];

			*f = *v; // copy current voice to new fadeout-ramp voice

			const float fVolumeLDiff = 0.0f - f->fCurrVolumeL;
			const float fVolumeRDiff = 0.0f - f->fCurrVolumeR;
			const float fVolumeMonoDiff = 0.0f - f->fCurrVolumeMono;

			f->volumeRampLength = audio.quickVolRampSamples; // 5ms
			f->fVolumeLDelta = fVolumeLDiff * audio.fQuickVolRampSamplesMul;
			f->fVolumeRDelta = fVolumeRDiff * audio.fQuickVolRampSamplesMul;
			f->fVolumeMonoDelta = fVolumeMonoDiff * audio.fQuickVolRampSamplesMul;

			f->isFadeOutVoice = true;
		}

		// make current voice fade in from zero when it starts
		v->fCurrVolumeL = v->fCurrVolumeR = v->fCurrVolumeMono = 0.0f;
	}

	if (!voiceTriggerFlag && v->fTargetVolumeL == v->fCurrVolumeL &&
		v->fTargetVolumeR == v->fCurrVolumeR &&
		v->fTargetVolumeMono == v->fCurrVolumeMono)
	{
		v->volumeRampLength = 0; // no ramp needed for now
	}
	else
	{
		const float fVolumeLDiff = v->fTargetVolumeL - v->fCurrVolumeL;
		const float fVolumeRDiff = v->fTargetVolumeR - v->fCurrVolumeR;
		const float fVolumeMonoDiff = v->fTargetVolumeMono - v->fCurrVolumeMono;

		float fRampLengthMul;
		if (status & CS_USE_QUICK_VOLRAMP) // 5ms duration
		{
			v->volumeRampLength = audio.quickVolRampSamples;
			fRampLengthMul = audio.fQuickVolRampSamplesMul;
		}
		else // duration of a tick
		{
			v->volumeRampLength = audio.samplesPerTickInt;
			fRampLengthMul = audio.fSamplesPerTickIntMul;
		}

		v->fVolumeLDelta = fVolumeLDiff * fRampLengthMul;
		v->fVolumeRDelta = fVolumeRDiff * fRampLengthMul;
		v->fVolumeMonoDelta = fVolumeMonoDiff * fRampLengthMul;
	}
}

static void voiceTrigger(int32_t ch, sample_t *s, int32_t position)
{
	voice_t *v = &voice[ch];

	int32_t length = s->length;
	int32_t loopStart = s->loopStart;
	int32_t loopLength = s->loopLength;
	int32_t loopEnd = s->loopStart + s->loopLength;
	uint8_t loopType = GET_LOOPTYPE(s->flags);
	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);

	if (s->dataPtr == NULL || length < 1)
	{
		v->active = false; // shut down voice (illegal parameters)
		return;
	}

	if (loopLength < 1) // disable loop if loopLength is below 1
		loopType = 0;

	if (sample16Bit)
	{
		v->base16 = (const int16_t *)s->dataPtr;
		v->revBase16 = &v->base16[loopStart + loopEnd]; // for pingpong loops
		v->leftEdgeTaps16 = s->leftEdgeTapSamples16 + MAX_LEFT_TAPS;
	}
	else
	{
		v->base8 = s->dataPtr;
		v->revBase8 = &v->base8[loopStart + loopEnd]; // for pingpong loops
		v->leftEdgeTaps8 = s->leftEdgeTapSamples8 + MAX_LEFT_TAPS;
	}

	v->hasLooped = false; // for cubic/sinc interpolation special case
	v->oneShot = false;
	v->reverseLoop = (s->flags & SAMPLE_REVERSE_LOOP) != 0;
	v->samplingBackwards = false;
	v->loopType = loopType;
	v->sampleEnd = (loopType == LOOP_DISABLED) ? length : loopEnd;
	v->loopStart = loopStart;
	v->loopLength = loopLength;
	v->position = position;
	v->positionFrac = 0;

	// if position overflows, shut down voice (f.ex. through 9xx command)
	if (v->position >= v->sampleEnd)
	{
		v->active = false;
		return;
	}

	v->mixFuncOffset = ((int32_t)sample16Bit * 15) + (audio.interpolationType * 3) + loopType;
	v->active = true;
}

static void voiceApplyTapeheadOneShot(voice_t *v, const sample_t *s,
	bool reverse)
{
	const bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
	v->oneShot = true;
	v->reverseLoop = false;
	v->hasLooped = false;
	v->loopStart = 0;
	v->loopLength = s->length;
	v->sampleEnd = s->length;
	v->position = 0;
	v->positionFrac = 0;
	v->samplingBackwards = reverse;
	v->loopType = reverse ? LOOP_PINGPONG : LOOP_DISABLED;
	if (sample16Bit)
		v->revBase16 = &v->base16[s->length];
	else
		v->revBase8 = &v->base8[s->length];
	v->mixFuncOffset = ((int32_t)sample16Bit * 15) +
		(audio.interpolationType * 3) + v->loopType;
}

void resetRampVolumes(void)
{
	voice_t *v = voice;
	for (int32_t i = 0; i < song.numChannels; i++, v++)
	{
		v->fCurrVolumeL = v->fTargetVolumeL;
		v->fCurrVolumeR = v->fTargetVolumeR;
		v->fCurrVolumeMono = v->fTargetVolumeMono;
		v->volumeRampLength = 0;
	}
}

void updateVoices(void)
{
	channel_t *ch = channel;
	voice_t *v = voice;

	for (int32_t i = 0; i < song.numChannels; i++, ch++, v++)
	{
		const uint8_t status = ch->tmpStatus = ch->status; // (tmpStatus is used for audio/video sync queue)
		if (status == 0)
			continue;

		ch->status = 0;

		// for "render individual tracks" mode during WAV render
		if (ch->dontRenderThisChannel)
			continue;

		if (status & CS_UPDATE_VOL)
		{
			/*
			** Performance mute affects only the audible mixer voice.
			** Scope volume deliberately remains based on the unmuted value,
			** allowing the waveform to keep moving underneath the red X.
			*/
			float matrixGain = 1.0f;
			if (polyMatrixOwnsDestination(i))
				matrixGain = matrixPolyGain * (1.0f / 256.0f);
			else if (patternLauncherOwnsDestination(i))
				matrixGain = matrixQGain * (1.0f / 256.0f);
			v->fVolume = performanceMute[i] ? 0.0f :
				ch->fFinalVol * matrixGain;
			v->scopeVolume = (uint8_t)((ch->fFinalVol * (SCOPE_HEIGHT*4.0f)) + 0.5f);
		}

		if (status & CS_UPDATE_PAN)
			v->panning = ch->finalPan;

		if (status & (CS_UPDATE_VOL + CS_UPDATE_PAN)) // for vol and/or pan updates
			voiceUpdateVolumes(i, status);

		if (status & CF_UPDATE_PERIOD)
		{
			const int64_t baseDelta = period2VoiceDelta(ch->finalPeriod);
			v->delta = microtonalScaleDelta(baseDelta,
				microtonalCurrentCents16(&ch->microtonal));

			if (audio.sincInterpolation)
			{
				if (v->delta <= sincRatio1)
					v->fSincLUT = fSinc[0];
				else if (v->delta <= sincRatio2)
					v->fSincLUT = fSinc[1];
				else
					v->fSincLUT = fSinc[2];
			}
		}

		if (status & CS_TRIGGER_VOICE)
		{
			voiceTrigger(i, ch->smpPtr, ch->smpStartPos);
			if (v->active && ch->tapeheadOneShotDirection != 0)
			{
				voiceApplyTapeheadOneShot(v, ch->smpPtr,
					ch->tapeheadOneShotDirection == 2);
			}
			ch->tapeheadOneShotDirection = 0;
		}
	}
}

void resetAudioDither(void)
{
	randSeed = INITIAL_DITHER_SEED;
	memset(fPrngState, 0, sizeof (fPrngState));
}

static inline int32_t random32(void)
{
	// LCG 32-bit random
	randSeed *= 134775813;
	randSeed++;

	return (int32_t)randSeed;
}

static void sendSamples16Bit(void *stream, uint32_t sampleBlockLength,
	uint8_t outputBusCount)
{
	int32_t out32;
	float fOut, fPrng;

	int16_t *streamPtr16 = (int16_t *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		for (uint8_t bus = 0; bus < outputBusCount; bus++)
		{
			const uint8_t leftChannel = bus * 2;
			const uint8_t rightChannel = leftChannel + 1;

			// left channel - 1-bit triangular dithering
			fPrng = (float)random32() * (1.0f / ((float)UINT32_MAX+1.0f)); // -0.5f .. 0.5f
			fOut = audio.fBusMixBufferL[bus][i] * fAudioNormalizeMul;
			fOut = (fOut + fPrng) - fPrngState[leftChannel];
			fPrngState[leftChannel] = fPrng;
			out32 = (int32_t)fOut;
			*streamPtr16++ = (int16_t)(CLAMP(out32, INT16_MIN, INT16_MAX));

			// right channel - 1-bit triangular dithering
			fPrng = (float)random32() * (1.0f / ((float)UINT32_MAX+1.0f)); // -0.5f .. 0.5f
			fOut = audio.fBusMixBufferR[bus][i] * fAudioNormalizeMul;
			fOut = (fOut + fPrng) - fPrngState[rightChannel];
			fPrngState[rightChannel] = fPrng;
			out32 = (int32_t)fOut;
			*streamPtr16++ = (int16_t)(CLAMP(out32, INT16_MIN, INT16_MAX));

			audio.fBusMixBufferL[bus][i] = 0.0f;
			audio.fBusMixBufferR[bus][i] = 0.0f;
		}
	}
}

static void sendSamples32BitFloat(void *stream, uint32_t sampleBlockLength,
	uint8_t outputBusCount)
{
	float fOut;

	float *fStreamPtr32 = (float *)stream;
	for (uint32_t i = 0; i < sampleBlockLength; i++)
	{
		for (uint8_t bus = 0; bus < outputBusCount; bus++)
		{
			// left channel
			fOut = audio.fBusMixBufferL[bus][i] * fAudioNormalizeMul;
			fOut = CLAMP(fOut, -1.0f, 1.0f);
			*fStreamPtr32++ = fOut;

			// right channel
			fOut = audio.fBusMixBufferR[bus][i] * fAudioNormalizeMul;
			fOut = CLAMP(fOut, -1.0f, 1.0f);
			*fStreamPtr32++ = fOut;

			audio.fBusMixBufferL[bus][i] = 0.0f;
			audio.fBusMixBufferR[bus][i] = 0.0f;
		}
	}
}

static void mixVoicePair(voice_t *v, voice_t *r, int32_t bufferPosition,
	int32_t samplesToMix, int32_t mixOffsetBias)
{
	if (v->active)
	{
		const bool volRampFlag = (v->volumeRampLength > 0);
		const bool silent = audio.monoOutputMode
			? v->fCurrVolumeMono == 0.0f
			: (v->fCurrVolumeL == 0.0f && v->fCurrVolumeR == 0.0f);
		if (!volRampFlag && silent)
			silenceMixRoutine(v, samplesToMix);
		else
			mixFuncTab[((int32_t)volRampFlag * mixOffsetBias) + v->mixFuncOffset](
				v, bufferPosition, samplesToMix);
	}

	if (r->active) // volume ramp fadeout-voice
		mixFuncTab[mixOffsetBias + r->mixFuncOffset](r, bufferPosition, samplesToMix);
}

static void mixSampleLauncherVoice(voice_t *v, int32_t bufferPosition,
	int32_t samplesToMix)
{
	if (!v->active)
		return;

	const bool silent = audio.monoOutputMode
		? v->fCurrVolumeMono == 0.0f
		: (v->fCurrVolumeL == 0.0f && v->fCurrVolumeR == 0.0f);
	if (silent)
		silenceMixRoutine(v, samplesToMix);
	else
		mixFuncTab[v->mixFuncOffset](v, bufferPosition, samplesToMix);
}

static void refreshSampleLauncherMatrixGain(uint8_t voiceIndex)
{
	voice_t *v = &sampleLauncherVoice[voiceIndex];
	const uint16_t gain = voiceIndex == 0 ? matrixQGain : matrixPolyGain;
	if (!v->active || sampleLauncherAppliedGain[voiceIndex] == gain)
		return;

	sampleLauncherAppliedGain[voiceIndex] = gain;
	v->fVolume = sampleLauncherBaseVolume[voiceIndex] *
		(gain * (1.0f / 256.0f));
	v->fTargetVolumeL = v->fVolume * fSqrtPanningTable[256-v->panning];
	v->fTargetVolumeR = v->fVolume * fSqrtPanningTable[v->panning];
	v->fTargetVolumeMono = v->fVolume;

	if (!audio.volumeRampingFlag || audio.quickVolRampSamples == 0)
	{
		v->fCurrVolumeL = v->fTargetVolumeL;
		v->fCurrVolumeR = v->fTargetVolumeR;
		v->fCurrVolumeMono = v->fTargetVolumeMono;
		v->volumeRampLength = 0;
		return;
	}

	v->volumeRampLength = audio.quickVolRampSamples;
	v->fVolumeLDelta = (v->fTargetVolumeL - v->fCurrVolumeL) *
		audio.fQuickVolRampSamplesMul;
	v->fVolumeRDelta = (v->fTargetVolumeR - v->fCurrVolumeR) *
		audio.fQuickVolRampSamplesMul;
	v->fVolumeMonoDelta = (v->fTargetVolumeMono - v->fCurrVolumeMono) *
		audio.fQuickVolRampSamplesMul;
}

static void doSampleLauncherMixing(int32_t bufferPosition,
	int32_t samplesToMix, uint8_t outputBusCount)
{
	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_AUDIO_VOICES; i++)
	{
		refreshSampleLauncherMatrixGain(i);
		voice_t *v = &sampleLauncherVoice[i];
		if (!v->active)
			continue;

		if (audio.monoOutputMode)
		{
			uint8_t physicalOutputCount = outputBusCount * 2;
			if (physicalOutputCount > TAPEHEAD_MAX_OUTPUT_BUSES)
				physicalOutputCount = TAPEHEAD_MAX_OUTPUT_BUSES;
			uint8_t destination = sampleLauncherOutputBus[i];
			if (destination >= physicalOutputCount)
				destination = 0;

			const uint8_t bus = destination >> 1;
			float *monoBuffer = (destination & 1)
				? audio.fBusMixBufferR[bus]
				: audio.fBusMixBufferL[bus];
			audio.fMixBufferL = monoBuffer;
			audio.fMixBufferR = monoBuffer;
		}
		else
		{
			uint8_t bus = sampleLauncherOutputBus[i];
			if (bus >= outputBusCount)
				bus = 0;
			audio.fMixBufferL = audio.fBusMixBufferL[bus];
			audio.fMixBufferR = audio.fBusMixBufferR[bus];
		}

		mixSampleLauncherVoice(v, bufferPosition, samplesToMix);
	}
}

static void doChannelMixing(int32_t bufferPosition, int32_t samplesToMix,
	uint8_t outputBusCount)
{
	voice_t *v = voice; // normal voices
	voice_t *r = &voice[MAX_CHANNELS]; // volume ramp fadeout-voices

	const int32_t mixOffsetBias = 3 * NUM_INTERPOLATORS * 2; // 3 = loop types (off/fwd/pingpong), 2 = bit depths (8-bit/16-bit)
	const uint16_t availableBusMask = (outputBusCount >= TAPEHEAD_MAX_OUTPUT_BUSES)
		? UINT16_MAX
		: (uint16_t)((1U << outputBusCount) - 1);

	for (int32_t i = 0; i < song.numChannels; i++, v++, r++)
	{
		if (audio.monoOutputMode)
		{
			/*
			** In Mono Out mode each bit identifies one physical output channel,
			** not a stereo pair. Route the mono gain path directly into that
			** channel. Both generated mixer pointers intentionally target the
			** same buffer; the right gain is forced to zero by the mixer macros.
			** This bypasses FT2 pan commands/envelopes without modifying XM data.
			*/
			uint8_t physicalOutputCount = outputBusCount * 2;
			if (physicalOutputCount > TAPEHEAD_MAX_OUTPUT_BUSES)
				physicalOutputCount = TAPEHEAD_MAX_OUTPUT_BUSES;

			const uint16_t availableOutputMask = (uint16_t)
				((1U << physicalOutputCount) - 1);
			uint16_t outputMask = channelMonoOutputMask[i] & availableOutputMask;
			if (outputMask == 0)
				outputMask = 1;

			uint8_t outputDestination = 0;
			while (!(outputMask & (1U << outputDestination)))
				outputDestination++;

			const uint8_t outputBus = outputDestination >> 1;
			float *monoBuffer = (outputDestination & 1)
				? audio.fBusMixBufferR[outputBus]
				: audio.fBusMixBufferL[outputBus];

			audio.fMixBufferL = monoBuffer;
			audio.fMixBufferR = monoBuffer;
			mixVoicePair(v, r, bufferPosition, samplesToMix, mixOffsetBias);
			continue;
		}

		/*
		** A stereo-only device (and offline WAV rendering) folds every logical
		** bus to A. Otherwise route this physical FT2 channel to its selected
		** logical bus or buses.
		*/
		uint16_t outputMask = 1;
		if (outputBusCount > 1)
		{
			outputMask = channelOutputBusMask[i] & availableBusMask;
			if (outputMask == 0)
				outputMask = 1;
		}

		const bool multipleOutputs = (outputMask & (outputMask - 1)) != 0;
		if (!multipleOutputs)
		{
			/*
			** The ordinary A/B case has exactly one destination. Point FT2's
			** generated mixer routines directly at that bus. This is both the
			** shortest signal path and, importantly, keeps live Bus B voices out
			** of the neutral scratch/distribution stage that the X220 hardware
			** diagnostic proved could discard them.
			*/
			uint8_t outputBus = 0;
			while (!(outputMask & (1U << outputBus)))
				outputBus++;

			audio.fMixBufferL = audio.fBusMixBufferL[outputBus];
			audio.fMixBufferR = audio.fBusMixBufferR[outputBus];
			mixVoicePair(v, r, bufferPosition, samplesToMix, mixOffsetBias);
		}
		else
		{
			/*
			** A "To Main" route has more than one destination. Render the voice
			** once into a neutral buffer, then duplicate those samples without
			** advancing note/effect/sample state a second time.
			*/
			float *channelBufferL = audio.fChannelMixBufferL + bufferPosition;
			float *channelBufferR = audio.fChannelMixBufferR + bufferPosition;
			memset(channelBufferL, 0, samplesToMix * sizeof (float));
			memset(channelBufferR, 0, samplesToMix * sizeof (float));

			audio.fMixBufferL = audio.fChannelMixBufferL;
			audio.fMixBufferR = audio.fChannelMixBufferR;
			mixVoicePair(v, r, bufferPosition, samplesToMix, mixOffsetBias);

			for (uint8_t bus = 0; bus < outputBusCount; bus++)
			{
				if (!(outputMask & (1U << bus)))
					continue;

				float *busBufferL = audio.fBusMixBufferL[bus] + bufferPosition;
				float *busBufferR = audio.fBusMixBufferR[bus] + bufferPosition;
				for (int32_t sample = 0; sample < samplesToMix; sample++)
				{
					busBufferL[sample] += channelBufferL[sample];
					busBufferR[sample] += channelBufferR[sample];
				}
			}
		}
	}

	/* The Sample deck owns a separate voice pool. It is mixed only after every
	** XM/Q/Poly channel has advanced, so neither deck can steal the other's
	** physical tracker voice or transport ownership. */
	doSampleLauncherMixing(bufferPosition, samplesToMix, outputBusCount);

	audio.fMixBufferL = audio.fBusMixBufferL[0];
	audio.fMixBufferR = audio.fBusMixBufferR[0];
}

#ifdef TAPEHEAD_AUDIO_ROUTING_TEST
bool tapeheadTestRenderOneShot(bool reverse, float *samples,
	uint8_t sampleCount)
{
	if (samples == NULL || sampleCount < 4)
		return false;

	int8_t sampleData[4] = { 16, 32, 48, 64 };
	float right[4] = { 0 };
	sample_t sample;
	memset(&sample, 0, sizeof (sample));
	sample.dataPtr = sampleData;
	sample.length = sample.loopLength = 4;
	sample.flags = LOOP_FORWARD;

	voice_t oldVoice = voice[0];
	float *oldMixL = audio.fMixBufferL;
	float *oldMixR = audio.fMixBufferR;
	const uint8_t oldInterpolation = audio.interpolationType;
	const bool oldMono = audio.monoOutputMode;

	memset(&voice[0], 0, sizeof (voice[0]));
	audio.interpolationType = INTERPOLATION_DISABLED;
	audio.monoOutputMode = false;
	audio.fMixBufferL = samples;
	audio.fMixBufferR = right;
	memset(samples, 0, sampleCount * sizeof (float));
	voiceTrigger(0, &sample, 0);
	voiceApplyTapeheadOneShot(&voice[0], &sample, reverse);
	voice[0].delta = 1ULL << MIXER_FRAC_BITS;
	voice[0].fCurrVolumeL = 1.0f;
	voice[0].fCurrVolumeR = 0.0f;
	mixFuncTab[voice[0].mixFuncOffset](&voice[0], 0, 4);
	const bool ended = !voice[0].active;

	voice[0] = oldVoice;
	audio.fMixBufferL = oldMixL;
	audio.fMixBufferR = oldMixR;
	audio.interpolationType = oldInterpolation;
	audio.monoOutputMode = oldMono;
	return ended;
}

bool tapeheadTestRouteSyntheticVoice(uint16_t outputMask,
	uint8_t renderBusCount, uint8_t staleGlobalBusCount, float *peakBusA,
	float *peakBusB)
{
	if (outputMask == 0 || (outputMask & ~3U) != 0 || renderBusCount != 2 ||
		staleGlobalBusCount < 1 || peakBusA == NULL || peakBusB == NULL)
		return false;

	int8_t sampleData[8] = { 64, 64, 64, 64, 64, 64, 64, 64 };
	float busAL[4] = { 0 }, busAR[4] = { 0 };
	float busBL[4] = { 0 }, busBR[4] = { 0 };
	float channelL[4] = { 0 }, channelR[4] = { 0 };

	const int32_t oldNumChannels = song.numChannels;
	const uint16_t oldOutputMask = channelOutputBusMask[0];
	const uint8_t oldGlobalBusCount = audio.outputBusCount;
	voice_t oldVoice = voice[0];
	voice_t oldFadeVoice = voice[MAX_CHANNELS];
	float *oldBusAL = audio.fBusMixBufferL[0];
	float *oldBusAR = audio.fBusMixBufferR[0];
	float *oldBusBL = audio.fBusMixBufferL[1];
	float *oldBusBR = audio.fBusMixBufferR[1];
	float *oldChannelL = audio.fChannelMixBufferL;
	float *oldChannelR = audio.fChannelMixBufferR;

	song.numChannels = 1;
	channelOutputBusMask[0] = outputMask;
	/* Simulate the stale global count that folded the live JACK route to A. */
	audio.outputBusCount = staleGlobalBusCount;
	memset(&voice[0], 0, sizeof (voice[0]));
	memset(&voice[MAX_CHANNELS], 0, sizeof (voice[MAX_CHANNELS]));
	voice[0].active = true;
	voice[0].base8 = sampleData;
	voice[0].sampleEnd = 8;
	voice[0].delta = 1ULL << MIXER_FRAC_BITS;
	voice[0].fCurrVolumeL = 1.0f;
	voice[0].fCurrVolumeR = 1.0f;
	voice[0].mixFuncOffset = 0;
	audio.fBusMixBufferL[0] = busAL;
	audio.fBusMixBufferR[0] = busAR;
	audio.fBusMixBufferL[1] = busBL;
	audio.fBusMixBufferR[1] = busBR;
	audio.fChannelMixBufferL = channelL;
	audio.fChannelMixBufferR = channelR;

	doChannelMixing(0, 4, renderBusCount);

	*peakBusA = 0.0f;
	*peakBusB = 0.0f;
	for (int32_t i = 0; i < 4; i++)
	{
		*peakBusA = MAX(*peakBusA, MAX(fabsf(busAL[i]), fabsf(busAR[i])));
		*peakBusB = MAX(*peakBusB, MAX(fabsf(busBL[i]), fabsf(busBR[i])));
	}

	audio.fBusMixBufferL[0] = oldBusAL;
	audio.fBusMixBufferR[0] = oldBusAR;
	audio.fBusMixBufferL[1] = oldBusBL;
	audio.fBusMixBufferR[1] = oldBusBR;
	audio.fChannelMixBufferL = oldChannelL;
	audio.fChannelMixBufferR = oldChannelR;
	voice[0] = oldVoice;
	voice[MAX_CHANNELS] = oldFadeVoice;
	channelOutputBusMask[0] = oldOutputMask;
	audio.outputBusCount = oldGlobalBusCount;
	song.numChannels = oldNumChannels;

	return true;
}

bool tapeheadTestRouteSyntheticMonoVoice(uint8_t outputDestination,
	uint8_t panning, float *peaks, uint8_t peakCount)
{
	if (outputDestination >= 4 || peaks == NULL || peakCount < 4)
		return false;

	int8_t sampleData[8] = { 64, 64, 64, 64, 64, 64, 64, 64 };
	float busAL[4] = { 0 }, busAR[4] = { 0 };
	float busBL[4] = { 0 }, busBR[4] = { 0 };

	const int32_t oldNumChannels = song.numChannels;
	const uint16_t oldMonoOutputMask = channelMonoOutputMask[0];
	const bool oldMonoOutputMode = audio.monoOutputMode;
	voice_t oldVoice = voice[0];
	voice_t oldFadeVoice = voice[MAX_CHANNELS];
	float *oldBusAL = audio.fBusMixBufferL[0];
	float *oldBusAR = audio.fBusMixBufferR[0];
	float *oldBusBL = audio.fBusMixBufferL[1];
	float *oldBusBR = audio.fBusMixBufferR[1];

	song.numChannels = 1;
	channelMonoOutputMask[0] = (uint16_t)(1U << outputDestination);
	audio.monoOutputMode = true;
	memset(&voice[0], 0, sizeof (voice[0]));
	memset(&voice[MAX_CHANNELS], 0, sizeof (voice[MAX_CHANNELS]));
	voice[0].active = true;
	voice[0].base8 = sampleData;
	voice[0].sampleEnd = 8;
	voice[0].delta = 1ULL << MIXER_FRAC_BITS;
	voice[0].panning = panning;
	voice[0].fCurrVolumeL = panning == 255 ? 0.0625f : 1.0f;
	voice[0].fCurrVolumeR = panning == 0 ? 0.0f : 1.0f;
	voice[0].fCurrVolumeMono = 1.0f;
	voice[0].mixFuncOffset = 0;
	audio.fBusMixBufferL[0] = busAL;
	audio.fBusMixBufferR[0] = busAR;
	audio.fBusMixBufferL[1] = busBL;
	audio.fBusMixBufferR[1] = busBR;

	doChannelMixing(0, 4, 2);

	memset(peaks, 0, peakCount * sizeof (float));
	for (int32_t i = 0; i < 4; i++)
	{
		peaks[0] = MAX(peaks[0], fabsf(busAL[i]));
		peaks[1] = MAX(peaks[1], fabsf(busAR[i]));
		peaks[2] = MAX(peaks[2], fabsf(busBL[i]));
		peaks[3] = MAX(peaks[3], fabsf(busBR[i]));
	}

	audio.fBusMixBufferL[0] = oldBusAL;
	audio.fBusMixBufferR[0] = oldBusAR;
	audio.fBusMixBufferL[1] = oldBusBL;
	audio.fBusMixBufferR[1] = oldBusBR;
	voice[0] = oldVoice;
	voice[MAX_CHANNELS] = oldFadeVoice;
	channelMonoOutputMask[0] = oldMonoOutputMask;
	audio.monoOutputMode = oldMonoOutputMode;
	song.numChannels = oldNumChannels;

	return true;
}

bool tapeheadTestRouteSyntheticSampleLauncherVoice(uint8_t outputBus,
	float *peakBusA, float *peakBusB)
{
	if (outputBus >= 2 || peakBusA == NULL || peakBusB == NULL)
		return false;

	int8_t sampleData[8] = { 64, 64, 64, 64, 64, 64, 64, 64 };
	float busAL[4] = { 0 }, busAR[4] = { 0 };
	float busBL[4] = { 0 }, busBR[4] = { 0 };

	const int32_t oldNumChannels = song.numChannels;
	const bool oldMonoOutputMode = audio.monoOutputMode;
	voice_t oldLauncherVoice = sampleLauncherVoice[0];
	const uint8_t oldLauncherBus = sampleLauncherOutputBus[0];
	const float oldLauncherBaseVolume = sampleLauncherBaseVolume[0];
	const uint16_t oldLauncherAppliedGain = sampleLauncherAppliedGain[0];
	float *oldBusAL = audio.fBusMixBufferL[0];
	float *oldBusAR = audio.fBusMixBufferR[0];
	float *oldBusBL = audio.fBusMixBufferL[1];
	float *oldBusBR = audio.fBusMixBufferR[1];

	song.numChannels = 0;
	audio.monoOutputMode = false;
	memset(&sampleLauncherVoice[0], 0, sizeof (sampleLauncherVoice[0]));
	sampleLauncherVoice[0].active = true;
	sampleLauncherVoice[0].base8 = sampleData;
	sampleLauncherVoice[0].sampleEnd = 8;
	sampleLauncherVoice[0].delta = 1ULL << MIXER_FRAC_BITS;
	sampleLauncherVoice[0].fCurrVolumeL = 1.0f;
	sampleLauncherVoice[0].fCurrVolumeR = 1.0f;
	sampleLauncherVoice[0].mixFuncOffset = 0;
	sampleLauncherBaseVolume[0] = 1.0f;
	sampleLauncherAppliedGain[0] = matrixQGain;
	sampleLauncherOutputBus[0] = outputBus;
	audio.fBusMixBufferL[0] = busAL;
	audio.fBusMixBufferR[0] = busAR;
	audio.fBusMixBufferL[1] = busBL;
	audio.fBusMixBufferR[1] = busBR;

	doChannelMixing(0, 4, 2);

	*peakBusA = 0.0f;
	*peakBusB = 0.0f;
	for (int32_t i = 0; i < 4; i++)
	{
		*peakBusA = MAX(*peakBusA, MAX(fabsf(busAL[i]), fabsf(busAR[i])));
		*peakBusB = MAX(*peakBusB, MAX(fabsf(busBL[i]), fabsf(busBR[i])));
	}

	audio.fBusMixBufferL[0] = oldBusAL;
	audio.fBusMixBufferR[0] = oldBusAR;
	audio.fBusMixBufferL[1] = oldBusBL;
	audio.fBusMixBufferR[1] = oldBusBR;
	sampleLauncherVoice[0] = oldLauncherVoice;
	sampleLauncherOutputBus[0] = oldLauncherBus;
	sampleLauncherBaseVolume[0] = oldLauncherBaseVolume;
	sampleLauncherAppliedGain[0] = oldLauncherAppliedGain;
	audio.monoOutputMode = oldMonoOutputMode;
	song.numChannels = oldNumChannels;
	return true;
}
#endif

// used for song-to-WAV renderer
void mixReplayerTickToBuffer(uint32_t samplesToMix, void *stream, uint8_t bitDepth)
{
	doChannelMixing(0, samplesToMix, 1);

	// normalize mix buffer and send to audio stream
	if (bitDepth == 16)
		sendSamples16Bit(stream, samplesToMix, 1);
	else
		sendSamples32BitFloat(stream, samplesToMix, 1);
}

int32_t pattQueueReadSize(void)
{
	while (pattQueueClearing);

	if (pattSync.writePos > pattSync.readPos)
		return pattSync.writePos - pattSync.readPos;
	else if (pattSync.writePos < pattSync.readPos)
		return pattSync.writePos - pattSync.readPos + SYNC_QUEUE_LEN + 1;
	else
		return 0;
}

int32_t pattQueueWriteSize(void)
{
	int32_t size;

	if (pattSync.writePos > pattSync.readPos)
	{
		size = pattSync.readPos - pattSync.writePos + SYNC_QUEUE_LEN;
	}
	else if (pattSync.writePos < pattSync.readPos)
	{
		pattQueueClearing = true;

		/* Buffer is full, reset the read/write pos. This is actually really nasty since
		** read/write are two different threads, but because of timestamp validation it
		** shouldn't be that dangerous.
		** It will also create a small visual stutter while the buffer is getting filled,
		** though that is barely noticable on normal buffer sizes, and it takes a minute
		** or two at max BPM between each time (when queue size is default, 4095)
		*/
		pattSync.data[0].timestamp = 0;
		pattSync.readPos = 0;
		pattSync.writePos = 0;

		size = SYNC_QUEUE_LEN;

		pattQueueClearing = false;
	}
	else
	{
		size = SYNC_QUEUE_LEN;
	}

	return size;
}

bool pattQueuePush(pattSyncData_t t)
{
	if (!pattQueueWriteSize())
		return false;

	ASSERT(pattSync.writePos <= SYNC_QUEUE_LEN);
	pattSync.data[pattSync.writePos] = t;
	pattSync.writePos = (pattSync.writePos + 1) & SYNC_QUEUE_LEN;

	return true;
}

bool pattQueuePop(void)
{
	if (!pattQueueReadSize())
		return false;

	pattSync.readPos = (pattSync.readPos + 1) & SYNC_QUEUE_LEN;
	ASSERT(pattSync.readPos <= SYNC_QUEUE_LEN);

	return true;
}

pattSyncData_t *pattQueuePeek(void)
{
	if (!pattQueueReadSize())
		return NULL;

	ASSERT(pattSync.readPos <= SYNC_QUEUE_LEN);
	return &pattSync.data[pattSync.readPos];
}

uint64_t getPattQueueTimestamp(void)
{
	if (!pattQueueReadSize())
		return 0;

	ASSERT(pattSync.readPos <= SYNC_QUEUE_LEN);
	return pattSync.data[pattSync.readPos].timestamp;
}

int32_t chQueueReadSize(void)
{
	while (chQueueClearing);

	if (chSync.writePos > chSync.readPos)
		return chSync.writePos - chSync.readPos;
	else if (chSync.writePos < chSync.readPos)
		return chSync.writePos - chSync.readPos + SYNC_QUEUE_LEN + 1;
	else
		return 0;
}

int32_t chQueueWriteSize(void)
{
	int32_t size;

	if (chSync.writePos > chSync.readPos)
	{
		size = chSync.readPos - chSync.writePos + SYNC_QUEUE_LEN;
	}
	else if (chSync.writePos < chSync.readPos)
	{
		chQueueClearing = true;

		/* Buffer is full, reset the read/write pos. This is actually really nasty since
		** read/write are two different threads, but because of timestamp validation it
		** shouldn't be that dangerous.
		** It will also create a small visual stutter while the buffer is getting filled,
		** though that is barely noticable on normal buffer sizes, and it takes several
		** minutes between each time (when queue size is default, 16384)
		*/
		chSync.data[0].timestamp = 0;
		chSync.readPos = 0;
		chSync.writePos = 0;

		size = SYNC_QUEUE_LEN;

		chQueueClearing = false;
	}
	else
	{
		size = SYNC_QUEUE_LEN;
	}

	return size;
}

bool chQueuePush(chSyncData_t t)
{
	if (!chQueueWriteSize())
		return false;

	ASSERT(chSync.writePos <= SYNC_QUEUE_LEN);
	chSync.data[chSync.writePos] = t;
	chSync.writePos = (chSync.writePos + 1) & SYNC_QUEUE_LEN;

	return true;
}

bool chQueuePop(void)
{
	if (!chQueueReadSize())
		return false;

	chSync.readPos = (chSync.readPos + 1) & SYNC_QUEUE_LEN;
	ASSERT(chSync.readPos <= SYNC_QUEUE_LEN);

	return true;
}

chSyncData_t *chQueuePeek(void)
{
	if (!chQueueReadSize())
		return NULL;

	ASSERT(chSync.readPos <= SYNC_QUEUE_LEN);
	return &chSync.data[chSync.readPos];
}

uint64_t getChQueueTimestamp(void)
{
	if (!chQueueReadSize())
		return 0;

	ASSERT(chSync.readPos <= SYNC_QUEUE_LEN);
	return chSync.data[chSync.readPos].timestamp;
}

void lockAudio(void)
{
	if (tapeheadJackIsOpen())
		tapeheadJackLock();
	else if (audio.dev != 0)
		SDL_LockAudioDevice(audio.dev);

	audio.locked = true;
}

void unlockAudio(void)
{
	if (tapeheadJackIsOpen())
		tapeheadJackUnlock();
	else if (audio.dev != 0)
		SDL_UnlockAudioDevice(audio.dev);

	audio.locked = false;
}

void resetSyncQueues(void)
{
	pattSync.data[0].timestamp = 0;
	pattSync.readPos = 0;
	pattSync.writePos = 0;
	
	chSync.data[0].timestamp = 0;
	chSync.writePos = 0;
	chSync.readPos = 0;
}

void lockMixerCallback(void) // lock audio + clear voices/scopes (for short operations)
{
	if (!audio.locked)
		lockAudio();

	audio.resetSyncTickTimeFlag = true;

	stopVoices(); // VERY important! prevents potential crashes by purging pointers

	// scopes, mixer and replayer are guaranteed to not be active at this point

	resetSyncQueues();
}

void unlockMixerCallback(void)
{
	stopVoices(); // VERY important! prevents potential crashes by purging pointers
	
	if (audio.locked)
		unlockAudio();
}

void pauseAudio(void) // lock audio + clear voices/scopes + render silence (for long operations)
{
	/* Native Sample Banks share their sample memory with the editor. Stop the
	** deck before an editor operation can resize or replace that memory. */
	if (editor.curInstr > 0 && editor.curInstr <= MAX_INST &&
		sampleLauncherInstrumentIsMapped(editor.curInstr))
	{
		sampleLauncherReset();
	}
	if (audioPaused)
	{
		stopVoices(); // VERY important! prevents potential crashes by purging pointers
		return;
	}

	if (tapeheadJackIsOpen())
		tapeheadJackPause(true);
	else if (audio.dev > 0)
		SDL_PauseAudioDevice(audio.dev, true);

	audio.resetSyncTickTimeFlag = true;

	stopVoices(); // VERY important! prevents potential crashes by purging pointers

	// scopes, mixer and replayer are guaranteed to not be active at this point

	resetSyncQueues();
	audioPaused = true;
}

void resumeAudio(void) // unlock audio
{
	if (!audioPaused)
		return;

	if (tapeheadJackIsOpen())
		tapeheadJackPause(false);
	else if (audio.dev > 0)
		SDL_PauseAudioDevice(audio.dev, false);

	audioPaused = false;
}

static void fillVisualsSyncBuffer(void)
{
	pattSyncData_t pattSyncData;
	chSyncData_t chSyncData;

	if (audio.resetSyncTickTimeFlag)
	{
		audio.resetSyncTickTimeFlag = false;

		audio.tickTime64 = SDL_GetPerformanceCounter() + audio.audLatencyPerfValInt;
		audio.tickTime64Frac = audio.audLatencyPerfValFrac;
	}

	if (songPlaying)
	{
		// push pattern variables to sync queue
		pattSyncData.tick = song.curReplayerTick;
		pattSyncData.row = song.curReplayerRow;
		pattSyncData.pattNum = song.curReplayerPattNum;
		pattSyncData.songPos = song.curReplayerSongPos;
		pattSyncData.BPM = (uint8_t)song.BPM;
		pattSyncData.speed = (uint8_t)song.speed;
		pattSyncData.globalVolume = (uint8_t)song.globalVolume;
		pattSyncData.timestamp = audio.tickTime64;
		pattQueuePush(pattSyncData);
	}

	// push channel variables to sync queue

	syncedChannel_t *c = chSyncData.channels;
	channel_t *s = channel;
	voice_t *v = voice;

	for (int32_t i = 0; i < song.numChannels; i++, c++, s++, v++)
	{
		c->scopeVolume = v->scopeVolume;
		c->period = s->finalPeriod;
		c->instrNum = s->instrNum;
		c->smpNum = s->smpNum;
		c->status = s->tmpStatus;
		c->smpStartPos = s->smpStartPos;

		c->pianoNoteNum = 255; // no piano key
		if (songPlaying && ui.instEditorShown && (c->status & CF_UPDATE_PERIOD) && !s->keyOff)
		{
			const int32_t note = getPianoKey(s->finalPeriod, s->finetune, s->relativeNote);
			if (note >= 0 && note <= 95)
				c->pianoNoteNum = (uint8_t)note;
		}
	}

	chSyncData.timestamp = audio.tickTime64;
	chQueuePush(chSyncData);

	audio.tickTime64 += tickTimeLenInt;

	audio.tickTime64Frac += tickTimeLenFrac;
	if (audio.tickTime64Frac >= TICK_TIME_FRAC_SCALE)
	{
		audio.tickTime64Frac &= TICK_TIME_FRAC_MASK;
		audio.tickTime64++;
	}
}

static void renderAudioFrames(uint32_t sampleFrames, uint8_t outputBusCount)
{
	if (sampleFrames == 0)
		return;

	if (outputBusCount < 1 || outputBusCount > TAPEHEAD_MAX_OUTPUT_BUSES)
		outputBusCount = 1;

	audio.callbackOngoing = true;

	int32_t bufferPosition = 0;

	uint32_t samplesLeft = sampleFrames;
	while (samplesLeft > 0)
	{
		if (audio.tickSampleCounter <= 0) // new replayer tick
		{
			replayerBusy = true;
			if (!musicPaused) // important, don't remove this check! (also used for safety)
			{
				if (audio.volumeRampingFlag)
					resetRampVolumes();

				tickReplayer();
				updateVoices();

				if (audio.samplesPerTickInt != 0)
					fillVisualsSyncBuffer();
			}
			replayerBusy = false;

			audio.tickSampleCounter = audio.samplesPerTickInt;

			audio.tickSampleCounterFrac += audio.samplesPerTickFrac;
			if (audio.tickSampleCounterFrac >= BPM_FRAC_SCALE)
			{
				audio.tickSampleCounterFrac &= BPM_FRAC_MASK;
				audio.tickSampleCounter++;
			}
		}

		int32_t samplesToMix = samplesLeft;
		if (audio.tickSampleCounter > 0 && samplesToMix > audio.tickSampleCounter)
			samplesToMix = audio.tickSampleCounter;

		doChannelMixing(bufferPosition, samplesToMix, outputBusCount);
		bufferPosition += samplesToMix;

		audio.tickSampleCounter -= samplesToMix;
		samplesLeft -= samplesToMix;
	}
}

static void audioCallback(void *userdata, Uint8 *stream, int len)
{
	if (editor.wavIsRendering)
	{
		memset(stream, 0, len);
		return;
	}

	len /= audio.bytesPerFrame; // bytes -> sample frames
	if (len <= 0)
		return;

	renderAudioFrames((uint32_t)len, audio.outputBusCount);

	if (config.specialFlags & BITDEPTH_16)
		sendSamples16Bit(stream, len, audio.outputBusCount);
	else
		sendSamples32BitFloat(stream, len, audio.outputBusCount);

	audio.callbackOngoing = false;

	(void)userdata;
}

static void jackAudioCallback(float **outputs, uint32_t sampleFrames,
	uint8_t outputBusCount, void *userdata)
{
	(void)userdata;

	if (editor.wavIsRendering)
	{
		for (uint8_t outputChannel = 0;
			outputChannel < outputBusCount * 2; outputChannel++)
		{
			if (outputs[outputChannel] != NULL)
				memset(outputs[outputChannel], 0, sampleFrames * sizeof (float));
		}
		return;
	}

	/* JACK's configured port count is authoritative for this render cycle. */
	renderAudioFrames(sampleFrames, outputBusCount);

	for (uint8_t bus = 0; bus < outputBusCount; bus++)
	{
		float *outputL = outputs[bus * 2];
		float *outputR = outputs[(bus * 2) + 1];

		for (uint32_t i = 0; i < sampleFrames; i++)
		{
			const float mixerSampleL =
				audio.fBusMixBufferL[bus][i] * fAudioNormalizeMul;
			const float mixerSampleR =
				audio.fBusMixBufferR[bus][i] * fAudioNormalizeMul;

			float testTone = 0.0f;
			if (jackDiagnosticToneBus == (int8_t)bus)
			{
				testTone = sinf((float)jackDiagnosticTonePhase) * 0.125f;
				const uint32_t sampleRate = audio.haveFreq != 0
					? audio.haveFreq : 48000;
				jackDiagnosticTonePhase +=
					(6.28318530717958647692 * 440.0) / sampleRate;
				if (jackDiagnosticTonePhase >= 6.28318530717958647692)
					jackDiagnosticTonePhase -= 6.28318530717958647692;
			}

			const float sampleL = CLAMP(mixerSampleL + testTone, -1.0f, 1.0f);
			const float sampleR = CLAMP(mixerSampleR + testTone, -1.0f, 1.0f);

			if (outputL != NULL)
				outputL[i] = sampleL;
			if (outputR != NULL)
				outputR[i] = sampleR;

			/*
			** Measure the actual JACK port-buffer samples after the diagnostic
			** tone has been added and written. The original Pass 4 diagnostic
			** measured mixerSampleL/R above, which accidentally excluded the
			** injected tone and could therefore report B=0 while claiming to
			** test Bus B.
			*/
			if (jackDiagnosticsEnabled)
			{
				const float writtenSampleL = outputL != NULL ? outputL[i] : 0.0f;
				const float writtenSampleR = outputR != NULL ? outputR[i] : 0.0f;
				if (audio.monoOutputMode)
				{
					const uint8_t outputLIndex = bus * 2;
					const uint8_t outputRIndex = outputLIndex + 1;
					jackDiagnosticPeak[outputLIndex] = MAX(
						jackDiagnosticPeak[outputLIndex], fabsf(writtenSampleL));
					jackDiagnosticPeak[outputRIndex] = MAX(
						jackDiagnosticPeak[outputRIndex], fabsf(writtenSampleR));
				}
				else
				{
					const float peak = MAX(fabsf(writtenSampleL), fabsf(writtenSampleR));
					jackDiagnosticPeak[bus] = MAX(jackDiagnosticPeak[bus], peak);
				}
			}

			audio.fBusMixBufferL[bus][i] = 0.0f;
			audio.fBusMixBufferR[bus][i] = 0.0f;
		}
	}

	if (jackDiagnosticsEnabled)
	{
		jackDiagnosticFrames += sampleFrames;
		const uint32_t reportInterval = audio.haveFreq != 0
			? audio.haveFreq : 48000;
		if (jackDiagnosticFrames >= reportInterval)
		{
			fprintf(stderr, "Tapehead JACK live peaks:");
			const uint8_t diagnosticOutputCount = audio.monoOutputMode
				? (uint8_t)MIN(outputBusCount * 2, TAPEHEAD_MAX_OUTPUT_BUSES)
				: outputBusCount;
			for (uint8_t output = 0; output < diagnosticOutputCount; output++)
				fprintf(stderr, " %c=%.6f", 'A' + output, jackDiagnosticPeak[output]);

			fprintf(stderr, " | routes:");
			for (int32_t i = 0; i < song.numChannels; i++)
				fprintf(stderr, " %d=0x%04X", i + 1,
					getChannelOutputMask(i, audio.monoOutputMode));
			fputc('\n', stderr);
			fflush(stderr);

			jackDiagnosticFrames = 0;
			memset(jackDiagnosticPeak, 0, sizeof (jackDiagnosticPeak));
		}
	}

	audio.callbackOngoing = false;
}

static void configureJackDiagnostics(uint8_t outputBusCount)
{
	jackDiagnosticsEnabled = false;
	jackDiagnosticToneBus = -1;
	jackDiagnosticFrames = 0;
	jackDiagnosticTonePhase = 0.0;
	memset(jackDiagnosticPeak, 0, sizeof (jackDiagnosticPeak));

	const char *debugValue = getenv("TAPEHEAD_JACK_DEBUG");
	if (debugValue != NULL && debugValue[0] != '\0' && debugValue[0] != '0')
		jackDiagnosticsEnabled = true;

	const char *toneValue = getenv("TAPEHEAD_JACK_TEST_BUS");
	if (toneValue != NULL && toneValue[0] != '\0')
	{
		int32_t bus = -1;
		if (toneValue[0] >= 'A' && toneValue[0] <= 'P')
			bus = toneValue[0] - 'A';
		else if (toneValue[0] >= 'a' && toneValue[0] <= 'p')
			bus = toneValue[0] - 'a';
		else
			bus = atoi(toneValue) - 1;

		if (bus >= 0 && bus < outputBusCount)
		{
			jackDiagnosticToneBus = (int8_t)bus;
			jackDiagnosticsEnabled = true;
		}
	}

	if (jackDiagnosticsEnabled)
	{
		fprintf(stderr, "Tapehead JACK diagnostics enabled");
		if (jackDiagnosticToneBus >= 0)
		{
			fprintf(stderr, "; 440Hz test tone injected directly into Bus %c",
				'A' + jackDiagnosticToneBus);
		}
		fputc('\n', stderr);
		fflush(stderr);
	}
}

static bool setupAudioBuffers(uint32_t minimumSampleFrames)
{
	const int32_t maxAudioFreq = MAX(MAX_AUDIO_FREQ, MAX_WAV_RENDER_FREQ);
	int32_t maxSamplesPerTick = (int32_t)ceil(maxAudioFreq / (MIN_BPM / 2.5)) + 1;
	if (minimumSampleFrames > (uint32_t)maxSamplesPerTick)
		maxSamplesPerTick = (int32_t)minimumSampleFrames;

	for (uint8_t bus = 0; bus < TAPEHEAD_MAX_OUTPUT_BUSES; bus++)
	{
		audio.fBusMixBufferL[bus] = (float *)calloc(maxSamplesPerTick, sizeof (float));
		audio.fBusMixBufferR[bus] = (float *)calloc(maxSamplesPerTick, sizeof (float));
		if (audio.fBusMixBufferL[bus] == NULL || audio.fBusMixBufferR[bus] == NULL)
			return false;
	}

	audio.fChannelMixBufferL = (float *)calloc(maxSamplesPerTick, sizeof (float));
	audio.fChannelMixBufferR = (float *)calloc(maxSamplesPerTick, sizeof (float));
	if (audio.fChannelMixBufferL == NULL || audio.fChannelMixBufferR == NULL)
		return false;

	audio.fMixBufferL = audio.fBusMixBufferL[0];
	audio.fMixBufferR = audio.fBusMixBufferR[0];

	return true;
}

static void freeAudioBuffers(void)
{
	audio.fMixBufferL = NULL;
	audio.fMixBufferR = NULL;

	for (uint8_t bus = 0; bus < TAPEHEAD_MAX_OUTPUT_BUSES; bus++)
	{
		if (audio.fBusMixBufferL[bus] != NULL)
		{
			free(audio.fBusMixBufferL[bus]);
			audio.fBusMixBufferL[bus] = NULL;
		}

		if (audio.fBusMixBufferR[bus] != NULL)
		{
			free(audio.fBusMixBufferR[bus]);
			audio.fBusMixBufferR[bus] = NULL;
		}
	}

	if (audio.fChannelMixBufferL != NULL)
	{
		free(audio.fChannelMixBufferL);
		audio.fChannelMixBufferL = NULL;
	}

	if (audio.fChannelMixBufferR != NULL)
	{
		free(audio.fChannelMixBufferR);
		audio.fChannelMixBufferR = NULL;
	}
}

static void calcAudioLatencyVars(int32_t audioBufferSize, int32_t audioFreq)
{
	if (audioFreq == 0)
		return;

	const double dAudioLatencyTime = (audioBufferSize / (double)audioFreq) * (double)hpcFreq.freq64;
	double dAudioLatencyTimeInt, dAudioLatencyTimeFrac = modf(dAudioLatencyTime, &dAudioLatencyTimeInt);

	audio.audLatencyPerfValInt = (uint32_t)dAudioLatencyTimeInt;
	audio.audLatencyPerfValFrac = (uint64_t)(dAudioLatencyTimeFrac * TICK_TIME_FRAC_SCALE);
}

static void setLastWorkingAudioDevName(void)
{
	if (audio.lastWorkingAudioDeviceName != NULL)
	{
		free(audio.lastWorkingAudioDeviceName);
		audio.lastWorkingAudioDeviceName = NULL;
	}

	if (audio.currOutputDevice != NULL)
		audio.lastWorkingAudioDeviceName = strdup(audio.currOutputDevice);
}

bool setupAudio(bool showErrorMsg)
{
	SDL_AudioSpec want, have;

	closeAudio();

	if (config.audioFreq < MIN_AUDIO_FREQ || config.audioFreq > MAX_AUDIO_FREQ)
		config.audioFreq = DEFAULT_AUDIO_FREQ;

	// get audio buffer size from config special flags

	uint16_t configAudioBufSize = 1024;
	if (config.specialFlags & BUFFSIZE_512)
		configAudioBufSize = 512;
	else if (config.specialFlags & BUFFSIZE_2048)
		configAudioBufSize = 2048;

	audio.wantFreq = config.audioFreq;
	audio.wantSamples = configAudioBufSize;
	audio.multichannelFallback = false;

	const uint8_t requestedOutputChannels = tapeheadConfig.outputBuses * 2;
	const bool useJack = tapeheadJackDeviceSelected(audio.currOutputDevice);
	uint32_t openedFreq, openedSamples;
	uint8_t openedChannels;
	SDL_AudioFormat openedFormat;

	if (useJack)
	{
		configureJackDiagnostics(tapeheadConfig.outputBuses);
		if (!tapeheadJackOpen(tapeheadConfig.outputBuses, jackAudioCallback, NULL,
			&openedFreq, &openedSamples))
		{
			if (showErrorMsg)
			{
				showErrorMsgBox(
					"Couldn't open Tapehead JACK virtual outputs:\n%s",
					tapeheadJackGetLastError());
			}
			return false;
		}

		openedChannels = requestedOutputChannels;
		openedFormat = AUDIO_F32;
	}
	else
	{
		// set up SDL audio device
		memset(&want, 0, sizeof (want));
		want.freq = config.audioFreq;
		want.format = (config.specialFlags & BITDEPTH_32) ? AUDIO_F32 : AUDIO_S16;
		want.channels = requestedOutputChannels;
		want.callback = audioCallback;
		want.samples  = configAudioBufSize;

		char *device = audio.currOutputDevice;
		if (device != NULL && strcmp(device, DEFAULT_AUDIO_DEV_STR) == 0)
			device = NULL; // force default device

		audio.dev = SDL_OpenAudioDevice(device, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
		if (audio.dev == 0 && requestedOutputChannels > 2)
		{
			/*
			** Preserve the logical routing even when this device cannot expose the
			** requested channel count. The mixer folds all buses to ordinary stereo.
			*/
			want.channels = 2;
			audio.dev = SDL_OpenAudioDevice(device, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
			audio.multichannelFallback = audio.dev != 0;
		}

		if (audio.dev == 0)
		{
			want.channels = requestedOutputChannels;
			audio.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
			if (audio.dev == 0 && requestedOutputChannels > 2)
			{
				want.channels = 2;
				audio.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
				audio.multichannelFallback = audio.dev != 0;
			}

			if (audio.currOutputDevice != NULL)
			{
				free(audio.currOutputDevice);
				audio.currOutputDevice = NULL;
			}
			audio.currOutputDevice = strdup(DEFAULT_AUDIO_DEV_STR);

			if (audio.dev == 0)
			{
				if (showErrorMsg)
					showErrorMsgBox("Couldn't open audio device:\n\"%s\"\n\nDo you have an audio device enabled and plugged in?", SDL_GetError());

				return false;
			}
		}

		openedFreq = have.freq;
		openedSamples = have.samples;
		openedChannels = have.channels;
		openedFormat = have.format;
	}

	// test if the received audio format is compatible
	if (openedFormat != AUDIO_S16 && openedFormat != AUDIO_F32)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\nThis program only supports 16-bit or 32-bit float audio streams. Sorry!");

		closeAudio();
		return false;
	}

	// test if the received audio stream is compatible

	if (openedChannels < 2 || openedChannels > TAPEHEAD_MAX_OUTPUT_BUSES * 2 ||
		(openedChannels & 1))
	{
		if (showErrorMsg)
		{
			showErrorMsgBox(
				"Couldn't open audio device:\n"
				"Tapehead requires an even output count between 2 and %d channels. Sorry!",
				TAPEHEAD_MAX_OUTPUT_BUSES * 2);
		}

		closeAudio();
		return false;
	}

	/*
	if (have.freq != 44100 && have.freq != 48000 && have.freq != 96000)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\nThis program doesn't support an audio output rate of %dHz. Sorry!", have.freq);

		closeAudio();
		return false;
	}
	*/

	if (!setupAudioBuffers(openedSamples))
	{
		if (showErrorMsg)
			showErrorMsgBox("Not enough memory!");

		closeAudio();
		return false;
	}

	// set new bit depth flag

	config.specialFlags &= ~BITDEPTH_32;
	config.specialFlags |=  BITDEPTH_16;

	if (openedFormat == AUDIO_F32)
	{
		config.specialFlags &= ~BITDEPTH_16;
		config.specialFlags |=  BITDEPTH_32;
	}

	audio.haveFreq = openedFreq;
	audio.haveSamples = openedSamples;
	audio.outputChannels = openedChannels;
	audio.outputBusCount = openedChannels / 2;
	audio.monoOutputMode = tapeheadConfig.monoOutputs;
	initializeMonoChannelOutputRouting((uint8_t)MIN(
		openedChannels, TAPEHEAD_MAX_OUTPUT_BUSES));
	audio.bytesPerFrame = openedChannels *
		((openedFormat == AUDIO_F32) ? sizeof (float) : sizeof (int16_t));
	config.audioFreq = audio.freq = openedFreq;

	calcAudioLatencyVars(openedSamples, openedFreq);

	if (audio.multichannelFallback)
	{
		fprintf(stderr,
			"Tapehead: requested %u stereo output buses, but the selected "
			"device opened in stereo. Logical buses are folded to Bus A.\n",
			tapeheadConfig.outputBuses);
	}

	// make a copy of the new known working audio settings

	audio.lastWorkingAudioFreq = config.audioFreq;
	audio.lastWorkingAudioBits = config.specialFlags & (BITDEPTH_16 + BITDEPTH_32 + BUFFSIZE_512 + BUFFSIZE_1024 + BUFFSIZE_2048);
	setLastWorkingAudioDevName();

	// update config audio radio buttons if we're on that screen at the moment
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
		showConfigScreen();

	updateWavRendererSettings();
	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));

	// don't call stopVoices() in this routine
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		stopVoice(i);

	stopAllScopes();

	// zero tick sample counter so that it will instantly initiate a tick
	audio.tickSampleCounterFrac = audio.tickSampleCounter = 0;

	calcReplayerVars(FT2_REF_AUDIO_RATE, audio.freq);

	if (song.BPM == 0)
		song.BPM = 125;

	setMixerBPM(song.BPM); // this is important

	audio.resetSyncTickTimeFlag = true;

	setWavRenderFrequency(audio.freq);
	setWavRenderBitDepth((config.specialFlags & BITDEPTH_32) ? 32 : 16);

	return true;
}

void closeAudio(void)
{
	if (tapeheadJackIsOpen())
		tapeheadJackClose();

	if (audio.dev > 0)
	{
		SDL_PauseAudioDevice(audio.dev, true);
		SDL_CloseAudioDevice(audio.dev);
		audio.dev = 0;
	}

	freeAudioBuffers();

	audio.callbackOngoing = false;
}
