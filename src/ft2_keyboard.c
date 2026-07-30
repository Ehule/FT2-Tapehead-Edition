// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ft2_header.h"
#include "ft2_keyboard.h"
#include "ft2_gui.h"
#include "ft2_about.h"
#include "ft2_video.h"
#include "ft2_edit.h"
#include "ft2_config.h"
#include "ft2_help.h"
#include "ft2_mouse.h"
#include "ft2_nibbles.h"
#include "ft2_inst_ed.h"
#include "ft2_pattern_ed.h"
#include "ft2_diskop.h"
#include "ft2_wav_renderer.h"
#include "ft2_sample_ed.h"
#include "ft2_audio.h"
#include "ft2_replayer.h"
#include "ft2_trim.h"
#include "ft2_sample_ed_features.h"
#include "ft2_midi.h"
#include "ft2_interpolation.h"
#include "ft2_undo.h"
#include "ft2_structs.h"
#include "ft2_pattern_draw.h"

keyb_t keyb; // globalized

static const uint8_t scancodeKey2Note[52] = // keys (USB usage page standard) to FT2 notes look-up table
{
	0x08, 0x05, 0x04, 0x11, 0x00, 0x07, 0x09, 0x19,
	0x0B, 0x00, 0x0E, 0x0C, 0x0A, 0x1B, 0x1D, 0x0D,
	0x12, 0x02, 0x14, 0x18, 0x06, 0x0F, 0x03, 0x16,
	0x01, 0x00, 0x0E, 0x10, 0x00, 0x13, 0x15, 0x17,
	0x00, 0x1A, 0x1C, 0x22, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x1F, 0x1E, 0x20, 0x13, 0x00, 0x10, 0x00,
	0x00, 0x0D, 0x0F, 0x11
};

static void handleKeys(SDL_Keycode keycode, SDL_Scancode scanKey);
static bool checkModifiedKeys(SDL_Keycode keycode);

static int32_t fastTracksChannelFromScancode(SDL_Scancode scancode)
{
	/* Physical keyboard rows map left-to-right onto XM channels 1..32:
	** 1..0 = 1..10, Q..P = 11..20, A..L = 21..29, Z..C = 30..32. */
	static const SDL_Scancode trackKeys[MAX_CHANNELS] =
	{
		SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
		SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8,
		SDL_SCANCODE_9, SDL_SCANCODE_0,
		SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R,
		SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I,
		SDL_SCANCODE_O, SDL_SCANCODE_P,
		SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
		SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K,
		SDL_SCANCODE_L,
		SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C
	};

	for (int32_t i = 0; i < MAX_CHANNELS; i++)
	{
		if (trackKeys[i] == scancode)
			return i;
	}

	return -1;
}

int8_t scancodeKeyToNote(SDL_Scancode scancode)
{
	if (scancode == SDL_SCANCODE_CAPSLOCK || scancode == SDL_SCANCODE_NONUSBACKSLASH)
		return NOTE_OFF;

	// translate key to note
	int8_t note = 0;
	if (scancode >= SDL_SCANCODE_B && scancode <= SDL_SCANCODE_SLASH)
		note = scancodeKey2Note[(int32_t)scancode - SDL_SCANCODE_B];

	if (note == 0)
		return -1; // not a note key, do further key handling

	return note + (editor.curOctave * 12);
}

static void setKeyModifiers(SDL_Keymod modState)
{
	keyb.leftCtrlPressed = (modState & KMOD_LCTRL) ? true : false;
	keyb.leftAltPressed = (modState & KMOD_LALT) ? true : false;
	keyb.leftShiftPressed = (modState & KMOD_LSHIFT) ? true : false;
#ifdef __APPLE__
	keyb.leftCommandPressed = (modState & KMOD_LGUI) ? true : false;
#endif
	keyb.keyModifierDown = (modState & (KMOD_LSHIFT | KMOD_LCTRL | KMOD_LALT | KMOD_LGUI)) ? true : false;


#ifdef _WIN32
	keyb.leftWinKeyDown = (modState & KMOD_LGUI) ? true : false;
#endif
}

void readKeyModifiers(void)
{
	setKeyModifiers(SDL_GetModState());
}

void keyUpHandler(SDL_Scancode scancode, SDL_Keycode keycode)
{
	if (editor.editTextFlag || ui.sysReqShown)
		return; // kludge: don't handle key up! (XXX: Is this hack really needed anymore?)

	// clear the one-shot Enter/Return suppression after the key is released
	if (keycode == SDLK_RETURN)
		keyb.ignoreNoteEnterKey = false;

	/* Yet another kludge for not leaving a ghost key-up event after an inputBox/okBox
	** was exited with a key press. They could be picked up as note release events.
	*/
	if (keyb.ignoreCurrKeyUp)
	{
		keyb.ignoreCurrKeyUp = false;
		return;
	}

	if (cursor.object == CURSOR_NOTE && !keyb.keyModifierDown)
		testNoteKeysRelease(scancode);

	if (scancode == SDL_SCANCODE_KP_PLUS)
		keyb.numPadPlusPressed = false;

	/* All 32 mapped track keys can act as momentary slip clutches. The global
	** transmission clutch is latched on key-down, so key release does nothing. */
	const int32_t fastTrackChannel = fastTracksChannelFromScancode(scancode);
	if (fastTrackChannel >= 0)
		fastTracksPOCClutchRelease(fastTrackChannel);

	keyb.keyRepeat = false;

	(void)keycode;
}

void keyDownHandler(SDL_Scancode scancode, SDL_Keycode keycode, SDL_Keymod modifiers,
	bool keyWasRepeated)
{
	if (keycode == SDLK_UNKNOWN)
		return;

	/* readInput() snapshots modifiers before SDL's event queue is drained. Use
	** the modifier mask attached to this exact key event so a chord whose
	** modifier and primary key arrive in the same queue pass cannot fall
	** through as an unmodified key. */
	setKeyModifiers(modifiers);

	// Any keyboard interaction makes the pattern cursor immediately bright again.
	resetPatternCursorBlink();

	// revert "delete/rename" mouse modes (disk op.)
	if (mouse.mode != MOUSE_MODE_NORMAL)
		setMouseMode(MOUSE_MODE_NORMAL);

	if (ui.sysReqShown)
	{
		if (keycode == SDLK_RETURN)
			ui.sysReqEnterPressed = true;
		else if (keycode == SDLK_ESCAPE)
			ui.sysReqShown = false;

		return;
	}

	if (testNibblesCheatCodes(keycode))
		return; // current key (+ modifiers) matches nibbles cheat code sequence, ignore current key here

	if (keyWasRepeated)
	{
		if (editor.NI_Play || !keyb.keyRepeat)
			return; // do NOT repeat keys in Nibbles or if keyRepeat is disabled
	}

	if (scancode != SDL_SCANCODE_ESCAPE)
		keyb.keyRepeat = true;

	// handle certain keys (home/end/left/right etc) when editing text
	if (editor.editTextFlag)
	{
		handleTextEditControl(keycode);
		return;
	}

	if (editor.NI_Play)
	{
		nibblesKeyAdministrator(scancode);
		return;
	}

	if (patternNavPopupIsShown())
	{
		handlePatternNavPopupKey(keycode);
		return;
	}

	/* Silent Record Entry is a global Tapehead command on Ctrl+Grave. Claim it
	** before the preview handlers so Melodic Walk's plain/Shift+Grave spacing
	** controls remain available while Ctrl+Grave still toggles Silent Record.
	** Consume key-repeat events without repeatedly toggling the setting. */
	if (scancode == SDL_SCANCODE_GRAVE && (modifiers & KMOD_CTRL) &&
		!(modifiers & (KMOD_SHIFT | KMOD_ALT | KMOD_GUI)))
	{
		if (!keyWasRepeated)
			cbSilentRecEntry();

		return;
	}

	if (instrumentTransformHandlePreviewKey(scancode, keycode, keyWasRepeated))
		return;

	if (interpolationHandlePreviewKey(scancode, keycode, keyWasRepeated))
		return;

	if (!keyWasRepeated && scancode == SDL_SCANCODE_SPACE && keyb.leftCtrlPressed &&
		keyb.leftShiftPressed && !keyb.leftAltPressed)
	{
		openPatternNavPopup();
		return;
	}

	if (keycode == SDLK_ESCAPE)
	{
		if (quitBox(false) == 1)
			editor.throwExit = true;

		return;
	}

	if (scancode == SDL_SCANCODE_KP_PLUS)
		keyb.numPadPlusPressed = true;

	/* Tapehead Pattern Timeline: claim Extract + Stamp before the Fast Tracks
	** Alt+Shift track-key handler. E and X are also Fast Tracks ratio keys, so
	** the later handler would otherwise consume these shortcuts first. */
	if (!keyWasRepeated && ui.sampleEditorShown && keyb.leftAltPressed &&
		keyb.leftShiftPressed && !keyb.leftCtrlPressed)
	{
		if (scancode == SDL_SCANCODE_E)
		{
			extractSmpFromCursorToInstrAndStamp();
			return;
		}

		if (scancode == SDL_SCANCODE_X)
		{
			extractSmpRangeToInstrAndStamp();
			return;
		}
	}

	/* Ctrl+Shift+E/X are Sample Editor extraction shortcuts, but E and X are
	** also physical Fast Tracks keys for channels 13 and 31. Claim these
	** chords before the global per-track handler while the Sample Editor is
	** visible. Outside the Sample Editor, the Fast Tracks bindings retain
	** their normal behavior. Consume key-repeat events without extracting
	** again so a held chord cannot fill several sample slots accidentally. */
	if (ui.sampleEditorShown && keyb.leftCtrlPressed &&
		keyb.leftShiftPressed && !keyb.leftAltPressed)
	{
		if (keycode == SDLK_e)
		{
			if (!keyWasRepeated)
				extractSmpFromCursorToSample();

			return;
		}

		if (keycode == SDLK_x)
		{
			if (!keyWasRepeated)
				extractSmpRangeToSample();

			return;
		}
	}

	/* Ctrl+Alt+Plus toggles the latched global transmission clutch. Accept
	** both the main =/+ key and keypad plus so the command is practical across
	** keyboards while remaining tied to the physical plus-key position. */
	if (keyb.leftCtrlPressed && keyb.leftAltPressed &&
		(scancode == SDL_SCANCODE_EQUALS || scancode == SDL_SCANCODE_KP_PLUS) &&
		(fastTracksPOCAnyEnabled() || fastTracksPOCTransmissionClutchIsLatched()))
	{
		if (!keyWasRepeated)
			fastTracksPOCTransmissionClutchToggle();

		return;
	}

	const int32_t fastTrackChannel = fastTracksChannelFromScancode(scancode);


	/* Ctrl+Alt+track key is a momentary clutch. Only claim the command when
	** that track is currently active, preserving stock FT2 key behavior for
	** inactive channels. */
	if (keyb.leftCtrlPressed && keyb.leftAltPressed && !keyb.leftShiftPressed &&
		fastTrackChannel >= 0 && fastTracksPOCIsEnabled(fastTrackChannel))
	{
		if (!keyWasRepeated)
			fastTracksPOCClutchPress(fastTrackChannel);

		return;
	}

	// Alt+Shift+track key cycles that track's Fast Tracks ratio.
	if (!keyWasRepeated && keyb.leftAltPressed && keyb.leftShiftPressed && fastTrackChannel >= 0)
	{
		fastTracksPOCCycleRatio(fastTrackChannel);
		return;
	}

	// Ctrl+Shift+track key toggles Fast Tracks for that channel.
	if (!keyWasRepeated && keyb.leftCtrlPressed && keyb.leftShiftPressed && fastTrackChannel >= 0)
	{
		fastTracksPOCToggle(fastTrackChannel);
		return;
	}

	if (handleEditKeys(keycode, scancode))
		return;

	if (keyb.keyModifierDown && checkModifiedKeys(keycode))
		return;

	handleKeys(keycode, scancode); // no pattern editing, do general key handling
}

static void handleKeys(SDL_Keycode keycode, SDL_Scancode scanKey)
{
	// if we're holding numpad plus but not pressing bank keys, don't check any other key
	if (keyb.numPadPlusPressed && !keyb.leftCtrlPressed)
	{
		if (scanKey != SDL_SCANCODE_NUMLOCKCLEAR && scanKey != SDL_SCANCODE_KP_DIVIDE &&
			scanKey != SDL_SCANCODE_KP_MULTIPLY  && scanKey != SDL_SCANCODE_KP_MINUS)
		{
			return;
		}
	}

	// handle scankeys (actual key on keyboard, using US keyb. layout)
	switch (scanKey)
	{
		case SDL_SCANCODE_KP_ENTER: pbSwapInstrBank(); return;

		case SDL_SCANCODE_NUMLOCKCLEAR:
		{
			if (keyb.numPadPlusPressed)
			{
				if (editor.instrBankSwapped)
					pbSetInstrBank13();
				else
					pbSetInstrBank5();
			}
			else
			{
				if (editor.instrBankSwapped)
					pbSetInstrBank9();
				else
					pbSetInstrBank1();
			}
		}
		return;

		case SDL_SCANCODE_KP_DIVIDE:
		{
			if (keyb.numPadPlusPressed)
			{
				if (editor.instrBankSwapped)
					pbSetInstrBank14();
				else
					pbSetInstrBank6();
			}
			else
			{
				if (editor.instrBankSwapped)
					pbSetInstrBank10();
				else
					pbSetInstrBank2();
			}
		}
		return;

		case SDL_SCANCODE_KP_MULTIPLY:
		{
			if (keyb.numPadPlusPressed)
			{
				if (editor.instrBankSwapped)
					pbSetInstrBank15();
				else
					pbSetInstrBank7();
			}
			else
			{
				if (editor.instrBankSwapped)
					pbSetInstrBank11();
				else
					pbSetInstrBank3();
			}
		}
		return;

		case SDL_SCANCODE_KP_MINUS:
		{
			if (keyb.leftCtrlPressed) // non-FT2 feature: Decrease master volume by 16
			{
				if (config.masterVol >= 16)
					config.masterVol -= 16;
				else
					config.masterVol = 0;

				setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));
				if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
					showConfigScreen();
			}
			else
			{
				if (keyb.numPadPlusPressed)
				{
					if (editor.instrBankSwapped)
						pbSetInstrBank16();
					else
						pbSetInstrBank8();
				}
				else
				{
					if (editor.instrBankSwapped)
						pbSetInstrBank12();
					else
						pbSetInstrBank4();
				}
			}
		}
		return;

		case SDL_SCANCODE_KP_PLUS:
		{
			if (keyb.leftCtrlPressed) // non-FT2 feature: Increase master volume by 16
			{
				if (config.masterVol <= 256-16)
					config.masterVol += 16;
				else
					config.masterVol = 256;

				setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));
				if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
					showConfigScreen();
			}
		}
		return;

		case SDL_SCANCODE_KP_PERIOD:
		{
			if (editor.curInstr > 0)
			{
				if (keyb.leftShiftPressed) // this only triggers if num lock is off. Probably an SDL bug...
				{
					clearSample();
				}
				else
				{
					if (editor.curInstr == 0 || instr[editor.curInstr] == NULL)
						return;

					if (okBox(1, "System request", "Clear instrument?", NULL) == 1)
					{
						freeInstr(editor.curInstr);
						memset(song.instrName[editor.curInstr], 0, sizeof(song.instrName[editor.curInstr]));
						updateNewInstrument();
						setSongModifiedFlag();
					}
				}
			}
		}
		return;

		case SDL_SCANCODE_KP_0: setNewInstr(0); return;
		case SDL_SCANCODE_KP_1: setNewInstr(editor.instrBankOffset+1); return;
		case SDL_SCANCODE_KP_2: setNewInstr(editor.instrBankOffset+2); return;
		case SDL_SCANCODE_KP_3: setNewInstr(editor.instrBankOffset+3); return;
		case SDL_SCANCODE_KP_4: setNewInstr(editor.instrBankOffset+4); return;
		case SDL_SCANCODE_KP_5: setNewInstr(editor.instrBankOffset+5); return;
		case SDL_SCANCODE_KP_6: setNewInstr(editor.instrBankOffset+6); return;
		case SDL_SCANCODE_KP_7: setNewInstr(editor.instrBankOffset+7); return;
		case SDL_SCANCODE_KP_8: setNewInstr(editor.instrBankOffset+8); return;

		case SDL_SCANCODE_GRAVE: // "key below esc"
		{
			if (keyb.leftShiftPressed)
			{
				// decrease edit skip
				if (editor.editRowSkip == 0)
					editor.editRowSkip = 16;
				else
					editor.editRowSkip--;
			}
			else
			{
				// increase edit skip
				if (editor.editRowSkip == 16)
					editor.editRowSkip = 0;
				else
					editor.editRowSkip++;
			}

			if (!ui.nibblesShown     && !ui.configScreenShown &&
				!ui.aboutScreenShown && !ui.diskOpShown       &&
				!ui.helpScreenShown  && !ui.extendedPatternEditor)
			{
				drawIDAdd();
			}
		}
		return;

		default: break;
	}

	// no normal key (keycode) pressed (XXX: shouldn't happen..?)
	if (keycode == SDLK_UNKNOWN)
		return;

	// handle normal keys (keycodes - affected by keyb. layout in OS)
	switch (keycode)
	{
		default: return;

		case SDLK_DELETE: // non-FT2 addition
		{
			if (ui.sampleEditorShown)
				sampCut();
		}
		break;

		// This is maybe not an ideal key for this anymore...
		//case SDLK_PRINTSCREEN: togglePatternEditorExtended(); break;

		// EDIT/PLAY KEYS

		// record pattern
		case SDLK_RSHIFT: startPlaying(PLAYMODE_RECPATT, 0); break;

		// play song
#ifdef __APPLE__
		case SDLK_RGUI: // fall-through for Apple keyboards
#endif
		case SDLK_RCTRL:
			startPlaying(PLAYMODE_SONG, 0);
		break;

		// play pattern
		case SDLK_MODE: // Alt Gr is SDLK_MODE on some keyboards/layouts
		case SDLK_RALT:
		{
			if (!keyb.leftCtrlPressed) // kludge for Mac (toggle fullscreen)
				startPlaying(PLAYMODE_PATT, 0);
		}
		break;

		case SDLK_SPACE:
		{
			if (playMode == PLAYMODE_IDLE)
			{
				lockMixerCallback();
				memset(editor.keyOnTab, 0, sizeof (editor.keyOnTab));
				playMode = PLAYMODE_EDIT;
				ui.updatePosSections = true; // for updating mode text
				unlockMixerCallback();
			}
			else
			{
				stopPlaying();
			}

			// Cursor color depends on playMode, so repaint it immediately.
			ui.updatePatternEditor = true;
		}
		break;

		case SDLK_TAB:
		{
			if (keyb.leftShiftPressed)
				cursorTabLeft();
			else
				cursorTabRight();
		}
		break;

		case SDLK_LEFT:  cursorLeft();  break;
		case SDLK_RIGHT: cursorRight(); break;

		// function Keys (F1..F12)

		case SDLK_F1:
		{
			     if (keyb.leftShiftPressed) trackTranspAllInsDn();
			else if (keyb.leftCtrlPressed)  pattTranspAllInsDn();
			else if (keyb.leftAltPressed)   blockTranspAllInsDn();
			else                            editor.curOctave = 0;
		}
		break;

		case SDLK_F2:
		{
			     if (keyb.leftShiftPressed) trackTranspAllInsUp();
			else if (keyb.leftCtrlPressed)  pattTranspAllInsUp();
			else if (keyb.leftAltPressed)   blockTranspAllInsUp();
			else                            editor.curOctave = 1;
		}
		break;

		case SDLK_F3:
		{
			     if (keyb.leftShiftPressed) cutTrack();
			else if (keyb.leftCtrlPressed)  cutPattern();
			else if (keyb.leftAltPressed)   cutBlock();
			else                            editor.curOctave = 2;
		}
		break;

		case SDLK_F4:
		{
			     if (keyb.leftShiftPressed) copyTrack();
			else if (keyb.leftCtrlPressed)  copyPattern();
			else if (keyb.leftAltPressed)   copyBlock();
			else                            editor.curOctave = 3;
		}
		break;

		case SDLK_F5:
		{
			     if (keyb.leftShiftPressed) pasteTrack();
			else if (keyb.leftCtrlPressed)  pastePattern();
			else if (keyb.leftAltPressed)   pasteBlock();
			else                            editor.curOctave = 4;
		}
		break;

		case SDLK_F6: editor.curOctave = 5; break;

		case SDLK_F7:
		{
			     if (keyb.leftShiftPressed) trackTranspCurInsDn();
			else if (keyb.leftCtrlPressed)  pattTranspCurInsDn();
			else if (keyb.leftAltPressed)   blockTranspCurInsDn();
			else                            editor.curOctave = 6;
		}
		break;

		case SDLK_F8:
		{
			     if (keyb.leftShiftPressed) trackTranspCurInsUp();
			else if (keyb.leftCtrlPressed)  pattTranspCurInsUp();
			else if (keyb.leftAltPressed)   blockTranspCurInsUp();
			else                            editor.curOctave = 6;
		}
		break;

		case SDLK_F9:
		{
			lockAudio();

			song.row = editor.ptnJumpPos[0];
			if (song.row >= song.currNumRows)
				song.row = song.currNumRows - 1;

			if (!songPlaying)
			{
				editor.row = (uint8_t)song.row;
				ui.updatePatternEditor = true;
			}

			unlockAudio();
		}
		break;

		case SDLK_F10:
		{
			lockAudio();

			song.row = editor.ptnJumpPos[1];
			if (song.row >= song.currNumRows)
				song.row = song.currNumRows - 1;

			if (!songPlaying)
			{
				editor.row = (uint8_t)song.row;
				ui.updatePatternEditor = true;
			}

			unlockAudio();
		}
		break;

		case SDLK_F11:
		{
			lockAudio();

			song.row = editor.ptnJumpPos[2];
			if (song.row >= song.currNumRows)
				song.row  = song.currNumRows - 1;

			if (!songPlaying)
			{
				editor.row = (uint8_t)song.row;
				ui.updatePatternEditor = true;
			}

			unlockAudio();
		}
		break;

		case SDLK_F12:
		{
			lockAudio();

			song.row = editor.ptnJumpPos[3];
			if (song.row >= song.currNumRows)
				song.row = song.currNumRows - 1;

			if (!songPlaying)
			{
				editor.row = (uint8_t)song.row;
				ui.updatePatternEditor = true;
			}

			unlockAudio();
		}
		break;

		// PATTERN EDITOR POSITION KEYS
		
		case SDLK_INSERT:
		{
			if (keyb.leftShiftPressed)
				insertPatternLine();
			else
				insertPatternNote();
		}
		break;

		case SDLK_BACKSPACE:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed) redoPerform();
				else if (!keyb.leftCtrlPressed) undoPerform();
			}
			else if (ui.diskOpShown && tapeheadConfig.diskOpBackspaceParent) diskOpGoParent();
			else if (keyb.leftShiftPressed) deletePatternLine();
			else if (tapeheadConfig.patternBackspacePullUp) deletePatternNote();
			else clearPreviousPatternEntry();
		}
		break;

		case SDLK_UP:
		{
			if (keyb.leftShiftPressed)
			{
				decCurIns();
			}
			else
			{
				if (keyb.leftAltPressed)
					keybPattMarkUp();
				else
					rowOneUpWrap();
			}
			break;
		}
		break;

		case SDLK_DOWN:
		{
			if (keyb.leftShiftPressed)
			{
				incCurIns();
			}
			else
			{
				if (keyb.leftAltPressed)
					keybPattMarkDown();
				else
					rowOneDownWrap();
			}
		}
		break;

		case SDLK_PAGEUP:
			rowUp(16);
		break;

		case SDLK_PAGEDOWN:
			rowDown(16);
		break;

		case SDLK_HOME:
		{
			const bool audioWasntLocked = !audio.locked;
			if (audioWasntLocked)
				lockAudio();

			song.row = 0;
			if (!songPlaying)
			{
				editor.row = (uint8_t)song.row;
				ui.updatePatternEditor = true;
			}

			if (audioWasntLocked)
				unlockAudio();
		}
		break;

		case SDLK_END:
		{
			const bool audioWasntLocked = !audio.locked;
			if (audioWasntLocked)
				lockAudio();

			song.row = song.currNumRows - 1;
			if (!songPlaying)
			{
				editor.row = (uint8_t)song.row;
				ui.updatePatternEditor = true;
			}

			if (audioWasntLocked)
				unlockAudio();
		}
		break;
	}
}

static bool checkModifiedKeys(SDL_Keycode keycode)
{
	// normal keys
	switch (keycode)
	{
		default: break;

		case SDLK_KP_ENTER:
		case SDLK_RETURN:
		{
			if (keyb.leftAltPressed && !keyb.leftCtrlPressed)
			{
				toggleFullscreen();
				return true;
			}
			else if (keyb.leftCommandPressed || keyb.leftCtrlPressed)
			{
				if (keyb.leftShiftPressed)
					insertPatternLine();
				else
					insertPatternNote();
			}
		}
		break;

		case SDLK_F9:
		{
			if (keyb.leftCtrlPressed)
			{
				startPlaying(PLAYMODE_PATT, editor.ptnJumpPos[0]);
				return true;
			}
			else if (keyb.leftShiftPressed)
			{
				editor.ptnJumpPos[0] = (uint8_t)editor.row;
				return true;
			}
		}
		break;

		case SDLK_F10:
		{
			if (keyb.leftCtrlPressed)
			{
				startPlaying(PLAYMODE_PATT, editor.ptnJumpPos[1]);
				return true;
			}
			else if (keyb.leftShiftPressed)
			{
				editor.ptnJumpPos[1] = (uint8_t)editor.row;
				return true;
			}
		}
		break;

		case SDLK_F11:
		{
			if (keyb.leftCtrlPressed)
			{
				startPlaying(PLAYMODE_PATT, editor.ptnJumpPos[2]);
				return true;
			}
			else if (keyb.leftShiftPressed)
			{
				editor.ptnJumpPos[2] = (uint8_t)editor.row;
				return true;
			}
		}
		break;

		case SDLK_F12:
		{
			if (keyb.leftCtrlPressed)
			{
				startPlaying(PLAYMODE_PATT, editor.ptnJumpPos[3]);
				return true;
			}
			else if (keyb.leftShiftPressed)
			{
				editor.ptnJumpPos[3] = (uint8_t)editor.row;
				return true;
			}
		}
		break;

		case SDLK_a:
		{
			if (ui.sampleEditorShown)
			{
#ifdef __APPLE__
				if (keyb.leftCtrlPressed || keyb.leftAltPressed || keyb.leftCommandPressed)
#else
				if (keyb.leftCtrlPressed || keyb.leftAltPressed)
#endif
				{
					rangeAll();
					return true;
				}
			}
			else if (keyb.leftCtrlPressed)
			{
				showAdvEdit();
				return true;
			}
			else if (keyb.leftAltPressed)
			{
				jumpToChannel(8);
				return true;
			}
		}
		break;

		case SDLK_b:
		{
			if (keyb.leftCtrlPressed && keyb.leftShiftPressed)
				return interpolationBegin(INTERPOLATE_EFFECT);

			if (keyb.leftCtrlPressed)
			{
				if (!ui.aboutScreenShown)
					showAboutScreen();

				return true;
			}
		}
		break;

		case SDLK_c:
		{
#ifdef __APPLE__
			if (keyb.leftAltPressed || keyb.leftCommandPressed)
#else
			if (keyb.leftAltPressed)
#endif
			{
				if (ui.sampleEditorShown)
				{
					sampCopy();
				}
				else
				{
					// mark current track (non-FT2 feature)
					pattMark.markX1 = cursor.ch;
					pattMark.markX2 = pattMark.markX1;
					pattMark.markY1 = 0;
					pattMark.markY2 = patternNumRows[editor.editPattern];

					ui.updatePatternEditor = true;
				}

				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				if (ui.sampleEditorShown)
					sampCopy();
				else
					showConfigScreen();

				return true;
			}
		}
		break;

		case SDLK_d:
		{
			// Tape Head Edition: Shift+D plays the currently displayed
			// portion of the sample editor.
			if (keyb.leftShiftPressed && !keyb.leftCtrlPressed &&
			    !keyb.leftAltPressed && ui.sampleEditorShown)
			{
				sampPlayDisplay();
				return true;
			}

			if (keyb.leftAltPressed)
			{
				jumpToChannel(10);
				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				if (!ui.diskOpShown)
					showDiskOpScreen();

				return true;
			}
		}
		break;

		case SDLK_e:
		{
			/* Alt+Shift+E: Extract tail + Stamp. */
			if (keyb.leftShiftPressed && keyb.leftAltPressed && !keyb.leftCtrlPressed && ui.sampleEditorShown)
			{
				extractSmpFromCursorToInstrAndStamp();
				return true;
			}

			/*
			** Tape Head Edition: extract from the current sample cursor to
			** the end into a newly allocated instrument.
			*/
			if (keyb.leftShiftPressed && !keyb.leftAltPressed && ui.sampleEditorShown)
			{
				if (keyb.leftCtrlPressed)
					extractSmpFromCursorToSample();
				else
					extractSmpFromCursorToInstr();

				return true;
			}

			if (keyb.leftAltPressed)
			{
				jumpToChannel(2);
				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				if (ui.aboutScreenShown)  hideAboutScreen();
				if (ui.configScreenShown) hideConfigScreen();
				if (ui.helpScreenShown)   hideHelpScreen();
				if (ui.nibblesShown)      hideNibblesScreen();

				showSampleEditorExt();
				return true;
			}
		}
		break;

		case SDLK_f:
		{
#ifdef __APPLE__
			if (keyb.leftCommandPressed && keyb.leftCtrlPressed)
			{
				toggleFullscreen();
				return true;
			}
			else
#endif
			if (keyb.leftShiftPressed && keyb.leftCtrlPressed)
			{
				resetFPSCounter();
				video.showFPSCounter ^= 1;
				if (!video.showFPSCounter)
				{
					if (ui.extendedPatternEditor) // yet another kludge...
						exitPatternEditorExtended();

					showTopScreen(DONT_RESTORE_SCREENS);
				}
			}
			else if (keyb.leftAltPressed)
			{
				jumpToChannel(11);
				return true;
			}
		}
		break;

		case SDLK_g:
		{
			if (keyb.leftAltPressed)
			{
				jumpToChannel(12);
				return true;
			}
		}
		break;

		case SDLK_h:
		{
			if (keyb.leftAltPressed)
			{
				jumpToChannel(13);
				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				showHelpScreen();
				return true;
			}
		}
		break;

		case SDLK_i:
		{
			if (keyb.leftAltPressed)
			{
				jumpToChannel(7);
				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				showInstEditor();
				return true;
			}
		}
		break;

		case SDLK_j:
		{
			if (keyb.leftAltPressed)
			{
				jumpToChannel(14);
				return true;
			}
		}
		break;

		case SDLK_k:
		{
			if (keyb.leftAltPressed)
			{
				jumpToChannel(15);
				return true;
			}
		}
		break;

		case SDLK_m:
		{
			if (keyb.leftCtrlPressed && keyb.leftShiftPressed)
				return interpolationBegin(INTERPOLATE_NOTES);

			if (keyb.leftCtrlPressed)
			{
				if (ui.aboutScreenShown)  hideAboutScreen();
				if (ui.configScreenShown) hideConfigScreen();
				if (ui.helpScreenShown)   hideHelpScreen();
				if (ui.nibblesShown)      hideNibblesScreen();

				showInstEditorExt();

				return true;
			}
		}
		break;


		case SDLK_n:
		{
			if (keyb.leftCtrlPressed)
			{
				showNibblesScreen();
				return true;
			}
		}
		break;

		case SDLK_p:
		{
			if (keyb.leftCtrlPressed)
			{
				if (!ui.patternEditorShown)
				{
					if (ui.sampleEditorShown)    hideSampleEditor();
					if (ui.sampleEditorExtShown) hideSampleEditorExt();
					if (ui.instEditorShown)      hideInstEditor();

					showPatternEditor();
				}

				return true;
			}
		}
		break;

		case SDLK_q:
		{
			if (keyb.leftAltPressed)
			{
				jumpToChannel(0);
				return true;
			}
		}
		break;

		case SDLK_r:
		{
			if (keyb.leftAltPressed)
			{
				if (ui.sampleEditorShown)
					sampCrop();
				else
					jumpToChannel(3);

				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				showTrimScreen();
				return true;
			}
		}
		break;

		case SDLK_s:
		{
			// Tape Head Edition: Shift+S plays the marked sample range.
			if (keyb.leftShiftPressed && !keyb.leftCtrlPressed &&
			    !keyb.leftAltPressed && ui.sampleEditorShown)
			{
				sampPlayRange();
				return true;
			}

			if (keyb.leftAltPressed)
			{
				if (ui.sampleEditorShown)
					showRange();
				else
					jumpToChannel(9);

				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				showSampleEditor();
				return true;
			}
		}
		break;

		case SDLK_t:
		{
			if (keyb.leftAltPressed)
			{
				jumpToChannel(4);
				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				showTranspose();
				return true;
			}
		}
		break;

		case SDLK_u:
		{
			if (keyb.leftAltPressed)
			{
				jumpToChannel(6);
				return true;
			}
		}
		break;

		case SDLK_v:
		{
			if (keyb.leftCtrlPressed && keyb.leftShiftPressed)
				return interpolationBegin(INTERPOLATE_VOLUME);

#ifdef __APPLE__
			if (keyb.leftAltPressed || keyb.leftCommandPressed)
#else
			if (keyb.leftAltPressed)
#endif
			{
				if (ui.sampleEditorShown)
					sampPaste();
				else if (!ui.instEditorShown)
					scaleFadeVolumeBlock();

				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				if (ui.sampleEditorShown)
					sampPaste();
				else if (!ui.instEditorShown)
					scaleFadeVolumePattern();

				return true;
			}
			else if (keyb.leftShiftPressed)
			{
				if (!ui.sampleEditorShown && !ui.instEditorShown)
				{
					keyb.ignoreTextEditKey = true; // ignore key from first frame
					scaleFadeVolumeTrack();
				}

				return true;
			}
		}
		break;

		case SDLK_w:
		{
			if (keyb.leftAltPressed)
			{
				jumpToChannel(1);
				return true;
			}
		}
		break;

		case SDLK_x:
		{
			/* Alt+Shift+X: Extract selected range + Stamp. */
			if (keyb.leftShiftPressed && keyb.leftAltPressed && !keyb.leftCtrlPressed && ui.sampleEditorShown)
			{
				extractSmpRangeToInstrAndStamp();
				return true;
			}

			/*
			** Tape Head Edition: extract the selected sample range to a
			** newly allocated instrument while remaining on the master.
			*/
			if (keyb.leftShiftPressed && !keyb.leftAltPressed && ui.sampleEditorShown)
			{
				if (keyb.leftCtrlPressed)
					extractSmpRangeToSample();
				else
					extractSmpRangeToInstr();

				return true;
			}

#ifdef __APPLE__
			if (keyb.leftAltPressed || keyb.leftCommandPressed)
#else
			if (keyb.leftAltPressed)
#endif
			{
				if (ui.sampleEditorShown)
					sampCut();

				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				if (ui.extendedPatternEditor)
					exitPatternEditorExtended();

				if (ui.sampleEditorShown)    hideSampleEditor();
				if (ui.sampleEditorExtShown) hideSampleEditorExt();
				if (ui.instEditorShown)      hideInstEditor();
				if (ui.instEditorExtShown)   hideInstEditorExt();
				if (ui.transposeShown)       hideTranspose();
				if (ui.aboutScreenShown)     hideAboutScreen();
				if (ui.configScreenShown)    hideConfigScreen();
				if (ui.helpScreenShown)      hideHelpScreen();
				if (ui.nibblesShown)         hideNibblesScreen();
				if (ui.diskOpShown)          hideDiskOpScreen();
				if (ui.advEditShown)         hideAdvEdit();
				if (ui.wavRendererShown)     hideWavRenderer();
				if (ui.trimScreenShown)      hideTrimScreen();

				showTopScreen(DONT_RESTORE_SCREENS);
				showBottomScreen();

				showPatternEditor();

				return true;
			}
		}
		break;

		case SDLK_y:
		{
			if (keyb.leftAltPressed)
			{
				jumpToChannel(5);
				return true;
			}
		}
		break;

		case SDLK_z:
		{
			if (keyb.leftAltPressed)
			{
				if (ui.sampleEditorShown)
					zoomOut();

				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				togglePatternEditorExtended();
				return true;
			}
		}
		break;

		case SDLK_1:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed)
					writeToMacroSlot(1-1);
				else
					writeFromMacroSlot(1-1);

				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				editor.currConfigScreen = 0;
				showConfigScreen();
				checkRadioButton(RB_CONFIG_AUDIO);

				return true;
			}
		}
		break;

		case SDLK_2:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed)
					writeToMacroSlot(2-1);
				else
					writeFromMacroSlot(2-1);

				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				editor.currConfigScreen = 1;
				showConfigScreen();
				checkRadioButton(RB_CONFIG_LAYOUT);

				return true;
			}
		}
		break;

		case SDLK_3:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed)
					writeToMacroSlot(3-1);
				else
					writeFromMacroSlot(3-1);

				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				editor.currConfigScreen = 2;
				showConfigScreen();
				checkRadioButton(RB_CONFIG_MISCELLANEOUS);

				return true;
			}
		}
		break;

		case SDLK_4:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed)
					writeToMacroSlot(4-1);
				else
					writeFromMacroSlot(4-1);

				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
#ifdef HAS_MIDI
				editor.currConfigScreen = 3;
				showConfigScreen();
				checkRadioButton(RB_CONFIG_MIDI_INPUT);
#endif
				return true;
			}
		}
		break;

		case SDLK_5:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed)
					writeToMacroSlot(5-1);
				else
					writeFromMacroSlot(5-1);

				return true;
			}
		}
		break;

		case SDLK_6:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed)
					writeToMacroSlot(6-1);
				else
					writeFromMacroSlot(6-1);

				return true;
			}
		}
		break;

		case SDLK_7:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed)
					writeToMacroSlot(7-1);
				else
					writeFromMacroSlot(7-1);

				return true;
			}
		}
		break;

		case SDLK_8:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed)
					writeToMacroSlot(8-1);
				else
					writeFromMacroSlot(8-1);

				return true;
			}
		}
		break;

		case SDLK_9:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed)
					writeToMacroSlot(9-1);
				else
					writeFromMacroSlot(9-1);

				return true;
			}
		}
		break;

		case SDLK_0:
		{
			if (keyb.leftAltPressed)
			{
				if (keyb.leftShiftPressed)
					writeToMacroSlot(10-1);
				else
					writeFromMacroSlot(10-1);

				return true;
			}
		}
		break;

		case SDLK_LEFT:
		{
			if (keyb.leftShiftPressed)
			{
				decSongPos();
				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				pbEditPattDown();
				return true;
			}
			else if (keyb.leftAltPressed)
			{
				keybPattMarkLeft();
				return true;
			}
		}
		break;

		case SDLK_RIGHT:
		{
			if (keyb.leftShiftPressed)
			{
				incSongPos();
				return true;
			}
			else if (keyb.leftCtrlPressed)
			{
				pbEditPattUp();
				return true;
			}
			else if (keyb.leftAltPressed)
			{
				keybPattMarkRight();
				return true;
			}
		}
		break;
	}

	return false;
}
