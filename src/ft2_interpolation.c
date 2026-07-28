#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ft2_header.h"
#include "ft2_structs.h"
#include "ft2_replayer.h"
#include "ft2_pattern_ed.h"
#include "ft2_gui.h"
#include "ft2_keyboard.h"
#include "ft2_interpolation.h"
#include "ft2_undo.h"

static bool previewActive, scaleKeyChosen;
static uint8_t previewType, scalePreset;
static int8_t walkDirection;
static int32_t previewStepLength;
static uint16_t previewPattern;
static note_t patternSnapshot[MAX_PATT_LEN * MAX_CHANNELS];

static const uint16_t scaleMasks[10] =
{
	0x0FFF, // 1: chromatic
	0x0AB5, // 2: major
	0x05AD, // 3: natural minor
	0x0295, // 4: major pentatonic
	0x04A9, // 5: minor pentatonic
	0x06AD, // 6: dorian
	0x06B5, // 7: mixolydian
	0x05B3, // 8: Freygish (Phrygian dominant / Ahava Rabbah)
	0x09AD, // 9: harmonic minor
	0x0000  // 0: repeat (not a pitched scale)
};

static void interpolationError(void)
{
	okBox(0, "System message", "Incompatible interpolation values.", NULL);
}

static void interpolationStepError(void)
{
	okBox(0, "System message", "Non-zero step value required.", NULL);
}

static bool getSelection(int32_t *x1, int32_t *x2, int32_t *y1, int32_t *y2)
{
	*x1 = pattMark.markX1;
	*x2 = pattMark.markX2;
	*y1 = pattMark.markY1;
	*y2 = pattMark.markY2;

	if (*x1 < 0 || *x2 < *x1 || *y1 < 0 || *y2 <= *y1)
		return false;
	if (*x1 >= song.numChannels || *y1 >= patternNumRows[editor.editPattern])
		return false;

	if (*x2 >= song.numChannels) *x2 = song.numChannels - 1;
	if (*y2 > patternNumRows[editor.editPattern]) *y2 = patternNumRows[editor.editPattern];
	return (*y2 - *y1) >= 2;
}

static bool effectIsSupported(uint8_t efx)
{
	return efx == 0x08 || efx == 0x0C; // set panning / set volume
}

static int32_t getScaleLength(uint16_t mask)
{
	int32_t length = 0;
	for (int32_t i = 0; i < 12; i++)
		if (mask & (1 << i)) length++;
	return length;
}

static int32_t scaleDegreeToNote(int32_t rootNote, int32_t degree, uint16_t mask)
{
	const int32_t scaleLength = getScaleLength(mask);
	if (scaleLength <= 0)
		return rootNote;

	int32_t octave = degree / scaleLength;
	int32_t index = degree % scaleLength;
	if (index < 0)
	{
		index += scaleLength;
		octave--;
	}

	int32_t semitone = 0;
	for (int32_t i = 0; i < 12; i++)
	{
		if (mask & (1 << i))
		{
			if (index == 0)
			{
				semitone = i;
				break;
			}
			index--;
		}
	}

	return rootNote + (octave * 12) + semitone;
}

static int32_t nearestScaleDegree(int32_t rootNote, int32_t targetNote, int32_t direction, uint16_t mask)
{
	const int32_t scaleLength = getScaleLength(mask);
	if (scaleLength <= 0)
		return 0;

	int32_t bestDegree = 0;
	int32_t bestDistance = INT32_MAX;
	const int32_t degreeLimit = ((96 / 12) + 2) * scaleLength;
	for (int32_t degree = -degreeLimit; degree <= degreeLimit; degree++)
	{
		const int32_t note = scaleDegreeToNote(rootNote, degree, mask);
		if (note < 1 || note > 96)
			continue;
		if (direction > 0 && note > targetNote)
			continue;
		if (direction < 0 && note < targetNote)
			continue;

		const int32_t distance = abs(targetNote - note);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			bestDegree = degree;
		}
	}

	return bestDegree;
}

static int32_t countTrackAnchors(const note_t *p, int32_t ch, int32_t y1, int32_t y2, int32_t *firstRow, int32_t *lastRow)
{
	int32_t count = 0;
	*firstRow = *lastRow = -1;
	for (int32_t row = y1; row < y2; row++)
	{
		const uint8_t note = p[(row * MAX_CHANNELS) + ch].note;
		if (note >= 1 && note <= 96)
		{
			if (*firstRow < 0) *firstRow = row;
			*lastRow = row;
			count++;
		}
	}
	return count;
}

static bool validateNoteSelection(int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
	const note_t *p = pattern[editor.editPattern];
	int32_t totalAnchors = 0;

	for (int32_t ch = x1; ch <= x2; ch++)
	{
		int32_t firstRow, lastRow;
		const int32_t count = countTrackAnchors(p, ch, y1, y2, &firstRow, &lastRow);
		if (count > 2)
			return false;
		totalAnchors += count;

		if (count == 2)
		{
			const note_t *a = &p[(firstRow * MAX_CHANNELS) + ch];
			const note_t *b = &p[(lastRow * MAX_CHANNELS) + ch];
			if (a->instr != 0 && b->instr != 0 && a->instr != b->instr)
				return false;
		}
	}

	return totalAnchors > 0;
}

static bool validateSelection(uint8_t type, int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
	note_t *p = pattern[editor.editPattern];
	if (p == NULL)
		return false;

	if (type == INTERPOLATE_NOTES)
		return validateNoteSelection(x1, x2, y1, y2);

	for (int32_t ch = x1; ch <= x2; ch++)
	{
		const note_t *a = &p[(y1 * MAX_CHANNELS) + ch];
		const note_t *b = &p[((y2 - 1) * MAX_CHANNELS) + ch];

		if (type == INTERPOLATE_VOLUME)
		{
			if (a->vol < 0x10 || a->vol > 0x50 || b->vol < 0x10 || b->vol > 0x50)
				return false;
			for (int32_t row = y1 + 1; row < y2 - 1; row++)
				if (p[(row * MAX_CHANNELS) + ch].vol != 0)
					return false;
		}
		else
		{
			if (a->efx != b->efx || !effectIsSupported(a->efx))
				return false;
			for (int32_t row = y1 + 1; row < y2 - 1; row++)
			{
				const note_t *n = &p[(row * MAX_CHANNELS) + ch];
				if (n->efx != 0 || n->efxData != 0)
					return false;
			}
		}
	}

	return true;
}

static void paintOpenWalk(note_t *p, int32_t ch, int32_t anchorRow, const note_t *anchor, int32_t y2)
{
	const int32_t step = previewStepLength;
	const uint16_t mask = scaleMasks[scalePreset];
	int32_t degree = walkDirection;

	for (int32_t row = anchorRow + step; row < y2; row += step, degree += walkDirection)
	{
		const int32_t value = scaleDegreeToNote(anchor->note, degree, mask);
		if (value < 1 || value > 96)
			break;

		note_t *n = &p[(row * MAX_CHANNELS) + ch];
		n->note = (uint8_t)value;
		n->instr = anchor->instr;
	}
}

static void paintEndpointWalk(note_t *p, int32_t ch, int32_t startRow, int32_t endRow, const note_t *a, const note_t *b)
{
	const int32_t step = previewStepLength;
	const int32_t direction = b->note >= a->note ? 1 : -1;
	const int32_t endDegree = nearestScaleDegree(a->note, b->note, direction, scaleMasks[scalePreset]);
	const int32_t slotCount = (endRow - startRow + step - 1) / step;
	const uint8_t instrument = a->instr != 0 ? a->instr : b->instr;

	for (int32_t slot = 1, row = startRow + step; row < endRow; slot++, row += step)
	{
		int32_t degree;
		if (endDegree >= 0)
			degree = (endDegree * slot + slotCount / 2) / slotCount;
		else
			degree = (endDegree * slot - slotCount / 2) / slotCount;

		int32_t value = scaleDegreeToNote(a->note, degree, scaleMasks[scalePreset]);
		if (value < 1) value = 1;
		if (value > 96) value = 96;

		note_t *n = &p[(row * MAX_CHANNELS) + ch];
		n->note = (uint8_t)value;
		n->instr = instrument;
	}
}

static bool renderRepeatPreview(note_t *p, int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
	int32_t anchorCh = -1, anchorRow = -1;
	int32_t totalAnchors = 0;

	for (int32_t ch = x1; ch <= x2; ch++)
	{
		int32_t firstRow, lastRow;
		const int32_t count = countTrackAnchors(patternSnapshot, ch, y1, y2, &firstRow, &lastRow);
		totalAnchors += count;
		if (count == 1)
		{
			anchorCh = ch;
			anchorRow = firstRow;
		}
	}

	if (totalAnchors != 1)
		return false;

	const note_t anchor = patternSnapshot[(anchorRow * MAX_CHANNELS) + anchorCh];
	for (int32_t ch = x1; ch <= x2; ch++)
	{
		for (int32_t row = y1; row < y2; row++)
		{
			if (abs(row - anchorRow) % previewStepLength != 0)
				continue;

			note_t *n = &p[(row * MAX_CHANNELS) + ch];
			n->note = anchor.note;
			n->instr = anchor.instr;
		}
	}

	return true;
}

static void renderNotePreview(note_t *p, int32_t x1, int32_t x2, int32_t y1, int32_t y2)
{
	if (scalePreset == 9)
	{
		renderRepeatPreview(p, x1, x2, y1, y2);
		return;
	}

	int32_t totalAnchors = 0;
	int32_t onlyAnchorCh = -1, onlyAnchorRow = -1;
	for (int32_t ch = x1; ch <= x2; ch++)
	{
		int32_t firstRow, lastRow;
		const int32_t count = countTrackAnchors(patternSnapshot, ch, y1, y2, &firstRow, &lastRow);
		totalAnchors += count;
		if (count == 1)
		{
			onlyAnchorCh = ch;
			onlyAnchorRow = firstRow;
		}
	}

	if (totalAnchors == 1)
	{
		const note_t anchor = patternSnapshot[(onlyAnchorRow * MAX_CHANNELS) + onlyAnchorCh];
		for (int32_t ch = x1; ch <= x2; ch++)
		{
			note_t *seed = &p[(onlyAnchorRow * MAX_CHANNELS) + ch];
			seed->note = anchor.note;
			seed->instr = anchor.instr;
			paintOpenWalk(p, ch, onlyAnchorRow, &anchor, y2);
		}
		return;
	}

	for (int32_t ch = x1; ch <= x2; ch++)
	{
		int32_t firstRow, lastRow;
		const int32_t count = countTrackAnchors(patternSnapshot, ch, y1, y2, &firstRow, &lastRow);
		if (count == 1)
		{
			const note_t *anchor = &patternSnapshot[(firstRow * MAX_CHANNELS) + ch];
			paintOpenWalk(p, ch, firstRow, anchor, y2);
		}
		else if (count == 2)
		{
			const note_t *a = &patternSnapshot[(firstRow * MAX_CHANNELS) + ch];
			const note_t *b = &patternSnapshot[(lastRow * MAX_CHANNELS) + ch];
			paintEndpointWalk(p, ch, firstRow, lastRow, a, b);
		}
	}
}

static void renderPreview(void)
{
	int32_t x1, x2, y1, y2;
	if (!getSelection(&x1, &x2, &y1, &y2) || pattern[previewPattern] == NULL)
		return;

	note_t *p = pattern[previewPattern];
	memcpy(p, patternSnapshot, sizeof (patternSnapshot));

	if (previewType == INTERPOLATE_NOTES)
	{
		renderNotePreview(p, x1, x2, y1, y2);
		ui.updatePatternEditor = true;
		return;
	}

	for (int32_t ch = x1; ch <= x2; ch++)
	{
		const note_t *a = &patternSnapshot[(y1 * MAX_CHANNELS) + ch];
		const note_t *b = &patternSnapshot[((y2 - 1) * MAX_CHANNELS) + ch];
		const int32_t span = y2 - 1 - y1;
		const int32_t start = previewType == INTERPOLATE_VOLUME ? a->vol : a->efxData;
		const int32_t end = previewType == INTERPOLATE_VOLUME ? b->vol : b->efxData;
		for (int32_t row = y1 + 1; row < y2 - 1; row++)
		{
			const int32_t numerator = (end - start) * (row - y1);
			const int32_t value = start + (numerator >= 0 ? (numerator + span/2) / span : (numerator - span/2) / span);
			note_t *n = &p[(row * MAX_CHANNELS) + ch];
			if (previewType == INTERPOLATE_VOLUME)
				n->vol = (uint8_t)value;
			else
			{
				n->efx = a->efx;
				n->efxData = (uint8_t)value;
			}
		}
	}

	ui.updatePatternEditor = true;
}

bool interpolationBegin(uint8_t type)
{
	int32_t x1, x2, y1, y2;
	if (previewActive || !ui.patternEditorShown)
		return false;
	if (type == INTERPOLATE_NOTES && editor.editRowSkip == 0)
	{
		interpolationStepError();
		return true;
	}
	if (!getSelection(&x1, &x2, &y1, &y2) || !validateSelection(type, x1, x2, y1, y2))
	{
		interpolationError();
		return true;
	}

	previewPattern = editor.editPattern;
	previewType = type;
	scalePreset = 0;
	walkDirection = 1;
	previewStepLength = editor.editRowSkip;
	scaleKeyChosen = false;
	memcpy(patternSnapshot, pattern[previewPattern], sizeof (patternSnapshot));
	previewActive = true;
	drawIDAdd();
	renderPreview();
	return true;
}

static void cancelPreview(void)
{
	if (pattern[previewPattern] != NULL)
		memcpy(pattern[previewPattern], patternSnapshot, sizeof (patternSnapshot));
	previewActive = false;
	drawIDAdd();
	ui.updatePatternEditor = true;
}

bool interpolationHandlePreviewKey(SDL_Scancode scancode, SDL_Keycode keycode, bool keyWasRepeated)
{
	if (!previewActive)
		return false;

	if (!keyWasRepeated && (keycode == SDLK_RETURN || scancode == SDL_SCANCODE_KP_ENTER))
	{
		/* The preview snapshot is the transaction's before-state. Restore it
		** briefly so the undo manager can capture a normal pattern transaction,
		** then put the accepted preview back and commit it as one step. */
		note_t acceptedPattern[MAX_PATT_LEN * MAX_CHANNELS];
		memcpy(acceptedPattern, pattern[previewPattern], sizeof (acceptedPattern));
		memcpy(pattern[previewPattern], patternSnapshot, sizeof (patternSnapshot));
		undoPatternBegin(previewPattern, previewType == INTERPOLATE_NOTES ? "Melodic walk" :
			(previewType == INTERPOLATE_VOLUME ? "Volume interpolation" : "Effect interpolation"));
		memcpy(pattern[previewPattern], acceptedPattern, sizeof (acceptedPattern));
		undoPatternCommit();

		previewActive = false;
		drawIDAdd();
		setSongModifiedFlag();
		ui.updatePatternEditor = true;
		return true;
	}

	if (!keyWasRepeated && keycode == SDLK_ESCAPE)
	{
		cancelPreview();
		return true;
	}

	/* Playback and modifier controls are part of the live audition workflow.
	** Let them continue through normal input handling without discarding the
	** temporary pattern preview. This also keeps the Fast Tracks logo usable
	** with its Ctrl- and Shift-modified mouse actions. */
	if (keycode == SDLK_SPACE || keycode == SDLK_LCTRL || keycode == SDLK_RCTRL ||
		keycode == SDLK_LSHIFT || keycode == SDLK_RSHIFT)
	{
		return false;
	}

	if (!keyWasRepeated && previewType == INTERPOLATE_NOTES && scancode == SDL_SCANCODE_GRAVE)
	{
		if (keyb.leftShiftPressed)
		{
			previewStepLength--;
			if (previewStepLength <= 0)
			{
				cancelPreview();
				return true;
			}
		}
		else if (previewStepLength < 16)
		{
			previewStepLength++;
		}

		drawIDAdd();
		renderPreview();
		return true;
	}

	if (!keyWasRepeated && previewType == INTERPOLATE_NOTES)
	{
		int32_t preset = -1;
		if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9)
			preset = scancode - SDL_SCANCODE_1;
		else if (scancode == SDL_SCANCODE_0)
			preset = 9;

		if (preset >= 0)
		{
			if (scaleKeyChosen && scalePreset == (uint8_t)preset)
				walkDirection = -walkDirection;
			else
			{
				scalePreset = (uint8_t)preset;
				walkDirection = 1;
				scaleKeyChosen = true;
			}
			renderPreview();
			return true;
		}
	}

	cancelPreview(); // unrelated key continues through normal handling
	return false;
}

bool interpolationPreviewActive(void)
{
	return previewActive;
}

uint8_t interpolationGetDisplayedStep(void)
{
	if (previewActive && previewType == INTERPOLATE_NOTES)
		return (uint8_t)previewStepLength;

	return editor.editRowSkip;
}
