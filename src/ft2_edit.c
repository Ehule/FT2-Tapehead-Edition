// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ft2_header.h"
#include "ft2_undo.h"
#include "ft2_config.h"
#include "ft2_keyboard.h"
#include "ft2_audio.h"
#include "ft2_video.h"
#include "ft2_gui.h"
#include "ft2_midi.h"
#include "ft2_pattern_ed.h"
#include "ft2_sysreqs.h"
#include "ft2_textboxes.h"
#include "ft2_tables.h"
#include "ft2_structs.h"
#include "ft2_fasttracks.h"
#include "ft2_pattern_launcher_ui.h"

enum
{
	KEYTYPE_NUM = 0,
	KEYTYPE_ALPHA = 1
};

static double dVolScaleFK1 = 1.0, dVolScaleFK2 = 1.0;
static bool transposeViewMode;

// for block cut/copy/paste
static bool blockCopied;
static uint16_t ptnBufLen, trkBufLen;
static int32_t markXSize, markYSize;
static note_t blkCopyBuff[MAX_PATT_LEN * MAX_CHANNELS];
static note_t ptnCopyBuff[MAX_PATT_LEN * MAX_CHANNELS];
static note_t trackCopyBuff[MAX_PATT_LEN];

// for recordNote()
static const int8_t tickArr[16] = { 16, 8, 0, 4, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1 };

void recordNote(uint8_t note, int8_t vol);

// when the cursor is at the note slot
static bool testNoteKeys(SDL_Scancode scancode)
{
	// prevent Enter/Return used to close text editing from triggering a sample
	if (scancode == SDL_SCANCODE_RETURN && keyb.ignoreNoteEnterKey)
		return false;

	const int8_t noteNum = scancodeKeyToNote(scancode);
	if (noteNum == NOTE_OFF)
	{
		bool editmode = ui.patternEditorShown && (playMode == PLAYMODE_EDIT);

		// inserts "note off" if editing song
		if (editmode || playMode == PLAYMODE_RECPATT || playMode == PLAYMODE_RECSONG)
		{
			pauseMusic();
			const volatile uint16_t curPattern = editor.editPattern;
			int16_t row = editor.row;
			resumeMusic();

			if (!undoPatternBegin(curPattern, "Enter note-off"))
				return true;
			if (!allocatePattern(curPattern))
			{
				undoCancelTransaction();
				return true; // key pressed
			}

			pattern[curPattern][(row * MAX_CHANNELS) + cursor.ch].note = NOTE_OFF;

			const uint16_t numRows = patternNumRows[curPattern];
			if (playMode == PLAYMODE_EDIT && numRows >= 1)
				setSongPos(-1, (row + editor.editRowSkip) % numRows, RESET_SONG_TICK);

			ui.updatePatternEditor = true;
			setSongModifiedFlag();
			undoPatternCommit();
		}

		return true; // key pressed
	}

	if (noteNum > 0 && noteNum <= 96)
	{
		recordNote(noteNum, -1);
		return true; // note key pressed (and note triggered)
	}

	return false; // no note key pressed
}

// when the cursor is at the note slot
void testNoteKeysRelease(SDL_Scancode scancode)
{
	const int8_t noteNum = scancodeKeyToNote(scancode); // convert key scancode to note number
	if (noteNum > 0 && noteNum <= 96)
		recordNote(noteNum, 0); // release note
}

static bool testEditKeys(SDL_Scancode scancode, SDL_Keycode keycode)
{
	int8_t i;

	bool editmode = ui.patternEditorShown && (playMode == PLAYMODE_EDIT);

	if (cursor.object == CURSOR_NOTE)
	{
		// the edit cursor is at the note slot

		if (testNoteKeys(scancode))
		{
			keyb.keyRepeat = editmode; // repeat keys only if in edit mode
			return true; // we jammed an instrument
		}

		return false; // no note key pressed, test other keys
	}
	if (!editmode && playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT)
		return false; // we're not editing, test other keys

	// convert key to slot data

	if (cursor.object == CURSOR_VOL1)
	{
		// volume column effect type (mixed keys)

		for (i = 0; i < KEY2VOL_ENTRIES; i++)
		{
			if (keycode == key2VolTab[i])
				break;
		}

		if (i == KEY2VOL_ENTRIES)
		{
			// volume column key not found, let's try a hack for '-' and '+' keys first
			if (scancode == SDL_SCANCODE_MINUS)
				i = 5;
			else if (scancode == SDL_SCANCODE_EQUALS)
				i = 6;
			else
				i = -1; // invalid key for slot
		}
	}
	else if (cursor.object == CURSOR_EFX0 || cursor.object == CURSOR_TUNE0)
	{
		// effect type (mixed keys)

		for (i = 0; i < KEY2EFX_ENTRIES; i++)
		{
			if (keycode == key2EfxTab[i])
				break;
		}

		if (i == KEY2EFX_ENTRIES || (cursor.object == CURSOR_TUNE0 &&
			i != TAPEHEAD_EFX_MICROTUNE && i != TAPEHEAD_EFX_MICRODRIFT))
			i = -1; // invalid key for slot
	}
	else
	{
		// all other slots (hex keys)

		for (i = 0; i < KEY2HEX_ENTRIES; i++)
		{
			if (keycode == key2HexTab[i])
				break;
		}

		if (i == KEY2HEX_ENTRIES)
			i = -1; // invalid key for slot
	}

	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	if (i == -1)
		return false; // no edit to be done

	const char *undoDescription = "Enter pattern data";
	if (cursor.object == CURSOR_INST1 || cursor.object == CURSOR_INST2) undoDescription = "Enter instrument";
	else if (cursor.object == CURSOR_VOL1 || cursor.object == CURSOR_VOL2) undoDescription = "Enter volume";
	else if (cursor.object == CURSOR_TUNE0 || cursor.object == CURSOR_TUNE1 || cursor.object == CURSOR_TUNE2) undoDescription = "Enter tuning/drift";
	else if (cursor.object == CURSOR_EFX0 || cursor.object == CURSOR_EFX1 || cursor.object == CURSOR_EFX2) undoDescription = "Enter effect";
	if (!undoPatternBegin(curPattern, undoDescription))
		return true;
	if (!allocatePattern(curPattern))
	{
		undoCancelTransaction();
		return false;
	}

	// insert slot data

	note_t *p = &pattern[curPattern][(row * MAX_CHANNELS) + cursor.ch];
	const note_t oldNote = *p;
	switch (cursor.object)
	{
		case CURSOR_INST1:
		{

			p->instr = (p->instr & 0x0F) | (i << 4);
			if (p->instr > MAX_INST)
				p->instr = MAX_INST;

		}
		break;

		case CURSOR_INST2:
		{
			p->instr = (p->instr & 0xF0) | i;

		}
		break;

		case CURSOR_VOL1:
		{

			p->vol = (p->vol & 0x0F) | ((i + 1) << 4);
			if (p->vol >= 0x51 && p->vol <= 0x5F)
				p->vol = 0x50;

		}
		break;

		case CURSOR_VOL2:
		{

			if (p->vol < 0x10)
				p->vol = 0x10 + i;
			else
				p->vol = (p->vol & 0xF0) | i;

			if (p->vol >= 0x51 && p->vol <= 0x5F)
				p->vol = 0x50;

		}
		break;

		case CURSOR_EFX0:
		{

			p->efx = i;
		}
		break;

		case CURSOR_TUNE0:
			p->tuneType = (uint8_t)i;
			if (i == 0) p->tuneData = 0;
		break;

		case CURSOR_TUNE1:
			p->tuneData = (p->tuneData & 0x0F) | (i << 4);
		break;

		case CURSOR_TUNE2:
			p->tuneData = (p->tuneData & 0xF0) | i;
		break;

		case CURSOR_EFX1:
		{

			p->efxData = (p->efxData & 0x0F) | (i << 4);
		}
		break;

		case CURSOR_EFX2:
		{

			p->efxData = (p->efxData & 0xF0) | i;
		}
		break;

		default: break;
	}

	// increase row (only in edit mode)

	const int16_t numRows = patternNumRows[curPattern];
	if (playMode == PLAYMODE_EDIT && numRows >= 1)
		setSongPos(-1, (row + editor.editRowSkip) % numRows, RESET_SONG_TICK);

	const bool patternDataChanged = memcmp(&oldNote, p, sizeof (note_t)) != 0;
	if (i == 0) // if we inserted a zero, check if pattern is empty
		killPatternIfUnused(curPattern);

	if (patternDataChanged)
	{
		setSongModifiedFlag();
		undoPatternCommit();
	}
	else
	{
		undoCancelTransaction();
	}

	ui.updatePatternEditor = true;
	return true;
}

static void evaluateTimeStamp(int16_t *songPos, int16_t *pattNum, int16_t *row, int16_t *tick)
{
	int16_t outSongPos = editor.songPos;
	int16_t outPattern = editor.editPattern;
	int16_t outRow = editor.row;
	int16_t outTick = editor.speed - editor.tick;

	outTick = CLAMP(outTick, 0, editor.speed-1);

	// this is needed, but also breaks quantization on speed>15
	if (outTick > 15)
		outTick = 15;

	const int16_t numRows = patternNumRows[outPattern];

	if (config.recQuant > 0)
	{
		if (config.recQuantRes >= 16)
		{
			outTick += (editor.speed >> 1) + 1;
		}
		else
		{
			int16_t r = tickArr[config.recQuantRes-1];
			int16_t p = outRow & (r - 1);

			if (p < (r >> 1))
				outRow -= p;
			else
				outRow = (outRow + r) - p;

			outTick = 0;
		}
	}

	if (outTick > editor.speed)
	{
		outTick -= editor.speed;
		outRow++;
	}

	if (outRow >= numRows)
	{
		outRow = 0;

		if (playMode == PLAYMODE_RECSONG)
			outSongPos++;

		if (outSongPos >= song.songLength)
			outSongPos = song.songLoopStart;

		outPattern = song.orders[outSongPos];
	}

	*songPos = outSongPos;
	*pattNum = outPattern;
	*row = outRow;
	*tick = outTick;
}

void recordNote(uint8_t noteNum, int8_t vol) // directly ported from the original FT2 code - what a mess, but it works...
{
	if (noteNum >= 1 && noteNum <= 96)
		setAuditionNoteState(noteNum, vol != 0);

	int8_t i;
	int16_t pattNum, songPos, row, tick;
	int32_t time;
	note_t *p;

	const int16_t oldRow = editor.row;

	if (songPlaying)
	{
		// row quantization
		evaluateTimeStamp(&songPos, &pattNum, &row, &tick);
	}
	else
	{
		songPos = editor.songPos;
		pattNum = editor.editPattern;
		row = editor.row;
		tick = 0;
	}

	bool editmode = ui.patternEditorShown && (playMode == PLAYMODE_EDIT);
	bool recmode = (playMode == PLAYMODE_RECSONG) || (playMode == PLAYMODE_RECPATT);

	if (noteNum == NOTE_OFF)
		vol = 0;

	int8_t c = -1;
	int8_t k = -1;

	if (editmode || recmode)
	{
		// find out what channel is the most suitable in edit/record mode

		if ((config.multiEdit && editmode) || (config.multiRec && recmode))
		{
			time = INT32_MAX;
			for (i = 0; i < song.numChannels; i++)
			{
				if (!editor.channelMuted[i] && config.multiRecChn[i] && editor.keyOffTime[i] < time && editor.keyOnTab[i] == 0)
				{
					c = i;
					time = editor.keyOffTime[i];
				}
			}
		}
		else
		{
			c = cursor.ch;
		}

		for (i = 0; i < song.numChannels; i++)
		{
			if (noteNum == editor.keyOnTab[i] && config.multiRecChn[i])
				k = i;
		}
	}
	else
	{
		// find out what channel is the most suitable in idle/play mode (jamming)
		if (config.multiKeyJazz)
		{
			time = INT32_MAX;
			c = 0;

			if (songPlaying)
			{
				for (i = 0; i < song.numChannels; i++)
				{
					if (editor.keyOffTime[i] < time && editor.keyOnTab[i] == 0 && config.multiRecChn[i])
					{
						c = i;
						time = editor.keyOffTime[i];
					}
				}
			}

			if (time == INT32_MAX)
			{
				for (i = 0; i < song.numChannels; i++)
				{
					if (editor.keyOffTime[i] < time && editor.keyOnTab[i] == 0)
					{
						c = i;
						time = editor.keyOffTime[i];
					}
				}
			}
		}
		else
		{
			c = cursor.ch;
		}

		for (i = 0; i < song.numChannels; i++)
		{
			if (noteNum == editor.keyOnTab[i])
				k = i;
		}
	}

	if (vol != 0)
	{
		if (c < 0 || (k >= 0 && (config.multiEdit || (recmode || !editmode))))
			return;

		// play note

		editor.keyOnTab[c] = noteNum;

			 if (row >= oldRow &&
                   	     !(songPlaying && recmode &&
                               (config.specialFlags2 & SILENT_REC_ENTRY))) // suppress audition while recording playback
		{
#ifdef HAS_MIDI
			playTone(c, editor.curInstr, noteNum, vol, midi.currMIDIVibDepth, midi.currMIDIPitch);
#else
			playTone(c, editor.curInstr, noteNum, vol, 0, 0);
#endif
		}

		if (editmode || recmode)
		{
			if (!undoPatternBegin((uint16_t)pattNum, "Enter note"))
				return;
			if (allocatePattern(pattNum))
			{
				const int16_t numRows = patternNumRows[pattNum];
				p = &pattern[pattNum][(row * MAX_CHANNELS) + c];

				// insert data
				p->note = noteNum;
				if (editor.curInstr > 0)
					p->instr = editor.curInstr;

				if (vol >= 0)
					p->vol = 0x10 + vol;

				if (!recmode)
				{
					if (numRows >= 1)
						setSongPos(-1, (editor.row + editor.editRowSkip) % numRows, RESET_SONG_TICK);
				}
				else if (!config.recQuant && tick > 0)
				{
					p->efx = 0x0E;
					p->efxData = 0xD0 + (tick & 0x0F);
				}

				ui.updatePatternEditor = true;
				setSongModifiedFlag();
				undoPatternCommit();
			}
			else
			{
				undoCancelTransaction();
			}
		}
	}
	else
	{
		// note off

		if (k != -1)
			c = k;

		if (c < 0)
			return;

		editor.keyOffNr++;

		editor.keyOnTab[c] = 0;
		editor.keyOffTime[c] = editor.keyOffNr;

		if (row >= oldRow &&
		    !(songPlaying && recmode &&
		      (config.specialFlags2 & SILENT_REC_ENTRY))) //suppress audtion while recording playback
		{
#ifdef HAS_MIDI
			playTone(c, editor.curInstr, NOTE_OFF, vol, midi.currMIDIVibDepth, midi.currMIDIPitch);
#else
			playTone(c, editor.curInstr, NOTE_OFF, vol, 0, 0);
#endif
		}

		if (config.recRelease && recmode)
		{
			const uint16_t firstPattern = (uint16_t)pattNum;
			if (!undoTransactionBegin("Record note-off") ||
				!undoTransactionAddPattern(firstPattern))
			{
				undoCancelTransaction();
				return;
			}

			if (!allocatePattern(pattNum))
			{
				undoCancelTransaction();
				return;
			}

			int16_t numRows = patternNumRows[pattNum];
			p = &pattern[pattNum][(row * MAX_CHANNELS) + c];
			if (p->note != 0)
				row++;

			if (row >= numRows)
			{
				row = 0;
				if (songPlaying)
				{
					songPos++;
					if (songPos >= song.songLength)
						songPos = song.songLoopStart;
					pattNum = song.orders[songPos];
					numRows = patternNumRows[pattNum];
				}
			}

			if ((uint16_t)pattNum != firstPattern &&
				!undoTransactionAddPattern((uint16_t)pattNum))
			{
				undoCancelTransaction();
				return;
			}
			if (pattern[pattNum] == NULL && !allocatePattern(pattNum))
			{
				undoCancelTransaction();
				return;
			}

			p = &pattern[pattNum][(row * MAX_CHANNELS) + c];
			p->note = NOTE_OFF;
			if (!config.recQuant && tick > 0)
			{
				p->efx = 0x0E;
				p->efxData = 0xD0 + (tick & 0x0F);
			}

			ui.updatePatternEditor = true;
			setSongModifiedFlag();
			undoTransactionCommit();
		}
	}
}

bool handleEditKeys(SDL_Keycode keycode, SDL_Scancode scancode)
{
	// special case for delete - manipulate note data
	if (keycode == SDLK_DELETE)
	{
		bool editmode = ui.patternEditorShown && (playMode == PLAYMODE_EDIT);
		if (!editmode && playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT)
			return false; // we're not editing, test other keys

		pauseMusic();
		const volatile uint16_t curPattern = editor.editPattern;
		int16_t row = editor.row;
		resumeMusic();

		if (pattern[curPattern] == NULL)
			return true;

		if (!undoPatternBegin(curPattern, "Delete pattern entry"))
			return true;
		note_t *p = &pattern[curPattern][(row * MAX_CHANNELS) + cursor.ch];
		const note_t oldNote = *p;

		if (keyb.leftShiftPressed)
		{
			// delete all
			memset(p, 0, sizeof (*p));
		}
		else if (keyb.leftCtrlPressed)
		{
			// delete volume column + effect
			p->vol = 0;
			p->efx = 0;
			p->efxData = 0;
		}
		else if (keyb.leftAltPressed)
		{
			// delete effect
			p->efx = 0;
			p->efxData = 0;
		}
		else
		{
			if (cursor.object == CURSOR_VOL1 || cursor.object == CURSOR_VOL2)
			{
				// delete volume column
				p->vol = 0;
			}
			else if (cursor.object == CURSOR_TUNE0 || cursor.object == CURSOR_TUNE1 || cursor.object == CURSOR_TUNE2)
			{
				p->tuneType = p->tuneData = 0;
			}
			else if (cursor.object == CURSOR_EFX0 || cursor.object == CURSOR_EFX1 || cursor.object == CURSOR_EFX2)
			{
				p->efx = p->efxData = 0;
			}
			else
			{
				// delete note + instrument
				p->note = 0;
				p->instr = 0;
			}
		}

		const bool patternDataChanged = memcmp(&oldNote, p, sizeof (note_t)) != 0;
		killPatternIfUnused(curPattern);

		// increase row (only in edit mode)
		const int16_t numRows = patternNumRows[curPattern];
		if (playMode == PLAYMODE_EDIT && numRows >= 1)
			setSongPos(-1, (row + editor.editRowSkip) % numRows, RESET_SONG_TICK);

		ui.updatePatternEditor = true;
		if (patternDataChanged)
		{
			setSongModifiedFlag();
			undoPatternCommit();
		}
		else
		{
			undoCancelTransaction();
		}

		return true;
	}

	// a kludge for french keyb. layouts to allow writing numbers in the pattern data with left SHIFT
	const bool frKeybHack = keyb.leftShiftPressed && !keyb.leftAltPressed && !keyb.leftCtrlPressed &&
	               (scancode >= SDL_SCANCODE_1) && (scancode <= SDL_SCANCODE_0);

	if (frKeybHack || !keyb.keyModifierDown)
		return testEditKeys(scancode, keycode);

	return false;
}

void writeToMacroSlot(uint8_t slot)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	uint16_t writeVol = 0;
	uint16_t writeEfx = 0;

	if (pattern[curPattern] != NULL)
	{
		note_t *p = &pattern[curPattern][(row * MAX_CHANNELS) + cursor.ch];
		writeVol = p->vol;
		writeEfx = (p->efx << 8) | p->efxData;
	}

	if (cursor.object == CURSOR_VOL1 || cursor.object == CURSOR_VOL2)
		config.volMacro[slot] = writeVol;
	else
		config.comMacro[slot] = writeEfx;
}

void writeFromMacroSlot(uint8_t slot)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	bool editmode = ui.patternEditorShown && (playMode == PLAYMODE_EDIT);
	if (!editmode && playMode != PLAYMODE_RECSONG && playMode != PLAYMODE_RECPATT)
		return;

	if (!undoPatternBegin(curPattern, "Insert macro"))
		return;
	if (!allocatePattern(curPattern))
	{
		undoCancelTransaction();
		return;
	}
	
	note_t *p = &pattern[curPattern][(row * MAX_CHANNELS) + cursor.ch];
	if (cursor.object == CURSOR_VOL1 || cursor.object == CURSOR_VOL2)
	{
		p->vol = (uint8_t)config.volMacro[slot];
	}
	else
	{
		uint8_t efx = (uint8_t)(config.comMacro[slot] >> 8);
		if (efx > 35)
		{
			// illegal effect
			p->efx = 0;
			p->efxData = 0;
		}
		else
		{
			p->efx = efx;
			p->efxData = config.comMacro[slot] & 0xFF;
		}
	}

	const int16_t numRows = patternNumRows[curPattern];
	if (playMode == PLAYMODE_EDIT && numRows >= 1)
		setSongPos(-1, (row + editor.editRowSkip) % numRows, RESET_SONG_TICK);

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
	undoPatternCommit();
}

void insertPatternNote(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	bool editmode = ui.patternEditorShown && (playMode == PLAYMODE_EDIT);
	if (!editmode && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	note_t *p = pattern[curPattern];
	if (p == NULL)
		return;
	undoPatternBegin(curPattern, "Insert note");

	const int16_t numRows = patternNumRows[curPattern];

	if (numRows > 1)
	{
		for (int32_t i = numRows-2; i >= row; i--)
			p[((i+1) * MAX_CHANNELS) + cursor.ch] = p[(i * MAX_CHANNELS) + cursor.ch];
	}

	memset(&p[(row * MAX_CHANNELS) + cursor.ch], 0, sizeof (note_t));

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
	undoPatternCommit();
}

void insertPatternLine(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	bool editmode = ui.patternEditorShown && (playMode == PLAYMODE_EDIT);
	if (!editmode && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;
	undoPatternBegin(curPattern, "Insert line");

	setPatternLen(curPattern, patternNumRows[curPattern] + config.recTrueInsert); // config.recTrueInsert is 0 or 1

	note_t *p = pattern[curPattern];
	if (p != NULL)
	{
		const int16_t numRows = patternNumRows[curPattern];

		if (numRows > 1)
		{
			for (int32_t i = numRows-2; i >= row; i--)
			{
				for (int32_t j = 0; j < MAX_CHANNELS; j++)
					p[((i+1) * MAX_CHANNELS) + j] = p[(i * MAX_CHANNELS) + j];
			}
		}

		memset(&p[row * MAX_CHANNELS], 0, TRACK_WIDTH);

		killPatternIfUnused(curPattern);
	}

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
	undoPatternCommit();
}

void clearPreviousPatternEntry(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	bool editmode = ui.patternEditorShown && (playMode == PLAYMODE_EDIT);
	if (!editmode && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;

	if (row <= 0)
		return;

	row--;
	editor.row = song.row = row;

	note_t *p = pattern[curPattern];
	if (p != NULL)
	{
		undoPatternBegin(curPattern, "Clear previous entry");
		memset(&p[(row * MAX_CHANNELS) + cursor.ch], 0, sizeof (note_t));
		killPatternIfUnused(curPattern);
		setSongModifiedFlag();
		undoPatternCommit();
	}

	ui.updatePatternEditor = true;
}

void deletePatternNote(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	bool editmode = ui.patternEditorShown && (playMode == PLAYMODE_EDIT);
	if (!editmode && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;
	undoPatternBegin(curPattern, "Delete note");

	const int16_t numRows = patternNumRows[curPattern];

	note_t *p = pattern[curPattern];
	if (p != NULL)
	{
		if (row > 0)
		{
			row--;
			editor.row = song.row = row;

			for (int32_t i = row; i < numRows-1; i++)
				p[(i * MAX_CHANNELS) + cursor.ch] = p[((i+1) * MAX_CHANNELS) + cursor.ch];

			memset(&p[((numRows-1) * MAX_CHANNELS) + cursor.ch], 0, sizeof (note_t));
		}
	}
	else
	{
		if (row > 0)
		{
			row--;
			editor.row = song.row = row;
		}
	}

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
	undoPatternCommit();
}

void deletePatternLine(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	int16_t row = editor.row;
	resumeMusic();

	bool editmode = ui.patternEditorShown && (playMode == PLAYMODE_EDIT);
	if (!editmode && playMode != PLAYMODE_RECPATT && playMode != PLAYMODE_RECSONG)
		return;
	undoPatternBegin(curPattern, "Delete line");

	const int16_t numRows = patternNumRows[curPattern];
	note_t *p = pattern[curPattern];
	if (p != NULL)
	{
		if (row > 0)
		{
			row--;
			editor.row = song.row = row;

			for (int32_t i = row; i < numRows-1; i++)
			{
				for (int32_t j = 0; j < MAX_CHANNELS; j++)
					p[(i * MAX_CHANNELS) + j] = p[((i+1) * MAX_CHANNELS) + j];
			}

			memset(&p[(numRows-1) * MAX_CHANNELS], 0, TRACK_WIDTH);
		}
	}
	else
	{
		if (row > 0)
		{
			row--;
			editor.row = song.row = row;
		}
	}

	if (config.recTrueInsert && numRows > 1)
		setPatternLen(curPattern, numRows-1);

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
	undoPatternCommit();
}

// ----- TRANSPOSE FUNCTIONS -----

static uint32_t countOverflowingNotes(uint8_t mode, int8_t addValue, bool allInstrumentsFlag,
	uint16_t curPattern, int32_t numRows, int32_t markX1, int32_t markX2, int32_t markY1, int32_t markY2)
{
	uint32_t notesToDelete = 0;

	// "addValue" is never <-12 or >12, so unsigned 8-bit testing for >96 is safe
	switch (mode)
	{
		case TRANSP_TRACK:
		{
			note_t *p = pattern[curPattern];
			if (p == NULL)
				return 0; // empty pattern

			p += cursor.ch;

			for (int32_t row = 0; row < numRows; row++, p += MAX_CHANNELS)
			{
				if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
				{
					if ((int8_t)p->note+addValue > 96 || (int8_t)p->note+addValue <= 0)
						notesToDelete++;
				}
			}
		}
		break;

		case TRANSP_PATT:
		{
			note_t *p = pattern[curPattern];
			if (p == NULL)
				return 0; // empty pattern

			const int32_t pitch = MAX_CHANNELS-song.numChannels;
			for (int32_t row = 0; row < numRows; row++, p += pitch)
			{
				for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
				{
					if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
					{
						if ((int8_t)p->note+addValue > 96 || (int8_t)p->note+addValue <= 0)
							notesToDelete++;
					}
				}
			}
		}
		break;

		case TRANSP_SONG:
		{
			const int32_t pitch = MAX_CHANNELS - song.numChannels;
			for (int32_t i = 0; i < MAX_PATTERNS; i++)
			{
				note_t *p = pattern[i];
				if (p == NULL)
					continue; // empty pattern, skip it

				for (int32_t row = 0; row < patternNumRows[i]; row++, p += pitch)
				{
					for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
					{
						if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
						{
							if ((int8_t)p->note+addValue > 96 || (int8_t)p->note+addValue <= 0)
								notesToDelete++;
						}
					}
				}
			}
		}
		break;

		case TRANSP_BLOCK:
		{
			if (markY1 == markY2 || markY1 > markY2)
				return 0;

			if (markX1 > song.numChannels-1)
				markX1 = song.numChannels-1;

			if (markX2 > song.numChannels-1)
				markX2 = song.numChannels-1;

			if (markX2 < markX1)
				markX2 = markX1;

			if (markY1 >= numRows)
				markY1 = numRows-1;

			if (markY2 > numRows)
				markY2 = numRows-markY1;

			note_t *p = pattern[curPattern];
			if (p == NULL || markX1 < 0 || markY1 < 0 || markX2 < 0 || markY2 < 0)
				return 0;

			p += (markY1 * MAX_CHANNELS) + markX1;

			const int32_t pitch = MAX_CHANNELS - ((markX2 + 1) - markX1);
			for (int32_t row = markY1; row < markY2; row++, p += pitch)
			{
				for (int32_t ch = markX1; ch <= markX2; ch++, p++)
				{
					if ((p->note >= 1 && p->note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
					{
						if ((int8_t)p->note+addValue > 96 || (int8_t)p->note+addValue <= 0)
							notesToDelete++;
					}
				}
			}
		}
		break;

		default: break;
	}

	return notesToDelete;
}

bool transposeViewModeIsEnabled(void)
{
	return transposeViewMode;
}

void toggleTransposeViewMode(void)
{
	transposeViewMode ^= 1;
}

static void buildTransposeViewTargets(uint8_t mode, uint16_t curPattern,
	int32_t numRows, int32_t centerRow, int32_t markX1, int32_t markX2,
	int32_t markY1, int32_t markY2,
	uint16_t targetPatterns[MAX_CHANNELS],
	bool targets[MAX_CHANNELS][MAX_PATT_LEN])
{
	memset(targets, 0, sizeof (bool) * MAX_CHANNELS * MAX_PATT_LEN);
	for (int32_t channelIndex = 0; channelIndex < MAX_CHANNELS; channelIndex++)
		targetPatterns[channelIndex] = curPattern;

	if (curPattern >= MAX_PATTERNS || numRows <= 0)
		return;

	const pattCoord_t *pattCoord =
		&pattCoordTable[config.ptnStretch][ui.pattChanScrollShown][getPatternEditorView()];
	const int32_t screenRows = pattCoord->numUpperRows + 1 + pattCoord->numLowerRows;

	int32_t firstChannel;
	int32_t lastChannel;
	if (mode == TRANSP_TRACK)
	{
		firstChannel = cursor.ch;
		lastChannel = cursor.ch;
	}
	else
	{
		firstChannel = ui.channelOffset;
		lastChannel = MIN(song.numChannels, ui.channelOffset + ui.numChannelsShown) - 1;
	}

	if (mode == TRANSP_BLOCK)
	{
		if (markY1 >= markY2 || markX1 < 0 || markX2 < 0)
			return;

		firstChannel = MAX(firstChannel, markX1);
		lastChannel = MIN(lastChannel, markX2);
	}

	firstChannel = CLAMP(firstChannel, 0, song.numChannels - 1);
	lastChannel = CLAMP(lastChannel, 0, song.numChannels - 1);
	if (firstChannel > lastChannel)
		return;

	fastTracksSnapshot_t snapshot;
	fastTracksPOCGetSnapshot(&snapshot);

	for (int32_t screenIndex = 0; screenIndex < screenRows; screenIndex++)
	{
		const int32_t displayedRow =
			centerRow - pattCoord->numUpperRows + screenIndex;
		if (displayedRow < 0)
			continue;
		if (displayedRow >= numRows)
			break;

		if (mode == TRANSP_BLOCK &&
			(displayedRow < markY1 || displayedRow >= markY2))
		{
			continue;
		}

		for (int32_t channelIndex = firstChannel;
			channelIndex <= lastChannel; channelIndex++)
		{
			int32_t sourceRow = displayedRow;
			const fastTracksTrackSnapshot_t *track =
				&snapshot.tracks[channelIndex];
			if (track->enabled)
			{
				targetPatterns[channelIndex] = track->sourcePattern;
				const int32_t sourceNumRows =
					patternNumRows[targetPatterns[channelIndex]] > 0 ?
					patternNumRows[targetPatterns[channelIndex]] : 1;
				sourceRow = track->sourceRow +
					(screenIndex - pattCoord->numUpperRows);
				sourceRow %= sourceNumRows;
				if (sourceRow < 0)
					sourceRow += sourceNumRows;
			}

			/*
			** Short patterns can wrap more than once inside the viewport.
			** A rendered event is still one underlying note, so transpose it
			** once per click rather than once per repeated screen appearance.
			*/
			targets[channelIndex][sourceRow] = true;
		}
	}
}

static uint32_t countTransposeViewOverflow(int8_t addValue,
	bool allInstrumentsFlag,
	const uint16_t targetPatterns[MAX_CHANNELS],
	const bool targets[MAX_CHANNELS][MAX_PATT_LEN])
{
	uint32_t notesToDelete = 0;
	for (int32_t channelIndex = 0; channelIndex < song.numChannels; channelIndex++)
	{
		const uint16_t targetPattern = targetPatterns[channelIndex];
		const note_t *p = pattern[targetPattern];
		if (p == NULL)
			continue;

		for (int32_t row = 0; row < patternNumRows[targetPattern]; row++)
		{
			if (!targets[channelIndex][row])
				continue;

			const note_t *note =
				&p[(row * MAX_CHANNELS) + channelIndex];
			if ((note->note >= 1 && note->note <= 96) &&
				(allInstrumentsFlag || note->instr == editor.curInstr) &&
				((int8_t)note->note + addValue > 96 ||
				 (int8_t)note->note + addValue <= 0))
			{
				notesToDelete++;
			}
		}
	}

	return notesToDelete;
}

static void transposeViewTargets(int8_t addValue, bool allInstrumentsFlag,
	const uint16_t targetPatterns[MAX_CHANNELS],
	const bool targets[MAX_CHANNELS][MAX_PATT_LEN])
{
	for (int32_t channelIndex = 0; channelIndex < song.numChannels; channelIndex++)
	{
		const uint16_t targetPattern = targetPatterns[channelIndex];
		note_t *p = pattern[targetPattern];
		if (p == NULL)
			continue;

		for (int32_t row = 0; row < patternNumRows[targetPattern]; row++)
		{
			if (!targets[channelIndex][row])
				continue;

			note_t *note = &p[(row * MAX_CHANNELS) + channelIndex];
			if ((note->note >= 1 && note->note <= 96) &&
				(allInstrumentsFlag || note->instr == editor.curInstr))
			{
				uint8_t transposedNote = note->note + addValue;
				if (transposedNote > 96)
					transposedNote = 0;

				note->note = transposedNote;
			}
		}
	}
}

static void doTranspose(uint8_t mode, int8_t addValue, bool allInstrumentsFlag)
{
	uint16_t viewTargetPatterns[MAX_CHANNELS];
	bool viewTargets[MAX_CHANNELS][MAX_PATT_LEN];

	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	const int32_t numRows = patternNumRows[curPattern];
	const int32_t centerRow = editor.row;
	volatile int32_t markX1 = pattMark.markX1;
	volatile int32_t markX2 = pattMark.markX2;
	volatile int32_t markY1 = pattMark.markY1;
	volatile int32_t markY2 = pattMark.markY2;
	const bool useViewMode = transposeViewMode;
	if (useViewMode)
	{
		buildTransposeViewTargets(mode, curPattern, numRows, centerRow,
			markX1, markX2, markY1, markY2,
			viewTargetPatterns, viewTargets);
	}
	resumeMusic();

	if (useViewMode)
	{
		const uint32_t overflowingNotes = countTransposeViewOverflow(
			addValue, allInstrumentsFlag, viewTargetPatterns, viewTargets);
		if (overflowingNotes > 0)
		{
			char text[48];
			sprintf(text, "%u note(s) will be erased! Proceed?", overflowingNotes);
			if (okBox(2, "System request", text, NULL) != 1)
				return;
		}

		transposeViewTargets(addValue, allInstrumentsFlag,
			viewTargetPatterns, viewTargets);
		ui.updatePatternEditor = true;
		setSongModifiedFlag();
		return;
	}

	uint32_t overflowingNotes = countOverflowingNotes(mode, addValue, allInstrumentsFlag,
		curPattern, numRows, markX1, markX2, markY1, markY2);
	if (overflowingNotes > 0)
	{
		char text[48];
		sprintf(text, "%u note(s) will be erased! Proceed?", overflowingNotes);
		if (okBox(2, "System request", text, NULL) != 1)
			return;
	}

	// "addValue" is never <-12 or >12, so unsigned 8-bit testing for >96 is safe
	switch (mode)
	{
		case TRANSP_TRACK:
		{
			note_t *p = pattern[curPattern];
			if (p == NULL)
				return;

			p += cursor.ch;

			for (int32_t row = 0; row < numRows; row++, p += MAX_CHANNELS)
			{
				volatile uint8_t note = p->note;
				if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
				{
					note += addValue;
					if (note > 96)
						note = 0; // also handles underflow

					p->note = note;
				}
			}
		}
		break;

		case TRANSP_PATT:
		{
			note_t *p = pattern[curPattern];
			if (p == NULL)
				return;

			const int32_t pitch = MAX_CHANNELS - song.numChannels;
			for (int32_t row = 0; row < numRows; row++, p += pitch)
			{
				for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
				{
					volatile uint8_t note = p->note;
					if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
					{
						note += addValue;
						if (note > 96)
							note = 0; // also handles underflow

						p->note = note;
					}
				}
			}
		}
		break;

		case TRANSP_SONG:
		{
			const int32_t pitch = MAX_CHANNELS - song.numChannels;
			for (int32_t i = 0; i < MAX_PATTERNS; i++)
			{
				note_t *p = pattern[i];
				if (p == NULL)
					continue; // empty pattern, skip it

				for (int32_t row = 0; row < patternNumRows[i]; row++, p += pitch)
				{
					for (int32_t ch = 0; ch < song.numChannels; ch++, p++)
					{
						volatile uint8_t note = p->note;
						if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
						{
							note += addValue;
							if (note > 96)
								note = 0; // also handles underflow

							p->note = note;
						}
					}
				}
			}
		}
		break;

		case TRANSP_BLOCK:
		{
			if (markY1 == markY2 || markY1 > markY2)
				return;

			if (markX1 > song.numChannels-1)
				markX1 = song.numChannels-1;

			if (markX2 > song.numChannels-1)
				markX2 = song.numChannels-1;

			if (markX2 < markX1)
				markX2 = markX1;

			if (markY1 >= numRows)
				markY1 = numRows-1;

			if (markY2 > numRows)
				markY2 = numRows-markY1;

			note_t *p = pattern[curPattern];
			if (p == NULL || markX1 < 0 || markY1 < 0 || markX2 < 0 || markY2 < 0)
				return;

			p += (markY1 * MAX_CHANNELS) + markX1;

			const int32_t pitch = MAX_CHANNELS - ((markX2 + 1) - markX1);
			for (int32_t row = markY1; row < markY2; row++, p += pitch)
			{
				for (int32_t ch = markX1; ch <= markX2; ch++, p++)
				{
					volatile uint8_t note = p->note;
					if ((note >= 1 && note <= 96) && (allInstrumentsFlag || p->instr == editor.curInstr))
					{
						note += addValue;
						if (note > 96)
							note = 0; // also handles underflow

						p->note = note;
					}
				}
			}
		}
		break;

		default: break;
	}

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
}

void trackTranspCurInsUp(void)
{
	doTranspose(TRANSP_TRACK, 1, TRANSP_CUR_INSTRUMENT);
}

void trackTranspCurInsDn(void)
{
	doTranspose(TRANSP_TRACK, -1, TRANSP_CUR_INSTRUMENT);
}

void trackTranspCurIns12Up(void)
{
	doTranspose(TRANSP_TRACK, 12, TRANSP_CUR_INSTRUMENT);
}

void trackTranspCurIns12Dn(void)
{
	doTranspose(TRANSP_TRACK, -12, TRANSP_CUR_INSTRUMENT);
}

void trackTranspAllInsUp(void)
{
	doTranspose(TRANSP_TRACK, 1, TRANSP_ALL_INSTRUMENTS);
}

void trackTranspAllInsDn(void)
{
	doTranspose(TRANSP_TRACK, -1, TRANSP_ALL_INSTRUMENTS);
}

void trackTranspAllIns12Up(void)
{
	doTranspose(TRANSP_TRACK, 12, TRANSP_ALL_INSTRUMENTS);
}

void trackTranspAllIns12Dn(void)
{
	doTranspose(TRANSP_TRACK, -12, TRANSP_ALL_INSTRUMENTS);
}

void pattTranspCurInsUp(void)
{
	doTranspose(TRANSP_PATT, 1, TRANSP_CUR_INSTRUMENT);
}

void pattTranspCurInsDn(void)
{
	doTranspose(TRANSP_PATT, -1, TRANSP_CUR_INSTRUMENT);
}

void pattTranspCurIns12Up(void)
{
	doTranspose(TRANSP_PATT, 12, TRANSP_CUR_INSTRUMENT);
}

void pattTranspCurIns12Dn(void)
{
	doTranspose(TRANSP_PATT, -12, TRANSP_CUR_INSTRUMENT);
}

void pattTranspAllInsUp(void)
{
	doTranspose(TRANSP_PATT, 1, TRANSP_ALL_INSTRUMENTS);
}

void pattTranspAllInsDn(void)
{
	doTranspose(TRANSP_PATT, -1, TRANSP_ALL_INSTRUMENTS);
}

void pattTranspAllIns12Up(void)
{
	doTranspose(TRANSP_PATT, 12, TRANSP_ALL_INSTRUMENTS);
}

void pattTranspAllIns12Dn(void)
{
	doTranspose(TRANSP_PATT, -12, TRANSP_ALL_INSTRUMENTS);
}

void songTranspCurInsUp(void)
{
	doTranspose(TRANSP_SONG, 1, TRANSP_CUR_INSTRUMENT);
}

void songTranspCurInsDn(void)
{
	doTranspose(TRANSP_SONG, -1, TRANSP_CUR_INSTRUMENT);
}

void songTranspCurIns12Up(void)
{
	doTranspose(TRANSP_SONG, 12, TRANSP_CUR_INSTRUMENT);
}

void songTranspCurIns12Dn(void)
{
	doTranspose(TRANSP_SONG, -12, TRANSP_CUR_INSTRUMENT);
}

void songTranspAllInsUp(void)
{
	doTranspose(TRANSP_SONG, 1, TRANSP_ALL_INSTRUMENTS);
}

void songTranspAllInsDn(void)
{
	doTranspose(TRANSP_SONG, -1, TRANSP_ALL_INSTRUMENTS);
}

void songTranspAllIns12Up(void)
{
	doTranspose(TRANSP_SONG, 12, TRANSP_ALL_INSTRUMENTS);
}

void songTranspAllIns12Dn(void)
{
	doTranspose(TRANSP_SONG, -12, TRANSP_ALL_INSTRUMENTS);
}

void blockTranspCurInsUp(void)
{
	doTranspose(TRANSP_BLOCK, 1, TRANSP_CUR_INSTRUMENT);
}

void blockTranspCurInsDn(void)
{
	doTranspose(TRANSP_BLOCK, -1, TRANSP_CUR_INSTRUMENT);
}

void blockTranspCurIns12Up(void)
{
	doTranspose(TRANSP_BLOCK, 12, TRANSP_CUR_INSTRUMENT);
}

void blockTranspCurIns12Dn(void)
{
	doTranspose(TRANSP_BLOCK, -12, TRANSP_CUR_INSTRUMENT);
}

void blockTranspAllInsUp(void)
{
	doTranspose(TRANSP_BLOCK, 1, TRANSP_ALL_INSTRUMENTS);
}

void blockTranspAllInsDn(void)
{
	doTranspose(TRANSP_BLOCK, -1, TRANSP_ALL_INSTRUMENTS);
}

void blockTranspAllIns12Up(void)
{
	doTranspose(TRANSP_BLOCK, 12, TRANSP_ALL_INSTRUMENTS);
}

void blockTranspAllIns12Dn(void)
{
	doTranspose(TRANSP_BLOCK, -12, TRANSP_ALL_INSTRUMENTS);
}

static void copyNote(note_t *src, note_t *dst)
{
	if (editor.copyMaskEnable)
	{
		if (editor.copyMask[0])
			dst->note = src->note;

		if (editor.copyMask[1])
			dst->instr = src->instr;

		if (editor.copyMask[2])
			dst->vol = src->vol;

		if (editor.copyMask[3])
		{
			dst->tuneType = src->tuneType;
			dst->tuneData = src->tuneData;
			dst->efx = src->efx;
		}

		if (editor.copyMask[4])
			dst->efxData = src->efxData;
	}
	else
	{
		*dst = *src;
	}
}

static void pasteNote(note_t *src, note_t *dst)
{
	if (editor.copyMaskEnable)
	{
		if (editor.copyMask[0] && (src->note != 0 || !editor.transpMask[0]))
			dst->note = src->note;

		if (editor.copyMask[1] && (src->instr != 0 || !editor.transpMask[1]))
			dst->instr = src->instr;

		if (editor.copyMask[2] && (src->vol != 0 || !editor.transpMask[2]))
			dst->vol = src->vol;

		if (editor.copyMask[3] && (src->efx != 0 || !editor.transpMask[3]))
		{
			if (src->tuneType != 0 || !editor.transpMask[3])
			{
				dst->tuneType = src->tuneType;
				dst->tuneData = src->tuneData;
			}
			dst->efx = src->efx;
		}

		if (editor.copyMask[4] && (src->efxData != 0 || !editor.transpMask[4]))
			dst->efxData = src->efxData;
	}
	else
	{
		*dst = *src;
	}
}

void cutTrack(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	note_t *p = pattern[curPattern];
	if (p == NULL)
		return;
	undoPatternBegin(curPattern, "Cut track");

	const int16_t numRows = patternNumRows[curPattern];

	if (config.ptnCutToBuffer)
	{
		memset(trackCopyBuff, 0, sizeof (trackCopyBuff));

		for (int16_t i = 0; i < numRows; i++)
			copyNote(&p[(i * MAX_CHANNELS) + cursor.ch], &trackCopyBuff[i]);

		trkBufLen = numRows;
	}

	pauseMusic();
	for (int16_t i = 0; i < numRows; i++)
		memset(&p[(i * MAX_CHANNELS) + cursor.ch], 0, sizeof (note_t));
	resumeMusic();

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
	undoPatternCommit();
}

void copyTrack(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	note_t *p = pattern[curPattern];
	if (p != NULL)
	{
		memset(trackCopyBuff, 0, sizeof (trackCopyBuff));

		const int16_t numRows = patternNumRows[curPattern];
		for (int16_t i = 0; i < numRows; i++)
			copyNote(&p[(i * MAX_CHANNELS) + cursor.ch], &trackCopyBuff[i]);

		trkBufLen = numRows;
	}
}

void pasteTrack(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	if (trkBufLen == 0)
		return;
	undoPatternBegin(curPattern, "Paste track");
	if (!allocatePattern(curPattern))
	{
		undoCancelTransaction();
		return;
	}

	note_t *p = pattern[curPattern];
	const int16_t numRows = patternNumRows[curPattern];

	pauseMusic();
	for (int16_t i = 0; i < numRows; i++)
		pasteNote(&trackCopyBuff[i], &p[(i * MAX_CHANNELS) + cursor.ch]);
	resumeMusic();

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
	undoPatternCommit();
}

void cutPattern(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	note_t *p = pattern[curPattern];
	if (p == NULL)
		return;
	undoPatternBegin(curPattern, "Cut pattern");

	const int16_t numRows = patternNumRows[curPattern];

	if (config.ptnCutToBuffer)
	{
		memset(ptnCopyBuff, 0, (MAX_PATT_LEN * MAX_CHANNELS) * sizeof (note_t));

		for (int16_t x = 0; x < song.numChannels; x++)
		{
			for (int16_t i = 0; i < numRows; i++)
				copyNote(&p[(i * MAX_CHANNELS) + x], &ptnCopyBuff[(i * MAX_CHANNELS) + x]);
		}

		ptnBufLen = numRows;
	}

	pauseMusic();
	for (int16_t x = 0; x < song.numChannels; x++)
	{
		for (int16_t i = 0; i < numRows; i++)
			memset(&p[(i * MAX_CHANNELS) + x], 0, sizeof (note_t));
	}
	resumeMusic();

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
	undoPatternCommit();
}

void copyPattern(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	note_t *p = pattern[curPattern];
	if (p != NULL)
	{
		memset(ptnCopyBuff, 0, (MAX_PATT_LEN * MAX_CHANNELS) * sizeof (note_t));

		const int16_t numRows = patternNumRows[curPattern];
		for (int16_t x = 0; x < song.numChannels; x++)
		{
			for (int16_t i = 0; i < numRows; i++)
				copyNote(&p[(i * MAX_CHANNELS) + x], &ptnCopyBuff[(i * MAX_CHANNELS) + x]);
		}

		ptnBufLen = numRows;
	}
}

void pastePattern(void)
{
	const volatile uint16_t curPattern = editor.editPattern;

	if (ptnBufLen == 0)
		return;

	undoPatternBegin(curPattern, "Paste pattern");
	if (patternNumRows[curPattern] != ptnBufLen)
	{
		if (okBox(2, "System request", "Adjust pattern length to match copied pattern length?", NULL) == 1)
			setPatternLen(curPattern, ptnBufLen);
	}

	if (!allocatePattern(curPattern))
	{
		undoCancelTransaction();
		return;
	}

	note_t *p = pattern[curPattern];
	const int16_t numRows = patternNumRows[curPattern];
	
	pauseMusic();
	for (int16_t x = 0; x < song.numChannels; x++)
	{
		for (int16_t i = 0; i < numRows; i++)
			pasteNote(&ptnCopyBuff[(i * MAX_CHANNELS) + x], &p[(i * MAX_CHANNELS) + x]);
	}
	resumeMusic();

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
	undoPatternCommit();
}

void cutBlock(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	volatile int32_t markX1 = pattMark.markX1;
	volatile int32_t markX2 = pattMark.markX2;
	volatile int32_t markY1 = pattMark.markY1;
	volatile int32_t markY2 = pattMark.markY2;
	resumeMusic();

	const int16_t numRows = patternNumRows[curPattern];

	if (markY1 == markY2 || markY1 > markY2)
		return;

	if (markX1 > song.numChannels-1)
		markX1 = song.numChannels-1;

	if (markX2 > song.numChannels-1)
		markX2 = song.numChannels-1;

	if (markX2 < markX1)
		markX2 = markX1;

	if (markY1 >= numRows)
		markY1 = numRows-1;

	if (markY2 > numRows)
		markY2 = numRows-markY1;

	note_t *p = pattern[curPattern];
	if (p != NULL && markY1 >= 0 && markX1 >= 0 && markX2 >= 0 && markY2 >= 0)
	{
		undoPatternBegin(curPattern, "Cut block");
		pauseMusic();
		for (int32_t x = markX1; x <= markX2; x++)
		{
			for (int32_t y = markY1; y < markY2; y++)
			{
				note_t *n = &p[(y * MAX_CHANNELS) + x];

				if (config.ptnCutToBuffer)
					copyNote(n, &blkCopyBuff[((y - markY1) * MAX_CHANNELS) + (x - markX1)]);

				memset(n, 0, sizeof (note_t));
			}
		}
		resumeMusic();

		killPatternIfUnused(curPattern);

		if (config.ptnCutToBuffer)
		{
			markXSize = markX2 - markX1;
			markYSize = markY2 - markY1;
			blockCopied = true;
		}

		ui.updatePatternEditor = true;
		setSongModifiedFlag();
		undoPatternCommit();
	}
}

void copyBlock(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	volatile int32_t markX1 = pattMark.markX1;
	volatile int32_t markX2 = pattMark.markX2;
	volatile int32_t markY1 = pattMark.markY1;
	volatile int32_t markY2 = pattMark.markY2;
	resumeMusic();

	const int16_t numRows = patternNumRows[curPattern];

	if (markY1 == markY2 || markY1 > markY2)
		return;

	if (markX1 > song.numChannels-1)
		markX1 = song.numChannels-1;

	if (markX2 > song.numChannels-1)
		markX2 = song.numChannels-1;

	if (markX2 < markX1)
		markX2 = markX1;

	if (markY1 >= numRows)
		markY1 = numRows-1;

	if (markY2 > numRows)
		markY2 = numRows-markY1;

	note_t *p = pattern[curPattern];
	if (p != NULL && markY1 >= 0 && markX1 >= 0 && markX2 >= 0 && markY2 >= 0)
	{
		for (int32_t x = markX1; x <= markX2; x++)
		{
			for (int32_t y = markY1; y < markY2; y++)
				copyNote(&p[(y * MAX_CHANNELS) + x], &blkCopyBuff[((y - markY1) * MAX_CHANNELS) + (x - markX1)]);
		}

		markXSize = markX2 - markX1;
		markYSize = markY2 - markY1;
		blockCopied = true;
	}
}

static bool patternCellHasMaterial(const note_t *cell)
{
	return cell->note != 0 || cell->instr != 0 || cell->vol != 0 ||
		cell->efx != 0 || cell->efxData != 0 || cell->tuneType != 0 ||
		cell->tuneData != 0;
}

bool extractBlockToPattern(void)
{
	const uint16_t sourcePattern = editor.editPattern;
	int32_t x1 = pattMark.markX1;
	int32_t x2 = pattMark.markX2;
	int32_t y1 = pattMark.markY1;
	int32_t y2 = pattMark.markY2;
	const int32_t sourceRows = patternNumRows[sourcePattern];

	/* Match block-copy's inclusive channel/exclusive row convention, while
	** rejecting bounds that cannot describe any source cell. */
	if (pattern[sourcePattern] == NULL || song.numChannels <= 0 || sourceRows <= 0 ||
		x1 < 0 || x2 < 0 || y1 < 0 || y2 <= y1 || x1 > x2 ||
		x1 >= song.numChannels || y1 >= sourceRows)
	{
		okBox(0, "System message", "No valid pattern block is selected.", NULL);
		return false;
	}

	x2 = MIN(x2, song.numChannels - 1);
	y2 = MIN(y2, sourceRows);
	if (x2 < x1 || y2 <= y1)
	{
		okBox(0, "System message", "No valid pattern block is selected.", NULL);
		return false;
	}

	bool hasMaterial = false;
	for (int32_t y = y1; y < y2 && !hasMaterial; y++)
	{
		for (int32_t x = x1; x <= x2; x++)
		{
			if (patternCellHasMaterial(&pattern[sourcePattern][y * MAX_CHANNELS + x]))
			{
				hasMaterial = true;
				break;
			}
		}
	}

	if (!hasMaterial)
	{
		okBox(0, "System message", "The selected pattern block is empty.", NULL);
		return false;
	}

	const int16_t destination = findUnusedPattern();
	if (destination < 0)
	{
		okBox(0, "System message", "No unused pattern slot is available.", NULL);
		return false;
	}

	const int16_t oldRows = patternNumRows[destination];
	const int16_t extractedRows = (int16_t)(y2 - y1);
	if (!undoTransactionBegin("Extract block to pattern") ||
		!undoTransactionAddPattern((uint16_t)destination))
	{
		undoCancelTransaction();
		okBox(0, "System message", "Not enough memory to create undo data.", NULL);
		return false;
	}

	patternNumRows[destination] = extractedRows;
	const int16_t liveRows = song.currNumRows;
	if (!allocatePattern((uint16_t)destination))
	{
		patternNumRows[destination] = oldRows;
		song.currNumRows = liveRows;
		undoCancelTransaction();
		okBox(0, "System message", "Not enough memory to extract the block.", NULL);
		return false;
	}
	song.currNumRows = liveRows;

	pauseMusic();
	for (int32_t y = y1; y < y2; y++)
	{
		for (int32_t x = x1; x <= x2; x++)
		{
			pattern[destination][(y - y1) * MAX_CHANNELS + x] =
				pattern[sourcePattern][y * MAX_CHANNELS + x];
		}
	}
	resumeMusic();

	patternLauncherNotifyPatternChanged((uint16_t)destination);
	setSongModifiedFlag();
	undoTransactionCommit();
	return true;
}

void pasteBlock(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	const volatile uint16_t curRow = editor.row;
	resumeMusic();

	if (!blockCopied)
		return;

	undoPatternBegin(curPattern, "Paste block");
	if (!allocatePattern(curPattern))
	{
		undoCancelTransaction();
		return;
	}

	int32_t chStart = cursor.ch;
	int32_t rowStart = curRow;
	const int16_t numRows = patternNumRows[curPattern];

	if (chStart >= song.numChannels)
		chStart = song.numChannels-1;

	if (rowStart >= numRows)
		rowStart = numRows-1;

	int32_t markedChannels = markXSize + 1;
	if (chStart+markedChannels > song.numChannels)
		markedChannels = song.numChannels - chStart;

	int32_t markedRows = markYSize;
	if (rowStart+markedRows > numRows)
		markedRows = numRows - rowStart;

	if (markedChannels > 0 && markedRows > 0)
	{
		note_t *p = pattern[curPattern];

		pauseMusic();
		for (int32_t x = chStart; x < chStart+markedChannels; x++)
		{
			for (int32_t y = rowStart; y < rowStart+markedRows; y++)
				pasteNote(&blkCopyBuff[((y - rowStart) * MAX_CHANNELS) + (x - chStart)], &p[(y * MAX_CHANNELS) + x]);
		}
		resumeMusic();
	}

	killPatternIfUnused(curPattern);

	ui.updatePatternEditor = true;
	setSongModifiedFlag();
	undoPatternCommit();
}

typedef enum instrumentTransformScope_t
{
	INST_TRANSFORM_TRACK = 0,
	INST_TRANSFORM_PATTERN,
	INST_TRANSFORM_SONG,
	INST_TRANSFORM_BLOCK
} instrumentTransformScope_t;

typedef enum instrumentTransformMode_t
{
	INST_TRANSFORM_ALL = 0,
	INST_TRANSFORM_RANGE
} instrumentTransformMode_t;

typedef enum instrumentTransformMapping_t
{
	INST_TRANSFORM_NORMAL = 0,
	INST_TRANSFORM_REVERSE,
	INST_TRANSFORM_RANDOM
} instrumentTransformMapping_t;

typedef enum instrumentTransformField_t
{
	INST_TRANSFORM_FIELD_NONE = 0,
	INST_TRANSFORM_FIELD_OLD_LO,
	INST_TRANSFORM_FIELD_OLD_HI,
	INST_TRANSFORM_FIELD_NEW_LO,
	INST_TRANSFORM_FIELD_NEW_HI
} instrumentTransformField_t;

typedef struct instrumentTransformSnapshot_t
{
	bool exists;
	int16_t rows;
	note_t *data;
} instrumentTransformSnapshot_t;

#define INST_TRANSFORM_PANEL_X 24
#define INST_TRANSFORM_PANEL_Y 270
#define INST_TRANSFORM_PANEL_W 584
#define INST_TRANSFORM_PANEL_H 112

static bool instrumentTransformActive;
static instrumentTransformScope_t instrumentTransformScope;
static instrumentTransformMode_t instrumentTransformMode;
static instrumentTransformMapping_t instrumentTransformMapping;
static instrumentTransformField_t instrumentTransformField;
static uint8_t instrumentTransformOldLo, instrumentTransformOldHi;
static uint8_t instrumentTransformNewLo, instrumentTransformNewHi;
static uint8_t instrumentTransformMap[MAX_INST+1];
static uint8_t instrumentTransformLastGuiInstr;
static uint32_t instrumentTransformShuffleSeed;
static uint32_t instrumentTransformShuffleNumber;
static uint16_t instrumentTransformPattern;
static int32_t instrumentTransformX1, instrumentTransformX2, instrumentTransformY1, instrumentTransformY2;
static instrumentTransformSnapshot_t instrumentTransformSnapshots[MAX_PATTERNS];

static const char *instrumentTransformScopeName(void)
{
	static const char *names[] = { "TRACK", "PATTERN", "SONG", "BLOCK" };
	return names[instrumentTransformScope];
}

static const char *instrumentTransformFieldName(void)
{
	static const char *names[] = { "NONE", "OLD START", "OLD END", "NEW START", "NEW END" };
	return names[instrumentTransformField];
}

static uint32_t instrumentTransformRand(void)
{
	instrumentTransformShuffleSeed = (instrumentTransformShuffleSeed * 1664525u) + 1013904223u;
	return instrumentTransformShuffleSeed;
}

static void buildInstrumentTransformMap(void)
{
	for (int32_t i = 0; i <= MAX_INST; i++) instrumentTransformMap[i] = (uint8_t)i;

	if (instrumentTransformMode == INST_TRANSFORM_ALL)
	{
		for (int32_t i = 1; i <= MAX_INST; i++) instrumentTransformMap[i] = instrumentTransformNewLo;
		return;
	}

	const int32_t oldCount = instrumentTransformOldHi - instrumentTransformOldLo + 1;
	const int32_t newCount = instrumentTransformNewHi - instrumentTransformNewLo + 1;
	uint8_t values[MAX_INST];
	for (int32_t i = 0; i < oldCount; i++)
	{
		int32_t n = i % newCount;
		if (instrumentTransformMapping == INST_TRANSFORM_REVERSE) n = newCount - 1 - n;
		values[i] = (uint8_t)(instrumentTransformNewLo + n);
	}

	if (instrumentTransformMapping == INST_TRANSFORM_RANDOM)
	{
		for (int32_t i = oldCount-1; i > 0; i--)
		{
			const int32_t j = instrumentTransformRand() % (i+1);
			const uint8_t t = values[i]; values[i] = values[j]; values[j] = t;
		}
	}

	for (int32_t i = 0; i < oldCount; i++)
		instrumentTransformMap[instrumentTransformOldLo+i] = values[i];
}

static uint8_t transformInstrumentNumber(uint8_t instrumentNumber)
{
	if (instrumentNumber == 0) return 0;
	return instrumentTransformMap[instrumentNumber];
}

static void remapInstrXY(int32_t pattNum, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t src, uint8_t dst)
{
	note_t *pattPtr = pattern[pattNum];
	if (pattPtr == NULL) return;
	if (x1 > song.numChannels-1) x1 = song.numChannels-1;
	if (x2 > song.numChannels-1) x2 = song.numChannels-1;
	if (x2 < x1) x2 = x1;
	const int16_t numRows = patternNumRows[pattNum];
	if (y1 >= numRows) y1 = numRows-1;
	if (y2 >= numRows) y2 = numRows-1;
	if (y2 < y1) return;
	note_t *p = &pattPtr[(y1 * MAX_CHANNELS) + x1];
	const int32_t pitch = MAX_CHANNELS - ((x2 + 1) - x1);
	for (int32_t y = y1; y <= y2; y++, p += pitch)
		for (int32_t x = x1; x <= x2; x++, p++) if (p->instr == src) p->instr = dst;
}

static void transformInstrXY(int32_t pattNum, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
	note_t *pattPtr = pattern[pattNum];
	if (pattPtr == NULL) return;
	if (x1 > song.numChannels-1) x1 = song.numChannels-1;
	if (x2 > song.numChannels-1) x2 = song.numChannels-1;
	if (x2 < x1) x2 = x1;
	const int16_t numRows = patternNumRows[pattNum];
	if (numRows <= 0) return;
	if (y1 >= numRows) y1 = numRows-1;
	if (y2 >= numRows) y2 = numRows-1;
	if (y2 < y1) return;
	note_t *p = &pattPtr[(y1 * MAX_CHANNELS) + x1];
	const int32_t pitch = MAX_CHANNELS - ((x2 + 1) - x1);
	for (int32_t y = y1; y <= y2; y++, p += pitch)
		for (int32_t x = x1; x <= x2; x++, p++) p->instr = transformInstrumentNumber(p->instr);
}

static void freeInstrumentTransformSnapshots(void)
{
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		free(instrumentTransformSnapshots[i].data);
		memset(&instrumentTransformSnapshots[i], 0, sizeof (instrumentTransformSnapshots[i]));
	}
}

static bool captureInstrumentTransformSnapshots(void)
{
	freeInstrumentTransformSnapshots();
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		instrumentTransformSnapshot_t *snap = &instrumentTransformSnapshots[i];
		snap->rows = patternNumRows[i];
		if (pattern[i] == NULL) continue;
		snap->exists = true;
		const uint32_t bytes = (uint32_t)snap->rows * TRACK_WIDTH;
		snap->data = (note_t *)malloc(bytes);
		if (snap->data == NULL) { freeInstrumentTransformSnapshots(); return false; }
		memcpy(snap->data, pattern[i], bytes);
	}
	return true;
}

static void restoreInstrumentTransformSnapshots(void)
{
	pauseMusic();
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
	{
		instrumentTransformSnapshot_t *snap = &instrumentTransformSnapshots[i];
		if (!snap->exists)
		{
			if (pattern[i] != NULL) { memset(pattern[i], 0, (uint32_t)patternNumRows[i] * TRACK_WIDTH); killPatternIfUnused((uint16_t)i); }
			continue;
		}
		setPatternLen((uint16_t)i, snap->rows);
		if (allocatePattern((uint16_t)i)) memcpy(pattern[i], snap->data, (uint32_t)snap->rows * TRACK_WIDTH);
	}
	resumeMusic();
	ui.updatePatternEditor = true;
}

static void updateInstrumentTransformBounds(void)
{
	instrumentTransformPattern = editor.editPattern;
	instrumentTransformX1 = 0; instrumentTransformX2 = song.numChannels-1;
	instrumentTransformY1 = 0; instrumentTransformY2 = patternNumRows[instrumentTransformPattern]-1;
	if (instrumentTransformScope == INST_TRANSFORM_TRACK) instrumentTransformX1 = instrumentTransformX2 = cursor.ch;
	else if (instrumentTransformScope == INST_TRANSFORM_BLOCK)
	{
		instrumentTransformX1 = pattMark.markX1; instrumentTransformX2 = pattMark.markX2;
		instrumentTransformY1 = pattMark.markY1; instrumentTransformY2 = pattMark.markY2-1;
	}
}

static void renderInstrumentTransformPreview(void)
{
	restoreInstrumentTransformSnapshots();
	buildInstrumentTransformMap();
	updateInstrumentTransformBounds();
	pauseMusic();
	if (instrumentTransformScope == INST_TRANSFORM_SONG)
	{
		for (int32_t i = 0; i < MAX_PATTERNS; i++) transformInstrXY(i, 0, 0, song.numChannels-1, patternNumRows[i]-1);
	}
	else if (instrumentTransformScope != INST_TRANSFORM_BLOCK || instrumentTransformY1 <= instrumentTransformY2)
	{
		transformInstrXY(instrumentTransformPattern, instrumentTransformX1, instrumentTransformY1, instrumentTransformX2, instrumentTransformY2);
	}
	resumeMusic();
	ui.updatePatternEditor = true;
}

static void closeInstrumentTransform(bool apply)
{
	if (apply)
	{
		if (instrumentTransformScope == INST_TRANSFORM_SONG)
		{
			restoreInstrumentTransformSnapshots();
			undoSongBegin("Instrument transform");
			renderInstrumentTransformPreview();
			setSongModifiedFlag();
			undoSongCommit();
		}
		else
		{
			const uint16_t ptn = instrumentTransformPattern;
			const uint32_t bytes = (uint32_t)patternNumRows[ptn] * TRACK_WIDTH;
			note_t *accepted = pattern[ptn] == NULL ? NULL : (note_t *)malloc(bytes);
			if (accepted != NULL) memcpy(accepted, pattern[ptn], bytes);
			restoreInstrumentTransformSnapshots();
			undoPatternBegin(ptn, "Instrument transform");
			if (accepted != NULL && allocatePattern(ptn)) memcpy(pattern[ptn], accepted, bytes);
			free(accepted);
			setSongModifiedFlag();
			undoPatternCommit();
		}
	}
	else restoreInstrumentTransformSnapshots();
	instrumentTransformActive = false;
	freeInstrumentTransformSnapshots();
	ui.updatePatternEditor = true;
}

void openInstrumentTransformEditor(void)
{
	if (instrumentTransformActive || editor.curInstr == 0) return;
	instrumentTransformScope = INST_TRANSFORM_SONG;
	instrumentTransformMode = INST_TRANSFORM_ALL;
	instrumentTransformMapping = INST_TRANSFORM_NORMAL;
	instrumentTransformField = INST_TRANSFORM_FIELD_NEW_LO;
	instrumentTransformOldLo = editor.srcInstr > 0 ? editor.srcInstr : 1;
	instrumentTransformOldHi = instrumentTransformOldLo;
	instrumentTransformNewLo = instrumentTransformNewHi = editor.curInstr;
	instrumentTransformLastGuiInstr = editor.curInstr;
	instrumentTransformShuffleSeed = (uint32_t)SDL_GetTicks() ^ ((uint32_t)editor.editPattern << 16) ^ editor.curInstr;
	instrumentTransformShuffleNumber = 0;
	if (!captureInstrumentTransformSnapshots()) { okBox(0, "System message", "Not enough memory for Instrument Transform preview.", NULL); return; }
	instrumentTransformActive = true;
	renderInstrumentTransformPreview();
}

static void drawInstrumentTransformButton(int32_t x, int32_t y, int32_t w, const char *text, bool selected)
{
	drawFramework(x, y, w, 15, selected ? FRAMEWORK_TYPE2 : FRAMEWORK_TYPE1);
	textOut(x+4, y+4, PAL_FORGRND, text);
}

static void drawInstrumentTransformField(int32_t x, int32_t y, uint8_t value, instrumentTransformField_t field)
{
	char str[4]; snprintf(str, sizeof (str), "%02X", value);
	drawFramework(x, y, 30, 15, instrumentTransformField == field ? FRAMEWORK_TYPE2 : FRAMEWORK_TYPE1);
	textOut(x+8, y+4, PAL_FORGRND, str);
}

void instrumentTransformDrawPanel(void)
{
	if (ui.patternEditorOnly || !instrumentTransformActive || ui.sysReqShown) return;
	if (instrumentTransformField != INST_TRANSFORM_FIELD_NONE && editor.curInstr != instrumentTransformLastGuiInstr && editor.curInstr > 0)
	{
		instrumentTransformLastGuiInstr = editor.curInstr;
		switch (instrumentTransformField)
		{
			case INST_TRANSFORM_FIELD_OLD_LO: instrumentTransformOldLo = editor.curInstr; if (instrumentTransformOldLo > instrumentTransformOldHi) instrumentTransformOldHi = instrumentTransformOldLo; break;
			case INST_TRANSFORM_FIELD_OLD_HI: instrumentTransformOldHi = editor.curInstr; if (instrumentTransformOldHi < instrumentTransformOldLo) instrumentTransformOldLo = instrumentTransformOldHi; break;
			case INST_TRANSFORM_FIELD_NEW_LO: instrumentTransformNewLo = editor.curInstr; if (instrumentTransformNewLo > instrumentTransformNewHi) instrumentTransformNewHi = instrumentTransformNewLo; break;
			case INST_TRANSFORM_FIELD_NEW_HI: instrumentTransformNewHi = editor.curInstr; if (instrumentTransformNewHi < instrumentTransformNewLo) instrumentTransformNewLo = instrumentTransformNewHi; break;
			default: break;
		}
		renderInstrumentTransformPreview();
	}

	char str[160];
	drawFramework(INST_TRANSFORM_PANEL_X, INST_TRANSFORM_PANEL_Y, INST_TRANSFORM_PANEL_W, INST_TRANSFORM_PANEL_H, FRAMEWORK_TYPE1);
	fillRect(INST_TRANSFORM_PANEL_X+3, INST_TRANSFORM_PANEL_Y+3, INST_TRANSFORM_PANEL_W-6, INST_TRANSFORM_PANEL_H-6, PAL_DESKTOP);
	textOut(INST_TRANSFORM_PANEL_X+8, INST_TRANSFORM_PANEL_Y+7, PAL_FORGRND, "INSTRUMENT TRANSFORM");

	int32_t x=INST_TRANSFORM_PANEL_X+8, y=INST_TRANSFORM_PANEL_Y+20;
	drawInstrumentTransformButton(x,y,52,"TRACK",instrumentTransformScope==INST_TRANSFORM_TRACK); x+=55;
	drawInstrumentTransformButton(x,y,62,"PATTERN",instrumentTransformScope==INST_TRANSFORM_PATTERN); x+=65;
	drawInstrumentTransformButton(x,y,48,"SONG",instrumentTransformScope==INST_TRANSFORM_SONG); x+=51;
	drawInstrumentTransformButton(x,y,52,"BLOCK",instrumentTransformScope==INST_TRANSFORM_BLOCK);

	x=INST_TRANSFORM_PANEL_X+8; y=INST_TRANSFORM_PANEL_Y+39;
	drawInstrumentTransformButton(x,y,42,"ALL",instrumentTransformMode==INST_TRANSFORM_ALL); x+=45;
	drawInstrumentTransformButton(x,y,55,"RANGE",instrumentTransformMode==INST_TRANSFORM_RANGE);
	textOut(INST_TRANSFORM_PANEL_X+124,y+4,PAL_FORGRND,"OLD");
	drawInstrumentTransformField(INST_TRANSFORM_PANEL_X+153,y,instrumentTransformOldLo,INST_TRANSFORM_FIELD_OLD_LO);
	textOut(INST_TRANSFORM_PANEL_X+186,y+4,PAL_FORGRND,"-");
	drawInstrumentTransformField(INST_TRANSFORM_PANEL_X+196,y,instrumentTransformOldHi,INST_TRANSFORM_FIELD_OLD_HI);
	textOut(INST_TRANSFORM_PANEL_X+244,y+4,PAL_FORGRND,"NEW");
	drawInstrumentTransformField(INST_TRANSFORM_PANEL_X+279,y,instrumentTransformNewLo,INST_TRANSFORM_FIELD_NEW_LO);
	textOut(INST_TRANSFORM_PANEL_X+312,y+4,PAL_FORGRND,"-");
	drawInstrumentTransformField(INST_TRANSFORM_PANEL_X+322,y,instrumentTransformNewHi,INST_TRANSFORM_FIELD_NEW_HI);

	y=INST_TRANSFORM_PANEL_Y+58; x=INST_TRANSFORM_PANEL_X+8;
	drawInstrumentTransformButton(x,y,62,"NORMAL",instrumentTransformMapping==INST_TRANSFORM_NORMAL); x+=65;
	drawInstrumentTransformButton(x,y,66,"REVERSE",instrumentTransformMapping==INST_TRANSFORM_REVERSE); x+=69;
	drawInstrumentTransformButton(x,y,66,"RANDOM",instrumentTransformMapping==INST_TRANSFORM_RANDOM); x+=69;
	drawInstrumentTransformButton(x,y,92,"NEW SHUFFLE",false);
	drawInstrumentTransformButton(INST_TRANSFORM_PANEL_X+397,y,48,"APPLY",false);
	drawInstrumentTransformButton(INST_TRANSFORM_PANEL_X+448,y,55,"REVERT",false);
	drawInstrumentTransformButton(INST_TRANSFORM_PANEL_X+506,y,58,"CANCEL",false);

	y=INST_TRANSFORM_PANEL_Y+78;
	if (instrumentTransformMode == INST_TRANSFORM_ALL) snprintf(str,sizeof(str),"ALL -> %02X",instrumentTransformNewLo);
	else
	{
		char *p=str; size_t left=sizeof(str); int n=snprintf(p,left,"MAP "); p+=n; left-=n;
		for (int32_t i=instrumentTransformOldLo; i<=instrumentTransformOldHi && i<instrumentTransformOldLo+8; i++) { n=snprintf(p,left,"%02X>%02X ",i,instrumentTransformMap[i]); p+=n; left-=n; }
	}
	textOut(INST_TRANSFORM_PANEL_X+8,y,PAL_FORGRND,str);
	snprintf(str,sizeof(str),"SCOPE: %s   SELECTING: %s   SHUFFLE #%u",instrumentTransformScopeName(),instrumentTransformFieldName(),instrumentTransformShuffleNumber);
	textOut(INST_TRANSFORM_PANEL_X+8,INST_TRANSFORM_PANEL_Y+94,PAL_FORGRND,str);
}

bool instrumentTransformHandleMouseDown(int32_t mx, int32_t my, uint8_t mouseButton)
{
	if (ui.patternEditorOnly || !instrumentTransformActive || mouseButton != SDL_BUTTON_LEFT) return false;
	if (mx < INST_TRANSFORM_PANEL_X || mx >= INST_TRANSFORM_PANEL_X+INST_TRANSFORM_PANEL_W || my < INST_TRANSFORM_PANEL_Y || my >= INST_TRANSFORM_PANEL_Y+INST_TRANSFORM_PANEL_H) return false;
	const int32_t x=mx-INST_TRANSFORM_PANEL_X, y=my-INST_TRANSFORM_PANEL_Y;
	if (y>=20 && y<35)
	{
		if (x<63) instrumentTransformScope=INST_TRANSFORM_TRACK; else if (x<128) instrumentTransformScope=INST_TRANSFORM_PATTERN; else if (x<179) instrumentTransformScope=INST_TRANSFORM_SONG; else if (x<234) instrumentTransformScope=INST_TRANSFORM_BLOCK;
		renderInstrumentTransformPreview(); return true;
	}
	if (y>=39 && y<54)
	{
		if (x<53) instrumentTransformMode=INST_TRANSFORM_ALL;
		else if (x<108) instrumentTransformMode=INST_TRANSFORM_RANGE;
		else if (x>=153 && x<183) instrumentTransformField=INST_TRANSFORM_FIELD_OLD_LO;
		else if (x>=196 && x<226) instrumentTransformField=INST_TRANSFORM_FIELD_OLD_HI;
		else if (x>=279 && x<309) instrumentTransformField=INST_TRANSFORM_FIELD_NEW_LO;
		else if (x>=322 && x<352) instrumentTransformField=INST_TRANSFORM_FIELD_NEW_HI;
		instrumentTransformLastGuiInstr=editor.curInstr; renderInstrumentTransformPreview(); return true;
	}
	if (y>=58 && y<73)
	{
		if (x<73) instrumentTransformMapping=INST_TRANSFORM_NORMAL;
		else if (x<142) instrumentTransformMapping=INST_TRANSFORM_REVERSE;
		else if (x<211) { instrumentTransformMapping=INST_TRANSFORM_RANDOM; instrumentTransformShuffleNumber++; }
		else if (x<303) { instrumentTransformMapping=INST_TRANSFORM_RANDOM; instrumentTransformShuffleNumber++; instrumentTransformShuffleSeed ^= SDL_GetTicks()+instrumentTransformShuffleNumber; }
		else if (x>=397 && x<445) { closeInstrumentTransform(true); return true; }
		else if (x>=448 && x<503) { restoreInstrumentTransformSnapshots(); return true; }
		else if (x>=506) { closeInstrumentTransform(false); return true; }
		renderInstrumentTransformPreview(); return true;
	}
	return true;
}

bool instrumentTransformHandlePreviewKey(SDL_Scancode scancode, SDL_Keycode keycode, bool keyWasRepeated)
{
	if (!instrumentTransformActive) return false;
	if (!keyWasRepeated && (keycode == SDLK_RETURN || scancode == SDL_SCANCODE_KP_ENTER)) { closeInstrumentTransform(true); return true; }
	if (!keyWasRepeated && keycode == SDLK_ESCAPE) { closeInstrumentTransform(false); return true; }
	if (!keyWasRepeated && keycode == SDLK_r) { restoreInstrumentTransformSnapshots(); return true; }
	if (keycode == SDLK_SPACE || keycode == SDLK_LCTRL || keycode == SDLK_RCTRL || keycode == SDLK_LSHIFT || keycode == SDLK_RSHIFT || keycode == SDLK_LALT || keycode == SDLK_RALT) return false;
	return instrumentTransformField != INST_TRANSFORM_FIELD_NONE;
}

bool instrumentTransformPreviewActive(void) { return instrumentTransformActive; }

void remapBlock(void)
{
	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	volatile int32_t markX1 = pattMark.markX1, markX2 = pattMark.markX2;
	volatile int32_t markY1 = pattMark.markY1, markY2 = pattMark.markY2;
	resumeMusic();
	if (editor.srcInstr == editor.curInstr || markY1 == markY2 || markY1 > markY2) return;
	if (!undoPatternBegin(curPattern, "Remap block")) return;
	remapInstrXY(curPattern, markX1, markY1, markX2, markY2-1, editor.srcInstr, editor.curInstr);
	ui.updatePatternEditor = true; setSongModifiedFlag(); undoPatternCommit();
}

void remapTrack(void)
{
	const volatile uint16_t curPattern = editor.editPattern;
	if (editor.srcInstr == editor.curInstr) return;
	if (!undoPatternBegin(curPattern, "Remap track")) return;
	pauseMusic();
	remapInstrXY(curPattern, cursor.ch, 0, cursor.ch, patternNumRows[curPattern]-1, editor.srcInstr, editor.curInstr);
	resumeMusic(); ui.updatePatternEditor = true; setSongModifiedFlag(); undoPatternCommit();
}

void remapPattern(void)
{
	const volatile uint16_t curPattern = editor.editPattern;
	if (editor.srcInstr == editor.curInstr) return;
	if (!undoPatternBegin(curPattern, "Remap pattern")) return;
	pauseMusic();
	remapInstrXY(curPattern, 0, 0, song.numChannels-1, patternNumRows[curPattern]-1, editor.srcInstr, editor.curInstr);
	resumeMusic(); ui.updatePatternEditor = true; setSongModifiedFlag(); undoPatternCommit();
}

void remapSong(void)
{
	if (editor.srcInstr == editor.curInstr) return;
	if (!undoSongBegin("Remap song")) return;
	pauseMusic();
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
		remapInstrXY(i, 0, 0, song.numChannels-1, patternNumRows[i]-1, editor.srcInstr, editor.curInstr);
	resumeMusic(); ui.updatePatternEditor = true; setSongModifiedFlag(); undoSongCommit();
}

// "scale-fade volume" routines

static int8_t getNoteVolume(note_t *p)
{
	int8_t nv, vv, ev;

	if (p->vol >= 0x10 && p->vol <= 0x50)
		vv = p->vol - 0x10;
	else
		vv = -1;

	if (p->efx == 0xC)
		ev = MIN(p->efxData, 64);
	else
		ev = -1;

	if (p->instr != 0 && instr[p->instr] != NULL)
		nv = (int8_t)instr[p->instr]->smp[0].volume;
	else
		nv = -1;

	int8_t finalv = -1;
	if (nv >= 0) finalv = nv;
	if (vv >= 0) finalv = vv;
	if (ev >= 0) finalv = ev;

	return finalv;
}

static void setNoteVolume(note_t *p, int8_t newVol)
{
	if (newVol < 0)
		return;

	if (newVol > 64)
		newVol = 64;

	const int8_t oldv = getNoteVolume(p);
	if (p->vol == oldv)
		return; // volume is the same

	if (p->efx == 0x0C)
		p->efxData = newVol; // Cxx effect
	else
		p->vol = 0x10 + newVol; // volume column
}

static void scaleNote(int32_t pattNum, int32_t ch, int32_t row, double dScale)
{
	if (pattern[pattNum] == NULL)
		return;

	const int32_t numRows = patternNumRows[pattNum];
	if (row < 0 || row >= numRows || ch < 0 || ch >= song.numChannels)
		return;

	note_t *p = &pattern[pattNum][(row * MAX_CHANNELS) + ch];

	int32_t vol = getNoteVolume(p);
	if (vol >= 0)
	{
		vol = (int32_t)((vol * dScale) + 0.5); // rounded
		vol = MIN(MAX(0, vol), 64);
		setNoteVolume(p, (int8_t)vol);
	}
}

static bool askForScaleFade(char *msg)
{
	char volstr[32+1];

	sprintf(volstr, "%0.2f,%0.2f", dVolScaleFK1, dVolScaleFK2);
	if (inputBox(1, msg, volstr, sizeof (volstr)-1) != 1)
		return false;

	bool error = false;

	char *val1 = volstr;
	if (strlen(val1) < 3)
		error = true;

	char *val2 = strchr(volstr, ',');
	if (val2 == NULL || strlen(val2) < 3)
		error = true;

	if (error)
	{
		okBox(0, "System message", "Invalid constant expressions.", NULL);
		return false;
	}

	dVolScaleFK1 = atof(val1+0);
	dVolScaleFK2 = atof(val2+1);

	return true;
}

void scaleFadeVolumeTrack(void)
{
	if (!askForScaleFade("Volume scale-fade track (start-, end scale)"))
		return;

	const volatile uint16_t curPattern = editor.editPattern;

	if (pattern[curPattern] == NULL)
		return;

	const int32_t numRows = patternNumRows[curPattern];

	double dVolDelta = 0.0;
	if (numRows > 0)
		dVolDelta = (dVolScaleFK2 - dVolScaleFK1) / numRows;

	double dVol = dVolScaleFK1;

	pauseMusic();
	for (int32_t row = 0; row < numRows; row++)
	{
		scaleNote(curPattern, cursor.ch, row, dVol);
		dVol += dVolDelta;
	}
	resumeMusic();
}

void scaleFadeVolumePattern(void)
{
	if (!askForScaleFade("Volume scale-fade pattern (start-, end scale)"))
		return;

	const volatile uint16_t curPattern = editor.editPattern;

	if (pattern[curPattern] == NULL)
		return;

	const int32_t numRows = patternNumRows[curPattern];

	double dVolDelta = 0.0;
	if (numRows > 0)
		dVolDelta = (dVolScaleFK2 - dVolScaleFK1) / numRows;

	double dVol = dVolScaleFK1;

	pauseMusic();
	for (int32_t row = 0; row < numRows; row++)
	{
		for (int32_t ch = 0; ch < song.numChannels; ch++)
			scaleNote(curPattern, ch, row, dVol);

		dVol += dVolDelta;
	}
	resumeMusic();
}

void scaleFadeVolumeBlock(void)
{
	if (!askForScaleFade("Volume scale-fade block (start-, end scale)"))
		return;

	pauseMusic();
	const volatile uint16_t curPattern = editor.editPattern;
	volatile int32_t markX1 = pattMark.markX1;
	volatile int32_t markX2 = pattMark.markX2;
	volatile int32_t markY1 = pattMark.markY1;
	volatile int32_t markY2 = pattMark.markY2;
	resumeMusic();

	if (pattern[curPattern] == NULL || markY1 == markY2 || markY1 > markY2)
		return;

	const int32_t numRows = markY2 - markY1;

	double dVolDelta = 0.0;
	if (numRows > 0)
		dVolDelta = (dVolScaleFK2 - dVolScaleFK1) / numRows;

	double dVol = dVolScaleFK1;

	pauseMusic();
	for (int32_t row = markY1; row < markY2; row++)
	{
		for (int32_t ch = markX1; ch <= markX2; ch++)
			scaleNote(curPattern, ch, row, dVol);

		dVol += dVolDelta;
	}
	resumeMusic();
}

void toggleCopyMaskEnable(void) { editor.copyMaskEnable ^= 1; }
void toggleCopyMask0(void) { editor.copyMask[0] ^= 1; };
void toggleCopyMask1(void) { editor.copyMask[1] ^= 1; };
void toggleCopyMask2(void) { editor.copyMask[2] ^= 1; };
void toggleCopyMask3(void) { editor.copyMask[3] ^= 1; };
void toggleCopyMask4(void) { editor.copyMask[4] ^= 1; };
void togglePasteMask0(void) { editor.pasteMask[0] ^= 1; };
void togglePasteMask1(void) { editor.pasteMask[1] ^= 1; };
void togglePasteMask2(void) { editor.pasteMask[2] ^= 1; };
void togglePasteMask3(void) { editor.pasteMask[3] ^= 1; };
void togglePasteMask4(void) { editor.pasteMask[4] ^= 1; };
void toggleTranspMask0(void) { editor.transpMask[0] ^= 1; };
void toggleTranspMask1(void) { editor.transpMask[1] ^= 1; };
void toggleTranspMask2(void) { editor.transpMask[2] ^= 1; };
void toggleTranspMask3(void) { editor.transpMask[3] ^= 1; };
void toggleTranspMask4(void) { editor.transpMask[4] ^= 1; };
