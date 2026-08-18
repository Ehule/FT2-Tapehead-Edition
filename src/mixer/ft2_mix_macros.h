#pragma once

#include "../ft2_audio.h"

/* ----------------------------------------------------------------------- */
/*                          GENERAL MIXER MACROS                           */
/* ----------------------------------------------------------------------- */

#define GET_VOL \
	const float fVolumeL = audio.monoOutputMode ? v->fCurrVolumeMono : v->fCurrVolumeL; \
	const float fVolumeR = audio.monoOutputMode ? 0.0f : v->fCurrVolumeR;

#define GET_VOL_RAMP \
	fVolumeL = audio.monoOutputMode ? v->fCurrVolumeMono : v->fCurrVolumeL; \
	fVolumeR = audio.monoOutputMode ? 0.0f : v->fCurrVolumeR;

#define SET_VOL_BACK \
	if (audio.monoOutputMode) \
		v->fCurrVolumeMono = fVolumeL; \
	else \
	{ \
		v->fCurrVolumeL = fVolumeL; \
		v->fCurrVolumeR = fVolumeR; \
	}

#define GET_MIXER_VARS \
	const uint64_t delta = v->delta; \
	fMixBufferL = audio.fMixBufferL + bufferPos; \
	fMixBufferR = audio.fMixBufferR + bufferPos; \
	position = v->position; \
	positionFrac = v->positionFrac;

#define GET_MIXER_VARS_RAMP \
	const uint64_t delta = v->delta; \
	fMixBufferL = audio.fMixBufferL + bufferPos; \
	fMixBufferR = audio.fMixBufferR + bufferPos; \
	fVolumeLDelta = audio.monoOutputMode ? v->fVolumeMonoDelta : v->fVolumeLDelta; \
	fVolumeRDelta = audio.monoOutputMode ? 0.0f : v->fVolumeRDelta; \
	position = v->position; \
	positionFrac = v->positionFrac;

#define PREPARE_TAP_FIX8 \
	const int8_t *loopStartPtr = &v->base8[v->loopStart]; \
	const int8_t *leftEdgePtr = loopStartPtr+MAX_LEFT_TAPS;

#define PREPARE_TAP_FIX16 \
	const int16_t *loopStartPtr = &v->base16[v->loopStart]; \
	const int16_t *leftEdgePtr = loopStartPtr+MAX_LEFT_TAPS;

#define SET_BASE8 \
	base = v->base8; \
	smpPtr = base + position;

#define SET_BASE16 \
	base = v->base16; \
	smpPtr = base + position;

#define SET_BASE8_PINGPONG \
	base = v->base8; \
	revBase = v->revBase8;

#define SET_BASE16_PINGPONG \
	base = v->base16; \
	revBase = v->revBase16;

#define INC_POS \
	positionFrac += delta; \
	smpPtr += positionFrac >> MIXER_FRAC_BITS; \
	positionFrac &= MIXER_FRAC_MASK;

#define INC_POS_PINGPONG \
	positionFrac += deltaLo; \
	smpPtr += positionFrac >> MIXER_FRAC_BITS; \
	smpPtr += deltaHi; \
	positionFrac &= MIXER_FRAC_MASK;

#define SET_BACK_MIXER_POS \
	v->positionFrac = positionFrac; \
	v->position = position;

#define VOLUME_RAMPING \
	fVolumeL += fVolumeLDelta; \
	fVolumeR += fVolumeRDelta;

/* It may look like we are potentially going out of bounds while looking up the sample points,
** but the sample data is actually padded on both the left (negative) and right side, where correct tap
** samples are stored according to loop mode (or no loop).
**
** There is also a second special case for the left edge (negative taps) after the sample has looped once.
*/

/* ----------------------------------------------------------------------- */
/*                            NO INTERPOLATION                             */
/* ----------------------------------------------------------------------- */

#define RENDER_8BIT_SMP \
	fSample = *smpPtr * (1.0f / 128.0f); \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP \
	fSample = *smpPtr * (1.0f / 32768.0f); \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* ----------------------------------------------------------------------- */
/*                          LINEAR INTERPOLATION                           */
/* ----------------------------------------------------------------------- */

#define LINEAR_INTERPOLATION8(s, f) \
{ \
	const int16_t frac = (int16_t)((uint32_t)(f) >> (MIXER_FRAC_BITS-15)); /* 0..32767 */ \
	fSample = ((s[0] << 8) + ((((s[1] - s[0]) << 8) * frac) >> 15)) * (1.0f / 32768.0f); \
}

#define LINEAR_INTERPOLATION16(s, f) \
{ \
	const int16_t frac = (int16_t)((uint32_t)(f) >> (MIXER_FRAC_BITS-15)); /* 0..32767 */ \
	fSample = (s[0] + (((s[1] - s[0]) * frac) >> 15)) * (1.0f / 32768.0f); \
}

#define RENDER_8BIT_SMP_LINTRP \
	LINEAR_INTERPOLATION8(smpPtr, positionFrac) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_LINTRP \
	LINEAR_INTERPOLATION16(smpPtr, positionFrac) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* ----------------------------------------------------------------------- */
/*                       CUBIC SPLINE INTERPOLATION                        */
/* ----------------------------------------------------------------------- */

// Catmull-Rom algorithm
#define CUBIC_SPLINE_INTERPOLATION(s, f, scale) \
{ \
	const int32_t frac31 = (uint32_t)f >> 1; /* reduce from uint32_t to int32_t for fast SIMD usage */ \
	const float x = frac31 * (1.0f / ((float)INT32_MAX+1.0f)); \
	\
	const float v0 = s[-1]; \
	const float v1 = s[0]; \
	const float v2 = s[1]; \
	const float v3 = s[2]; \
	\
	const float c1 = v1; \
	const float c2 = 0.5f * (v2 - v0); \
	const float c3 = v0 - (2.5f * v1) + (2.0f * v2) - (0.5f * v3); \
	const float c4 = 0.5f * (v3 - v0) + 1.5f * (v1 - v2); \
	\
	fSample = (((c4 * x + c3) * x + c2) * x + c1) * (1.0f / scale); \
}

#define RENDER_8BIT_SMP_CINTRP \
	CUBIC_SPLINE_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_CINTRP \
	CUBIC_SPLINE_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* Special left-edge case mixers to get proper tap data after one loop cycle.
** These are only used on looped samples.
*/

#define RENDER_8BIT_SMP_CINTRP_TAP_FIX  \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int8_t *)&v->leftEdgeTaps8[(int32_t)(smpPtr-loopStartPtr)] : (int8_t *)smpPtr; \
	CUBIC_SPLINE_INTERPOLATION(smpTapPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_CINTRP_TAP_FIX \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int16_t *)&v->leftEdgeTaps16[(int32_t)(smpPtr-loopStartPtr)] : (int16_t *)smpPtr; \
	CUBIC_SPLINE_INTERPOLATION(smpTapPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* ----------------------------------------------------------------------- */
/*                       WINDOWED-SINC INTERPOLATION                       */
/* ----------------------------------------------------------------------- */

#define WINDOWED_SINC8_INTERPOLATION(s, f, scale) \
{ \
	const uint32_t frac32 = (uint32_t)f; \
	const uint32_t lutPhase = frac32 >> INTRP_PHASE_SHIFT; \
	const float fIntrpFrac = (int32_t)(frac32 & INTRP_PHASE_MASK) * (1.0f / INTRP_PHASE_SCALE); \
	\
	/* it may look like we go out of bounds for fSinc_2, but we have an extra phase after LUT */ \
	const float *fSinc_1 = v->fSincLUT + ( lutPhase    << SINC8_TAPS_BITS); \
	const float *fSinc_2 = v->fSincLUT + ((lutPhase+1) << SINC8_TAPS_BITS); \
	\
	float fSum = 0.0f; \
	\
	/* I hope your compiler vectorizes this well ;) */ \
	for (int32_t j = 0; j < SINC8_TAPS; j++) \
	{ \
		/* do linear interpolation between phases */ \
		const float y1 = fSinc_1[j]; \
		const float y2 = fSinc_2[j]; \
		\
		/* out of bounds for s[] is safe here and returns the correct samples */ \
		fSum += s[j-((SINC8_TAPS/2)-1)] * (y1 + ((y2 - y1) * fIntrpFrac)); \
	} \
	\
	fSample = fSum * (1.0f / scale); \
}

#define WINDOWED_SINC16_INTERPOLATION(s, f, scale) \
{ \
	const uint32_t frac32 = (uint32_t)f; \
	const uint32_t lutPhase = frac32 >> INTRP_PHASE_SHIFT; \
	const float fIntrpFrac = (int32_t)(frac32 & INTRP_PHASE_MASK) * (1.0f / INTRP_PHASE_SCALE); \
	\
	/* it may look like we go out of bounds for fSinc_2, but we have an extra phase after LUT */ \
	const float *fSinc_1 = v->fSincLUT + ( lutPhase    << SINC16_TAPS_BITS); \
	const float *fSinc_2 = v->fSincLUT + ((lutPhase+1) << SINC16_TAPS_BITS); \
	\
	float fSum = 0.0f; \
	\
	/* I hope your compiler vectorizes this well ;) */ \
	for (int32_t j = 0; j < SINC16_TAPS; j++) \
	{ \
		/* do linear interpolation between phases */ \
		const float y1 = fSinc_1[j]; \
		const float y2 = fSinc_2[j]; \
		\
		/* out of bounds for s[] is safe here and returns the correct samples */ \
		fSum += s[j-((SINC16_TAPS/2)-1)] * (y1 + ((y2 - y1) * fIntrpFrac)); \
	} \
	\
	fSample = fSum * (1.0f / scale); \
}

#define RENDER_8BIT_SMP_S8INTRP \
	WINDOWED_SINC8_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_S8INTRP \
	WINDOWED_SINC8_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_8BIT_SMP_S16INTRP \
	WINDOWED_SINC16_INTERPOLATION(smpPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_S16INTRP \
	WINDOWED_SINC16_INTERPOLATION(smpPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

/* Special left-edge case mixers to get proper tap data after one loop cycle.
** These are only used on looped samples.
*/

#define RENDER_8BIT_SMP_S8INTRP_TAP_FIX  \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int8_t *)&v->leftEdgeTaps8[(int32_t)(smpPtr-loopStartPtr)] : (int8_t *)smpPtr; \
	WINDOWED_SINC8_INTERPOLATION(smpTapPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_S8INTRP_TAP_FIX \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int16_t *)&v->leftEdgeTaps16[(int32_t)(smpPtr-loopStartPtr)] : (int16_t *)smpPtr; \
	WINDOWED_SINC8_INTERPOLATION(smpTapPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_8BIT_SMP_S16INTRP_TAP_FIX  \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int8_t *)&v->leftEdgeTaps8[(int32_t)(smpPtr-loopStartPtr)] : (int8_t *)smpPtr; \
	WINDOWED_SINC16_INTERPOLATION(smpTapPtr, positionFrac, 128) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;

#define RENDER_16BIT_SMP_S16INTRP_TAP_FIX \
	smpTapPtr = (smpPtr <= leftEdgePtr) ? (int16_t *)&v->leftEdgeTaps16[(int32_t)(smpPtr-loopStartPtr)] : (int16_t *)smpPtr; \
	WINDOWED_SINC16_INTERPOLATION(smpTapPtr, positionFrac, 32768) \
	*fMixBufferL++ += fSample * fVolumeL; \
	*fMixBufferR++ += fSample * fVolumeR;


/* ----------------------------------------------------------------------- */
/*                      SAMPLES-TO-MIX LIMITING MACROS                     */
/* ----------------------------------------------------------------------- */

#define LIMIT_MIX_NUM \
	samplesToMix = INT32_MAX; \
	if (v->delta != 0) \
	{ \
		i = (v->sampleEnd - 1) - position; \
		const uint64_t dividend = ((uint64_t)i << MIXER_FRAC_BITS) | ((uint32_t)positionFrac ^ MIXER_FRAC_MASK); \
		samplesToMix = (uint32_t)(dividend / (uint64_t)v->delta) + 1; \
	} \
	\
	if (samplesToMix > samplesLeft) \
		samplesToMix = samplesLeft;

#define START_PINGPONG \
	if (v->samplingBackwards) \
	{ \
		tmpDelta = 0 - delta; \
		position = ~position; \
		smpPtr = revBase + position; \
		positionFrac ^= MIXER_FRAC_MASK; \
	} \
	else \
	{ \
		tmpDelta = delta; \
		smpPtr = base + position; \
	} \
	\
	const int32_t deltaHi = (int64_t)tmpDelta >> MIXER_FRAC_BITS; \
	const uint32_t deltaLo = tmpDelta & MIXER_FRAC_MASK;

#define LIMIT_MIX_NUM_RAMP \
	if (v->volumeRampLength == 0) \
	{ \
		fVolumeLDelta = fVolumeRDelta = 0.0f; \
		if (v->isFadeOutVoice) \
		{ \
			v->active = false; /* volume ramp fadeout-voice is done, shut it down */ \
			return; \
		} \
	} \
	else \
	{ \
		if (samplesToMix > v->volumeRampLength) \
			samplesToMix = v->volumeRampLength; \
		\
		v->volumeRampLength -= samplesToMix; \
	}

#define HANDLE_SAMPLE_END \
	position = (int32_t)(smpPtr - base); \
	if (position >= v->sampleEnd) \
	{ \
		v->active = false; \
		return; \
	}

#define WRAP_LOOP \
	position = (int32_t)(smpPtr - base); \
	if (position >= v->sampleEnd) \
	{ \
		do \
		{ \
			position -= v->loopLength; \
		} \
		while (position >= v->sampleEnd); \
		\
		smpPtr = base + position; \
		\
		v->hasLooped = true; \
	}

#define WRAP_PINGPONG_LOOP \
	if (position >= v->sampleEnd) \
	{ \
		if (v->oneShot) \
		{ \
			v->active = false; \
			return; \
		} \
		do \
		{ \
			position -= v->loopLength; \
			if (v->reverseLoop) \
				v->samplingBackwards = true; \
			else \
				v->samplingBackwards ^= 1; \
		} \
		while (position >= v->sampleEnd); \
		v->hasLooped = true; \
	}

#define END_PINGPONG \
	if (v->samplingBackwards) \
	{ \
		positionFrac ^= MIXER_FRAC_MASK; \
		position = ~(int32_t)(smpPtr - revBase); \
	} \
	else \
	{ \
		position = (int32_t)(smpPtr - base); \
	}
