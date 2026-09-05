// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#include <SDL2/SDL_syswm.h>
#else
#include <unistd.h> // usleep()
#endif
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_config.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_events.h"
#include "ft2_mouse.h"
#include "scopes/ft2_scopes.h"
#include "ft2_pattern_ed.h"
#include "ft2_pattern_draw.h"
#include "ft2_sample_ed.h"
#include "ft2_nibbles.h"
#include "ft2_inst_ed.h"
#include "ft2_diskop.h"
#include "ft2_about.h"
#include "ft2_trim.h"
#include "ft2_sampling.h"
#include "ft2_module_loader.h"
#include "ft2_midi.h"
#include "ft2_bmp.h"
#include "ft2_structs.h"
#include "ft2_edit.h"
#include "ft2_video_scaler.h"
#include "ft2_jack.h"
#include "ft2_live_link.h"

static const uint8_t textCursorData[12] =
{
	PAL_FORGRND, PAL_FORGRND, PAL_FORGRND,
	PAL_FORGRND, PAL_FORGRND, PAL_FORGRND,
	PAL_FORGRND, PAL_FORGRND, PAL_FORGRND,
	PAL_FORGRND, PAL_FORGRND, PAL_FORGRND
};

#if defined _WIN32 || defined __linux__
static void setTapeheadWindowIcon(void)
{
	SDL_Surface *icon = NULL;
	char path[PATH_MAX + 1];
	char *basePath = SDL_GetBasePath();
	if (basePath != NULL)
	{
		const int32_t charsWritten = snprintf(path, sizeof (path), "%s%s",
			basePath, "tapehead-icon.bmp");
		if (charsWritten > 0 && charsWritten < (int32_t)sizeof (path))
			icon = SDL_LoadBMP(path);
		SDL_free(basePath);
	}

	if (icon == NULL)
		icon = SDL_LoadBMP("src/gfxdata/icon/tapehead/tapehead-icon.bmp");

	if (icon != NULL)
	{
		SDL_SetWindowIcon(video.window, icon);
		SDL_FreeSurface(icon);
	}
}
#endif

video_t video; // globalized

static bool songIsModified;
static volatile bool auditionNotesHeld[96];
static char wndTitle[256];
static sprite_t sprites[SPRITE_NUM];

void setAuditionNoteState(uint8_t note, bool held)
{
	if (note < 1 || note > 96)
		return;

	auditionNotesHeld[note-1] = held;
	ui.updatePosSections = true;
}

static int16_t getLowestAuditionNotes(int16_t *notes, int16_t maxNotes)
{
	int16_t notesFound = 0;

	for (int16_t i = 0; i < 96 && notesFound < maxNotes; i++)
	{
		if (auditionNotesHeld[i])
			notes[notesFound++] = i + 1;
	}

	return notesFound;
}

static void formatAuditionNote(char *text, int16_t note)
{
	static const char *sharpNotes[12] =
	{
		"C-", "C#", "D-", "D#", "E-", "F-",
		"F#", "G-", "G#", "A-", "A#", "B-"
	};
	static const char *flatNotes[12] =
	{
		"C-", "Db", "D-", "Eb", "E-", "F-",
		"Gb", "G-", "Ab", "A-", "Bb", "B-"
	};

	const int16_t noteIndex = note - 1;
	const char *noteName = config.ptnAcc == 0 ? sharpNotes[noteIndex % 12] : flatNotes[noteIndex % 12];
	text[0] = noteName[0];
	text[1] = noteName[1];
	text[2] = '0' + (char)(noteIndex / 12);
	text[3] = '\0';
}

// for FPS counter
#define FPS_LINES 19
#define FPS_SCAN_FRAMES 60
#define FPS_RENDER_W 285
#define FPS_RENDER_H (((FONT1_CHAR_H + 1) * FPS_LINES) + 1)
#define FPS_RENDER_X 2
#define FPS_RENDER_Y 2

static char fpsTextBuf[1024];
static bool avgFramesReady;
static uint32_t videoFrameCounter;
static uint64_t frameStartTime, runningFrameDuration;
static double dFrameDurationDiv, dAvgFPS;
// ------------------

static void drawReplayerData(void);

void resetFPSCounter(void)
{
	videoFrameCounter = 0;
	fpsTextBuf[0] = '\0';
	runningFrameDuration = 0;
	avgFramesReady = false;
}

void beginFPSCounter(void)
{
	if (video.showFPSCounter)
		frameStartTime = SDL_GetPerformanceCounter();
}

static void drawFPSCounter(void)
{
	SDL_version SDLVer;

	SDL_GetVersion(&SDLVer);

	if (++videoFrameCounter >= FPS_SCAN_FRAMES)
	{
		dAvgFPS = dFrameDurationDiv / (double)runningFrameDuration;
		if (dAvgFPS < 0.0 || dAvgFPS > 99999999.9999)
			dAvgFPS = 99999999.9999; // prevent number from overflowing text box

		runningFrameDuration = 0;
		videoFrameCounter = 0;
		avgFramesReady = true;
	}

	clearRect(FPS_RENDER_X+2, FPS_RENDER_Y+2, FPS_RENDER_W, FPS_RENDER_H);
	vLineDouble(FPS_RENDER_X, FPS_RENDER_Y+1, FPS_RENDER_H+2, PAL_FORGRND);
	vLineDouble(FPS_RENDER_X+FPS_RENDER_W, FPS_RENDER_Y+1, FPS_RENDER_H+2, PAL_FORGRND);
	hLineDouble(FPS_RENDER_X+1, FPS_RENDER_Y, FPS_RENDER_W, PAL_FORGRND);
	hLineDouble(FPS_RENDER_X+1, FPS_RENDER_Y+FPS_RENDER_H+2, FPS_RENDER_W, PAL_FORGRND);

	// if enough frame data isn't collected yet, show a message
	if (!avgFramesReady)
	{
		const char *text = "Gathering frame information...";
		const uint16_t textW = textWidth(text);
		textOut(FPS_RENDER_X+((FPS_RENDER_W/2)-(textW/2)), FPS_RENDER_Y+((FPS_RENDER_H/2)-(FONT1_CHAR_H/2)), PAL_FORGRND, text);
		return;
	}

	double dRefreshRate = video.dMonitorRefreshRate;
	if (dRefreshRate < 0.0 || dRefreshRate > 9999.9)
		dRefreshRate = 9999.9; // prevent number from overflowing text box

	const char *audioDriver = tapeheadLiveLinkIsOpen() ? "shared-memory" :
		tapeheadJackIsOpen() ? "jack-native" : SDL_GetCurrentAudioDriver();
	const char *audioDevice = audio.outputDeviceLost
		? "DISCONNECTED" : audioGetActiveOutputDevice();

	sprintf(fpsTextBuf,
	             "SDL version: %u.%u.%u\n" \
	             "Frames per second: %.3f\n" \
	             "Monitor refresh rate: %.1fHz (+/-)\n" \
	             "59..61Hz GPU VSync used: %s\n" \
	             "HPC frequency (timer): %.4fMHz\n" \
	             "Audio backend: %s\n" \
	             "Audio device: %.35s\n" \
	             "Audio format: %s, %u channels\n" \
	             "Audio fallback: %s\n" \
	             "Audio frequency: %.1fkHz (expected %.1fkHz)\n" \
	             "Audio buffer samples: %d (expected %d)\n" \
	             "Render size: %dx%d (offset %d,%d)\n" \
	             "Disp. size: %dx%d (window: %dx%d)\n" \
	             "Render scaling: x=%.4f, y=%.4f\n" \
	             "DPI zoom factors: x=%.4f, y=%.4f\n" \
	             "Mouse pixel-space muls: x=%.4f, y=%.4f\n" \
	             "Relative mouse coords: %d,%d\n" \
	             "Absolute mouse coords: %d,%d\n" \
	             "Press CTRL+SHIFT+F to close this box.\n",
	             SDLVer.major, SDLVer.minor, SDLVer.patch,
	             dAvgFPS,
	             dRefreshRate,
	             video.vsync60HzPresent ? "yes" : "no",
	             hpcFreq.freq64 / (1000.0 * 1000.0),
	             audioDriver != NULL ? audioDriver : "unknown",
	             audioDevice, audioGetOutputFormatName(), audio.outputChannels,
	             audio.startupDefaultFallback ? "user-approved default" : "none",
	             audio.haveFreq / 1000.0, audio.wantFreq / 1000.0,
	             audio.haveSamples, audio.wantSamples,
	             video.renderW, video.renderH, video.renderX, video.renderY,
	             video.displayW, video.displayH, video.windowW, video.windowH,
	             (double)video.renderW / SCREEN_W, (double)video.renderH / SCREEN_H,
	             video.dDpiZoomFactorX, video.dDpiZoomFactorY,
	             video.dMouseXMul, video.dMouseYMul,
	             mouse.x, mouse.y,
	             mouse.absX, mouse.absY);

	// draw text

	uint16_t xPos = FPS_RENDER_X+3;
	uint16_t yPos = FPS_RENDER_Y+3;

	char *textPtr = fpsTextBuf;
	while (*textPtr != '\0')
	{
		const char ch = *textPtr++;
		if (ch == '\n')
		{
			yPos += FONT1_CHAR_H+1;
			xPos = FPS_RENDER_X+3;
			continue;
		}

		charOut(xPos, yPos, PAL_FORGRND, ch);
		xPos += charWidth(ch);
	}

	// draw framerate tester symbol

	const uint16_t symbolEnd = 115;

	// ping-pong movement
	uint16_t x = editor.framesPassed % (symbolEnd * 2);
	if (x >= symbolEnd)
		x = (symbolEnd * 2) - x;

	charOut(164 + x, 16, PAL_FORGRND, '*');
}

#define REC_PLUS_OVERLAY_SCALE 2
#define REC_PLUS_OVERLAY_GLYPH_H 18
#define REC_PLUS_OVERLAY_MAX_GLYPH_W (SCREEN_W - 2)
#define REC_PLUS_OVERLAY_PIXEL_COUNT \
	(REC_PLUS_OVERLAY_MAX_GLYPH_W * REC_PLUS_OVERLAY_GLYPH_H)

static char recPlusOverlayText[32];
static uint32_t *recPlusOverlayBackup;
static int32_t recPlusOverlayFrames;

void showRecPlusOverlay(const char *text)
{
	if (recPlusOverlayBackup == NULL)
	{
		recPlusOverlayBackup = (uint32_t *)malloc(
			SCREEN_W * SCREEN_H * sizeof (uint32_t));

		if (recPlusOverlayBackup == NULL)
			return;
	}
	else if (recPlusOverlayFrames > 0)
	{
		/* Remove the previous overlay before taking the next background copy.
		** Otherwise a quick CAPTURING BLOCK message snapshots BLOCK LOOP and
		** permanently draws the two phrases on top of each other. */
		memcpy(video.frameBuffer, recPlusOverlayBackup,
			SCREEN_W * SCREEN_H * sizeof (uint32_t));
	}

	memcpy(recPlusOverlayBackup, video.frameBuffer,
		SCREEN_W * SCREEN_H * sizeof (uint32_t));

	strncpy(recPlusOverlayText, text, sizeof (recPlusOverlayText)-1);
	recPlusOverlayText[sizeof (recPlusOverlayText)-1] = '\0';

	/* Approximately two seconds at 60Hz. */
	recPlusOverlayFrames = 120;
}

static void drawRecPlusOverlay(void)
{
	if (recPlusOverlayFrames <= 0 || recPlusOverlayBackup == NULL)
		return;

	memcpy(video.frameBuffer, recPlusOverlayBackup,
		SCREEN_W * SCREEN_H * sizeof (uint32_t));

	const int32_t scale = REC_PLUS_OVERLAY_SCALE;
	const int32_t glyphH = REC_PLUS_OVERLAY_GLYPH_H;
	const int32_t maxGlyphW = REC_PLUS_OVERLAY_MAX_GLYPH_W;
	const int32_t srcX = 1;
	const int32_t srcY = 1;

	const uint32_t alpha =
		(uint32_t)((recPlusOverlayFrames * 255) / 120);

	const uint32_t solidRed = RGB32(255, 24, 24);

	int32_t glyphW = textWidth16(recPlusOverlayText) + 2;
	if (glyphW > maxGlyphW)
		glyphW = maxGlyphW;

	/*
	** Temporarily render the normal large font in the upper-left
	** corner, capture its red letters and shadow, then restore the GUI.
	*/
	const uint32_t oldForeground = video.palette[PAL_FORGRND];
	video.palette[PAL_FORGRND] = solidRed;

	bigTextOutShadow(
		(uint16_t)srcX,
		(uint16_t)srcY,
		PAL_FORGRND,
		PAL_BLCKTXT,
		recPlusOverlayText);

	video.palette[PAL_FORGRND] = oldForeground;

	uint32_t glyphPixels[REC_PLUS_OVERLAY_PIXEL_COUNT];
	bool glyphMask[REC_PLUS_OVERLAY_PIXEL_COUNT];

	for (int32_t y = 0; y < glyphH; y++)
	{
		for (int32_t x = 0; x < glyphW; x++)
		{
			const int32_t srcIndex =
				((srcY - 1 + y) * SCREEN_W) + (srcX - 1 + x);

			const int32_t glyphIndex = (y * maxGlyphW) + x;

			glyphPixels[glyphIndex] = video.frameBuffer[srcIndex];

			glyphMask[glyphIndex] =
				video.frameBuffer[srcIndex] !=
				recPlusOverlayBackup[srcIndex];
		}
	}

	memcpy(video.frameBuffer, recPlusOverlayBackup,
		SCREEN_W * SCREEN_H * sizeof (uint32_t));

	const int32_t scaledW = glyphW * scale;
	const int32_t scaledH = glyphH * scale;

	/* Center the complete phrase horizontally and vertically. */
	const int32_t dstX = (SCREEN_W - scaledW) / 2;
	const int32_t dstY = (SCREEN_H - scaledH) / 2;

	for (int32_t y = 0; y < glyphH; y++)
	{
		for (int32_t x = 0; x < glyphW; x++)
		{
			const int32_t glyphIndex = (y * maxGlyphW) + x;
			if (!glyphMask[glyphIndex])
				continue;

			const uint32_t srcColor = glyphPixels[glyphIndex];
			const uint32_t sr = RGB32_R(srcColor);
			const uint32_t sg = RGB32_G(srcColor);
			const uint32_t sb = RGB32_B(srcColor);

			for (int32_t sy = 0; sy < scale; sy++)
			{
				const int32_t py = dstY + (y * scale) + sy;
				if (py < 0 || py >= SCREEN_H)
					continue;

				for (int32_t sx = 0; sx < scale; sx++)
				{
					const int32_t px = dstX + (x * scale) + sx;
					if (px < 0 || px >= SCREEN_W)
						continue;

					const int32_t dstIndex = (py * SCREEN_W) + px;
					const uint32_t bgColor =
						recPlusOverlayBackup[dstIndex];

					const uint32_t br = RGB32_R(bgColor);
					const uint32_t bg = RGB32_G(bgColor);
					const uint32_t bb = RGB32_B(bgColor);

					const uint32_t outR =
						((sr * alpha) +
						(br * (255 - alpha))) / 255;

					const uint32_t outG =
						((sg * alpha) +
						(bg * (255 - alpha))) / 255;

					const uint32_t outB =
						((sb * alpha) +
						(bb * (255 - alpha))) / 255;

					video.frameBuffer[dstIndex] =
						RGB32(outR, outG, outB);
				}
			}
		}
	}

	recPlusOverlayFrames--;

	if (recPlusOverlayFrames == 0)
	{
		memcpy(video.frameBuffer, recPlusOverlayBackup,
			SCREEN_W * SCREEN_H * sizeof (uint32_t));
	}
}

void endFPSCounter(void)
{
	if (video.showFPSCounter && frameStartTime > 0)
		runningFrameDuration += SDL_GetPerformanceCounter() - frameStartTime;
}

/*
** Tapehead HD keeps the original 632x400 drawing surface and mouse map intact,
** then reconstructs the finished frame at a higher native resolution.
**
** "Round" is the original Scale2x/Scale3x proof-of-concept filter. "Crisp"
** begins with exact integer expansion, then changes only a single physical
** corner pixel when two matching source neighbors prove that a diagonal is
** present. Unlike the discarded sharp experiment, it never thins straight
** strokes or borrows an arbitrary color from one side of an exposed corner.
**
** This is intentionally a presentation layer. It does not change any FT2 UI
** coordinates, hitboxes, pattern rendering, replay code or editor behavior.
*/
static void scale2xFrameBuffer(const tapeheadVideoDamageRect_t *rect)
{
	tapeheadScale2xRoundRegion(video.frameBuffer, SCREEN_W, SCREEN_H,
		video.presentBuffer, rect);
}

static void scale3xFrameBuffer(const tapeheadVideoDamageRect_t *rect)
{
	tapeheadScale3xRoundRegion(video.frameBuffer, SCREEN_W, SCREEN_H,
		video.presentBuffer, rect);
}

static void scale2xCrispFrameBuffer(const tapeheadVideoDamageRect_t *rect)
{
	tapeheadScale2xCrispRegion(video.frameBuffer, SCREEN_W, SCREEN_H,
		video.presentBuffer, rect);
}

static void scale3xCrispFrameBuffer(const tapeheadVideoDamageRect_t *rect)
{
	tapeheadScale3xCrispRegion(video.frameBuffer, SCREEN_W, SCREEN_H,
		video.presentBuffer, rect);
}

static void prepareFrameForPresentation(const tapeheadVideoDamageRect_t *rect)
{
	if (!video.hdRendererActive || video.presentBuffer == NULL)
		return;

	if (video.hdStyle == TAPEHEAD_HD_STYLE_ROUND)
	{
		if (video.hdScale == 2)
			scale2xFrameBuffer(rect);
		else
			scale3xFrameBuffer(rect);
	}
	else
	{
		if (video.hdScale == 2)
			scale2xCrispFrameBuffer(rect);
		else
			scale3xCrispFrameBuffer(rect);
	}
}

static void uploadChangedFrame(void)
{
	tapeheadVideoDamagePlan_t damage;
	const uint32_t *previous = video.frameSnapshotValid
		? video.frameSnapshot : NULL;
	if (!tapeheadVideoDamagePlan(video.frameBuffer, previous,
		SCREEN_W, SCREEN_H, &damage))
	{
		return;
	}

	const int32_t scale = video.hdRendererActive ? video.hdScale : 1;
	for (uint16_t i = 0; i < damage.count; i++)
	{
		const tapeheadVideoDamageRect_t *sourceRect = &damage.rects[i];
		prepareFrameForPresentation(sourceRect);

		SDL_Rect textureRect;
		textureRect.x = sourceRect->x * scale;
		textureRect.y = sourceRect->y * scale;
		textureRect.w = sourceRect->w * scale;
		textureRect.h = sourceRect->h * scale;

		const uint32_t *pixels;
		if (video.hdRendererActive)
		{
			pixels = &video.presentBuffer[
				(textureRect.y * video.textureW) + textureRect.x];
		}
		else
		{
			pixels = &video.frameBuffer[
				(sourceRect->y * SCREEN_W) + sourceRect->x];
		}

		if (SDL_UpdateTexture(video.texture, &textureRect, pixels,
			video.textureW * sizeof (uint32_t)) < 0)
		{
			/* Retry the complete frame next time. A device reset or transient
			** backend error must never leave the cached snapshot authoritative. */
			video.frameSnapshotValid = false;
			return;
		}
	}

	/* Keep the comparison snapshot incremental too. Copying the complete
	** 632x400 surface here would give back part of the memory-bandwidth saving,
	** especially in the normal 1x renderer. */
	for (uint16_t i = 0; i < damage.count; i++)
	{
		const tapeheadVideoDamageRect_t *rect = &damage.rects[i];
		for (int32_t y = rect->y; y < rect->y + rect->h; y++)
		{
			const int32_t offset = (y * SCREEN_W) + rect->x;
			memcpy(&video.frameSnapshot[offset], &video.frameBuffer[offset],
				(size_t)rect->w * sizeof (uint32_t));
		}
	}
	video.frameSnapshotValid = true;
}

void flipFrame(void)
{
	const uint32_t windowFlags = SDL_GetWindowFlags(video.window);
	bool minimized = (windowFlags & SDL_WINDOW_MINIMIZED) ? true : false;

	renderSprites();

	if (video.showFPSCounter)
		drawFPSCounter();

	drawRecPlusOverlay();

	if (!minimized)
	{
		uploadChangedFrame();

		/* A full-destination copy replaces every output pixel, so clearing it
		** first is redundant. Centered fullscreen needs the clear for borders. */
		if (video.useCustomRenderRect)
			SDL_RenderClear(video.renderer);

		if (video.useCustomRenderRect)
			SDL_RenderCopy(video.renderer, video.texture, NULL, &video.renderRect);
		else
			SDL_RenderCopy(video.renderer, video.texture, NULL, NULL);

		SDL_RenderPresent(video.renderer);
	}

	eraseSprites();

	if (!video.vsync60HzPresent)
	{
		// we have no VSync, do crude thread sleeping to sync to ~60Hz
		hpc_Wait(&video.vblankHpc);
	}
	else
	{
		/* We have VSync, but it can unexpectedly get inactive in certain scenarios.
		** We have to force thread sleeping (to ~60Hz) if so.
		*/
#ifdef __APPLE__
		// macOS: VSync gets disabled if the window is 100% covered by another window. Let's add a (crude) fix:
		if (minimized || !(windowFlags & SDL_WINDOW_INPUT_FOCUS))
			hpc_Wait(&video.vblankHpc);
#elif __unix__
		/* *NIX: VSync can get disabled in fullscreen mode in some distros/systems. Let's add a fix.
		**
		** TODO/XXX: This is probably a BAD hack and can cause a poor fullscreen experience if VSync did
		**           in fact work in fullscreen mode...
		*/
		if (minimized || video.fullscreen)
			hpc_Wait(&video.vblankHpc);
#else
		if (minimized)
			hpc_Wait(&video.vblankHpc);
#endif
	}

	editor.framesPassed++;

	/* Reset audio/video sync timestamp every half an hour to prevent
	** possible sync drifting after hours of playing a song without
	** a single song stop (resets timestamp) in-between.
	*/
	if (editor.framesPassed >= VBLANK_HZ*60*30)
		audio.resetSyncTickTimeFlag = true;
}

void showErrorMsgBox(const char *fmt, ...)
{
	char strBuf[512+1];
	va_list args;

	// format the text string
	va_start(args, fmt);
	vsnprintf(strBuf, sizeof (strBuf)-1, fmt, args);
	va_end(args);

	// SDL message boxes can be very buggy on Windows XP, use MessageBoxA() instead
#ifdef _WIN32
	MessageBoxA(NULL, strBuf, "Error", MB_OK | MB_ICONERROR);
#else
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", strBuf, NULL);
#endif
}

static void updateRenderSizeVars(void)
{
	int32_t widthInPixels, heightInPixels;
	SDL_DisplayMode dm;

	int32_t di = SDL_GetWindowDisplayIndex(video.window);
	if (di < 0)
		di = 0; // return display index 0 (default) on error

	SDL_GetDesktopDisplayMode(di, &dm);
	video.displayW = dm.w;
	video.displayH = dm.h;

	SDL_GetWindowSize(video.window, &video.windowW, &video.windowH);
	video.renderX = 0;
	video.renderY = 0;

	video.useCustomRenderRect = false;

	if (video.fullscreen)
	{
		if (config.specialFlags2 & STRETCH_IMAGE)
		{
			// "streched out" windowed fullscreen

			video.renderW = video.windowW;
			video.renderH = video.windowH;

			// get DPI zoom factors (Macs with Retina, etc... returns 1.0 if no zoom)
			SDL_GL_GetDrawableSize(video.window, &widthInPixels, &heightInPixels);
			video.dDpiZoomFactorX = (double)widthInPixels / video.windowW;
			video.dDpiZoomFactorY = (double)heightInPixels / video.windowH;
		}
		else
		{
			// centered windowed fullscreen, with pixel-perfect integer upscaling

			const int32_t maxUpscaleFactor = MIN(video.windowW / SCREEN_W, video.windowH / SCREEN_H);
			video.renderW = SCREEN_W * maxUpscaleFactor;
			video.renderH = SCREEN_H * maxUpscaleFactor;
			video.renderX = (video.windowW - video.renderW) / 2;
			video.renderY = (video.windowH - video.renderH) / 2;

			// get DPI zoom factors (Macs with Retina, etc... returns 1.0 if no zoom)
			SDL_GL_GetDrawableSize(video.window, &widthInPixels, &heightInPixels);
			video.dDpiZoomFactorX = (double)widthInPixels / video.windowW;
			video.dDpiZoomFactorY = (double)heightInPixels / video.windowH;

			video.renderRect.x = (int32_t)floor(video.renderX * video.dDpiZoomFactorX);
			video.renderRect.y = (int32_t)floor(video.renderY * video.dDpiZoomFactorY);
			video.renderRect.w = (int32_t)floor(video.renderW * video.dDpiZoomFactorX);
			video.renderRect.h = (int32_t)floor(video.renderH * video.dDpiZoomFactorY);
			video.useCustomRenderRect = true; // use the destination coordinates above in SDL_RenderCopy()
		}
	}
	else
	{
		// windowed mode

		SDL_GetWindowSize(video.window, &video.renderW, &video.renderH);

		// get DPI zoom factors (Macs with Retina, etc... returns 1.0 if no zoom)
		SDL_GL_GetDrawableSize(video.window, &widthInPixels, &heightInPixels);
		video.dDpiZoomFactorX = (double)widthInPixels / video.windowW;
		video.dDpiZoomFactorY = (double)heightInPixels / video.windowH;
	}

	// "hardware mouse" calculations
	video.mouseCursorUpscaleFactor = MIN(video.renderW / SCREEN_W, video.renderH / SCREEN_H);
	createMouseCursors();
}

void enterFullscreen(void)
{
	SDL_SetWindowFullscreen(video.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	SDL_Delay(15); // fixes possible issues

	updateRenderSizeVars();
	updateMouseScaling();
	setMousePosToCenter();
}

void leaveFullscreen(void)
{
	SDL_SetWindowFullscreen(video.window, 0);
	SDL_Delay(15); // fixes possible issues

	setWindowSizeFromConfig(false); // false = do not change actual window size, only update variables
	SDL_SetWindowSize(video.window, SCREEN_W * video.windowModeUpscaleFactor, SCREEN_H * video.windowModeUpscaleFactor);

	updateRenderSizeVars();
	updateMouseScaling();
	setMousePosToCenter();

#ifdef __unix__ // can be required on Linux... (or else the window keeps moving down every time you leave fullscreen)
	SDL_SetWindowPosition(video.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
#endif
}

void toggleFullscreen(void)
{
	video.fullscreen ^= 1;

	if (video.fullscreen)
		enterFullscreen();
	else
		leaveFullscreen();
}

bool setupSprites(void)
{
	sprite_t *s;

	memset(sprites, 0, sizeof (sprites));

	// hide sprites
	s = sprites;
	for (int32_t i = 0; i < SPRITE_NUM; i++, s++)
		s->x = s->y = INT16_MAX;

	s = &sprites[SPRITE_MOUSE_POINTER];
	s->data = bmp.mouseCursors;
	s->w = MOUSE_CURSOR_W;
	s->h = MOUSE_CURSOR_H;

	s = &sprites[SPRITE_LEFT_LOOP_PIN];
	s->data = &bmp.loopPins[0*(154*16)];
	s->w = 16;
	s->h = SAMPLE_AREA_HEIGHT;

	s = &sprites[SPRITE_RIGHT_LOOP_PIN];
	s->data = &bmp.loopPins[2*(154*16)];
	s->w = 16;
	s->h = SAMPLE_AREA_HEIGHT;

	s = &sprites[SPRITE_TEXT_CURSOR];
	s->data = textCursorData;
	s->w = 1;
	s->h = 12;

	hideSprite(SPRITE_MOUSE_POINTER);
	hideSprite(SPRITE_LEFT_LOOP_PIN);
	hideSprite(SPRITE_RIGHT_LOOP_PIN);
	hideSprite(SPRITE_TEXT_CURSOR);

	// setup refresh buffer (used to clear sprites after each frame)
	s = sprites;
	for (uint32_t i = 0; i < SPRITE_NUM; i++, s++)
	{
		s->refreshBuffer = (uint32_t *)malloc(s->w * s->h * sizeof (int32_t));
		if (s->refreshBuffer == NULL)
			return false;
	}

	return true;
}

void changeSpriteData(int32_t sprite, const uint8_t *data)
{
	sprites[sprite].data = data;
	memset(sprites[sprite].refreshBuffer, 0, sprites[sprite].w * sprites[sprite].h * sizeof (int32_t));
}

void freeSprites(void)
{
	sprite_t *s = sprites;
	for (int32_t i = 0; i < SPRITE_NUM; i++, s++)
	{
		if (s->refreshBuffer != NULL)
		{
			free(s->refreshBuffer);
			s->refreshBuffer = NULL;
		}
	}
}

void setLeftLoopPinState(bool clicked)
{
	changeSpriteData(SPRITE_LEFT_LOOP_PIN, clicked ? &bmp.loopPins[1*(154*16)] : &bmp.loopPins[0*(154*16)]);
}

void setRightLoopPinState(bool clicked)
{
	changeSpriteData(SPRITE_RIGHT_LOOP_PIN, clicked ? &bmp.loopPins[3*(154*16)] : &bmp.loopPins[2*(154*16)]);
}

int32_t getSpritePosX(int32_t sprite)
{
	return sprites[sprite].x;
}

void setSpritePos(int32_t sprite, int32_t x, int32_t y)
{
	sprites[sprite].newX = (int16_t)x;
	sprites[sprite].newY = (int16_t)y;
}

void hideSprite(int32_t sprite)
{
	sprites[sprite].newX = SCREEN_W;
}

void eraseSprites(void)
{
	sprite_t *s = &sprites[SPRITE_NUM-1];
	for (int32_t i = SPRITE_NUM-1; i >= 0; i--, s--) // erasing must be done in reverse order
	{
		if (s->x >= SCREEN_W || s->y >= SCREEN_H) // sprite is hidden, don't draw nor fill clear buffer
			continue;

		ASSERT(s->refreshBuffer != NULL);

		int32_t sw = s->w;
		int32_t sh = s->h;
		int32_t sx = s->x;
		int32_t sy = s->y;

		// if x is negative, adjust variables
		if (sx < 0)
		{
			sw += sx; // subtraction
			sx = 0;
		}

		// if y is negative, adjust variables
		if (sy < 0)
		{
			sh += sy; // subtraction
			sy = 0;
		}

		const uint32_t *src32 = s->refreshBuffer;
		uint32_t *dst32 = &video.frameBuffer[(sy * SCREEN_W) + sx];

		// handle x/y clipping
		if (sx+sw >= SCREEN_W) sw = SCREEN_W - sx;
		if (sy+sh >= SCREEN_H) sh = SCREEN_H - sy;

		const int32_t srcPitch = s->w - sw;
		const int32_t dstPitch = SCREEN_W - sw;

		for (int32_t y = 0; y < sh; y++)
		{
			for (int32_t x = 0; x < sw; x++)
				*dst32++ = *src32++;

			src32 += srcPitch;
			dst32 += dstPitch;
		}
	}
}

void renderSprites(void)
{
	sprite_t *s = sprites;
	for (int32_t i = 0; i < SPRITE_NUM; i++, s++)
	{
		if (i == SPRITE_LEFT_LOOP_PIN || i == SPRITE_RIGHT_LOOP_PIN)
			continue; // these need special drawing (done elsewhere)

		// don't render the text edit cursor if window is inactive
		if (i == SPRITE_TEXT_CURSOR)
		{
			ASSERT(video.window != NULL);
			const uint32_t windowFlags = SDL_GetWindowFlags(video.window);
			if (!(windowFlags & SDL_WINDOW_INPUT_FOCUS))
				continue;
		}

		// set new sprite position
		s->x = s->newX;
		s->y = s->newY;

		if (s->x >= SCREEN_W || s->y >= SCREEN_H) // sprite is hidden, don't draw nor fill clear buffer
			continue;

		ASSERT(s->data != NULL && s->refreshBuffer != NULL);

		int32_t sw = s->w;
		int32_t sh = s->h;
		int32_t sx = s->x;
		int32_t sy = s->y;
		const uint8_t *src8 = s->data;

		// if x is negative, adjust variables
		if (sx < 0)
		{
			sw += sx; // subtraction
			src8 -= sx; // addition
			sx = 0;
		}

		// if y is negative, adjust variables
		if (sy < 0)
		{
			sh += sy; // subtraction
			src8 += (-sy * s->w); // addition
			sy = 0;
		}

		if (sw <= 0 || sh <= 0) // sprite is hidden, don't draw nor fill clear buffer
			continue;

		uint32_t *dst32 = &video.frameBuffer[(sy * SCREEN_W) + sx];
		uint32_t *clr32 = s->refreshBuffer;

		// handle x/y clipping
		if (sx+sw >= SCREEN_W) sw = SCREEN_W - sx;
		if (sy+sh >= SCREEN_H) sh = SCREEN_H - sy;

		const int32_t srcPitch = s->w - sw;
		const int32_t dstPitch = SCREEN_W - sw;

		if (mouse.mouseOverTextBox && i == SPRITE_MOUSE_POINTER)
		{
			// text edit mouse pointer (has color changing depending on content under it)
			for (int32_t y = 0; y < sh; y++)
			{
				for (int32_t x = 0; x < sw; x++)
				{
					*clr32++ = *dst32; // fill clear buffer

					if (*src8 != PAL_TRANSPR)
					{
						if (!(*dst32 & 0x00FFFFFF) || *dst32 == video.palette[PAL_TEXTMRK])
							*dst32 = 0xB3DBF6;
						else
							*dst32 = 0x004ECE;
					}

					dst32++;
					src8++;
				}

				clr32 += srcPitch;
				src8 += srcPitch;
				dst32 += dstPitch;
			}
		}
		else
		{
			// normal sprites
			for (int32_t y = 0; y < sh; y++)
			{
				for (int32_t x = 0; x < sw; x++)
				{
					*clr32++ = *dst32; // fill clear buffer

					if (*src8 != PAL_TRANSPR)
					{
						ASSERT(*src8 < PAL_NUM);
						*dst32 = video.palette[*src8];
					}

					dst32++;
					src8++;
				}

				clr32 += srcPitch;
				src8 += srcPitch;
				dst32 += dstPitch;
			}
		}
	}
}

void renderLoopPins(void)
{
	const uint8_t *src8;
	int32_t sx, x, y, sw, sh, srcPitch, dstPitch;
	uint32_t *clr32, *dst32;

	// left loop pin

	sprite_t *s = &sprites[SPRITE_LEFT_LOOP_PIN];
	ASSERT(s->data != NULL && s->refreshBuffer != NULL);

	// set new sprite position
	s->x = s->newX;
	s->y = s->newY;

	if (s->x < SCREEN_W) // loop pin shown?
	{
		sw = s->w;
		sh = s->h;
		sx = s->x;

		src8 = s->data;
		clr32 = s->refreshBuffer;

		// if x is negative, adjust variables
		if (sx < 0)
		{
			sw += sx; // subtraction
			src8 -= sx; // addition
			sx = 0;
		}

		dst32 = &video.frameBuffer[(s->y * SCREEN_W) + sx];

		// handle x clipping
		if (sx+sw >= SCREEN_W) sw = SCREEN_W - sx;

		srcPitch = s->w - sw;
		dstPitch = SCREEN_W - sw;

		for (y = 0; y < sh; y++)
		{
			for (x = 0; x < sw; x++)
			{
				*clr32++ = *dst32; // fill clear buffer

				if (*src8 != PAL_TRANSPR)
				{
					ASSERT(*src8 < PAL_NUM);
					*dst32 = video.palette[*src8];
				}

				dst32++;
				src8++;
			}

			src8 += srcPitch;
			clr32 += srcPitch;
			dst32 += dstPitch;
		}
	}

	// right loop pin

	s = &sprites[SPRITE_RIGHT_LOOP_PIN];
	ASSERT(s->data != NULL && s->refreshBuffer != NULL);

	// set new sprite position
	s->x = s->newX;
	s->y = s->newY;

	if (s->x < SCREEN_W) // loop pin shown?
	{
		s->x = s->newX;
		s->y = s->newY;

		sw = s->w;
		sh = s->h;
		sx = s->x;

		src8 = s->data;
		clr32 = s->refreshBuffer;

		// if x is negative, adjust variables
		if (sx < 0)
		{
			sw += sx; // subtraction
			src8 -= sx; // addition
			sx = 0;
		}

		dst32 = &video.frameBuffer[(s->y * SCREEN_W) + sx];

		// handle x clipping
		if (sx+sw >= SCREEN_W) sw = SCREEN_W - sx;

		srcPitch = s->w - sw;
		dstPitch = SCREEN_W - sw;

		for (y = 0; y < sh; y++)
		{
			for (x = 0; x < sw; x++)
			{
				*clr32++ = *dst32;

				if (*src8 != PAL_TRANSPR)
				{
					ASSERT(*src8 < PAL_NUM);
					if (y < 9 && *src8 == PAL_LOOPPIN)
					{
						// don't draw marker line on top of left loop pin's thumb graphics
						const uint8_t pal = *dst32 >> 24;
						if (pal != PAL_DESKTOP && pal != PAL_DSKTOP1 && pal != PAL_DSKTOP2)
							*dst32 = video.palette[*src8];
					}
					else
					{
						*dst32 = video.palette[*src8];
					}
				}

				dst32++;
				src8++;
			}

			src8 += srcPitch;
			clr32 += srcPitch;
			dst32 += dstPitch;
		}
	}
}

void closeVideo(void)
{
	if (video.texture != NULL)
	{
		SDL_DestroyTexture(video.texture);
		video.texture = NULL;
	}

	if (video.renderer != NULL)
	{
		SDL_DestroyRenderer(video.renderer);
		video.renderer = NULL;
	}

	if (video.window != NULL)
	{
		SDL_DestroyWindow(video.window);
		video.window = NULL;
	}

	if (video.frameBuffer != NULL)
	{
		free(video.frameBuffer);
		video.frameBuffer = NULL;
	}

	if (video.presentBuffer != NULL)
	{
		free(video.presentBuffer);
		video.presentBuffer = NULL;
	}

	if (video.frameSnapshot != NULL)
	{
		free(video.frameSnapshot);
		video.frameSnapshot = NULL;
		video.frameSnapshotValid = false;
	}

	if (recPlusOverlayBackup != NULL)
	{
		free(recPlusOverlayBackup);
		recPlusOverlayBackup = NULL;
		recPlusOverlayFrames = 0;
	}
}

void setWindowSizeFromConfig(bool updateRenderer)
{
#define MAX_UPSCALE_FACTOR 16 // 10112x6400 - ought to be good enough for many years to come

	uint8_t i;
	SDL_DisplayMode dm;

	uint8_t oldUpscaleFactor = video.windowModeUpscaleFactor;
	if (video.hdRendererActive)
	{
		video.windowModeUpscaleFactor = video.hdScale;

		int32_t di = SDL_GetWindowDisplayIndex(video.window);
		if (di < 0)
			di = 0;

		/* Never let an experimental HD setting strand the user with a window
		** larger than the desktop. Keep lowering the physical window factor while
		** retaining the higher-resolution texture internally.
		*/
		if (SDL_GetDesktopDisplayMode(di, &dm) == 0)
		{
			while (video.windowModeUpscaleFactor > 1 &&
			      (dm.w < (SCREEN_W * video.windowModeUpscaleFactor) + 32 ||
			       dm.h < (SCREEN_H * video.windowModeUpscaleFactor) + 96))
			{
				video.windowModeUpscaleFactor--;
			}
		}
	}
	else if (config.windowFlags & WINSIZE_AUTO)
	{
		int32_t di = SDL_GetWindowDisplayIndex(video.window);
		if (di < 0)
			di = 0; // return display index 0 (default) on error

		// find out which upscaling factor is the biggest to fit on screen
		if (SDL_GetDesktopDisplayMode(di, &dm) == 0)
		{
			for (i = MAX_UPSCALE_FACTOR; i >= 1; i--)
			{
				// height test is slightly taller because of window title, window borders and taskbar/menu/dock
				if (dm.w >= SCREEN_W*i && dm.h >= (SCREEN_H+64)*i)
				{
					video.windowModeUpscaleFactor = i;
					break;
				}
			}

			if (i == 0)
				video.windowModeUpscaleFactor = 1; // 1x is not going to fit, but use 1x anyways...
		}
		else
		{
			// couldn't get screen resolution, set to 1x
			video.windowModeUpscaleFactor = 1;
		}
	}
	else if (config.windowFlags & WINSIZE_1X) video.windowModeUpscaleFactor = 1;
	else if (config.windowFlags & WINSIZE_2X) video.windowModeUpscaleFactor = 2;
	else if (config.windowFlags & WINSIZE_3X) video.windowModeUpscaleFactor = 3;
	else if (config.windowFlags & WINSIZE_4X) video.windowModeUpscaleFactor = 4;

	if (updateRenderer)
	{
		SDL_SetWindowSize(video.window, SCREEN_W * video.windowModeUpscaleFactor, SCREEN_H * video.windowModeUpscaleFactor);

		if (oldUpscaleFactor != video.windowModeUpscaleFactor)
			SDL_SetWindowPosition(video.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

		updateRenderSizeVars();
		updateMouseScaling();
		setMousePosToCenter();
	}
}

void updateWindowTitle(bool forceUpdate)
{
	if (!forceUpdate && songIsModified == song.isModified)
		return; // window title is already set to the same

	char *songTitle = getCurrSongFilename();
	if (songTitle != NULL)
	{
		char songTitleTrunc[128];
		strncpy(songTitleTrunc, songTitle, sizeof (songTitleTrunc)-1);
		songTitleTrunc[sizeof (songTitleTrunc)-1] = '\0';

			if (song.isModified)
				sprintf(wndTitle, "Tapehead v%s - \"%s\" (unsaved)", PROG_VER_STR, songTitleTrunc);
			else
				sprintf(wndTitle, "Tapehead v%s - \"%s\"", PROG_VER_STR, songTitleTrunc);
	}
	else
	{
		if (song.isModified)
			sprintf(wndTitle, "Tapehead v%s - \"untitled\" (unsaved)", PROG_VER_STR);
		else
			sprintf(wndTitle, "Tapehead v%s - \"untitled\"", PROG_VER_STR);
	}

	if (audio.outputDeviceLost)
		strncat(wndTitle, " [AUDIO DISCONNECTED]",
			sizeof (wndTitle) - strlen(wndTitle) - 1);

	SDL_SetWindowTitle(video.window, wndTitle);
	songIsModified = song.isModified;
}

bool recreateTexture(void)
{
	if (video.texture != NULL)
	{
		SDL_DestroyTexture(video.texture);
		video.texture = NULL;
	}

	if (!video.hdRendererActive && (config.windowFlags & PIXEL_FILTER))
		SDL_SetHint("SDL_RENDER_SCALE_QUALITY", "best");
	else
		SDL_SetHint("SDL_RENDER_SCALE_QUALITY", "nearest");

	// SDL_PIXELFORMAT_ARGB8888 is the fastest mode when using texture streaming
	video.texture = SDL_CreateTexture(video.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		video.textureW, video.textureH);
	if (video.texture == NULL)
	{
		showErrorMsgBox("Couldn't create a %dx%d GPU texture:\n\"%s\"\n\nIs your GPU (+ driver) too old?",
			video.textureW, video.textureH, SDL_GetError());
		return false;
	}

	// disable alpha blending as we store the palette number in the MSB (0xXX000000)
	SDL_SetTextureBlendMode(video.texture, SDL_BLENDMODE_NONE);
	video.frameSnapshotValid = false;
	return true;
}

bool setupWindow(void)
{
	SDL_DisplayMode dm;

	video.vsync60HzPresent = false;
	video.hdRendererActive = tapeheadConfig.hdMode;
	video.hdScale = (tapeheadConfig.hdScale == 2) ? 2 : 3;
	video.hdStyle = tapeheadConfig.hdStyle;
	video.textureW = SCREEN_W * (video.hdRendererActive ? video.hdScale : 1);
	video.textureH = SCREEN_H * (video.hdRendererActive ? video.hdScale : 1);

	uint32_t windowFlags = SDL_WINDOW_ALLOW_HIGHDPI;
#if defined (__APPLE__) || defined (_WIN32) // yet another quirk!
	windowFlags |= SDL_WINDOW_HIDDEN;
#endif

	setWindowSizeFromConfig(false);

	int32_t di = SDL_GetWindowDisplayIndex(video.window);
	if (di < 0)
		di = 0; // return display index 0 (default) on error

	SDL_GetDesktopDisplayMode(di, &dm);
	video.dMonitorRefreshRate = (double)dm.refresh_rate;

#ifndef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
// older SDL2 versions don't define this, don't fail the build for it
#define SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR "SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR"
#endif
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

	if (dm.refresh_rate >= 59 && dm.refresh_rate <= 61)
		video.vsync60HzPresent = true;

	if (config.windowFlags & FORCE_VSYNC_OFF)
		video.vsync60HzPresent = false;

	video.window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		SCREEN_W * video.windowModeUpscaleFactor, SCREEN_H * video.windowModeUpscaleFactor,
		windowFlags);

	if (video.window == NULL)
	{
		showErrorMsgBox("Couldn't create SDL window:\n%s", SDL_GetError());
		return false;
	}

#if defined _WIN32 || defined __linux__
	setTapeheadWindowIcon();
#endif

#ifdef __APPLE__ // for macOS we need to do this here for reasons I have forgotten
	SDL_PumpEvents();
	SDL_ShowWindow(video.window);
#endif

	updateWindowTitle(true);
	return true;
}

bool setupRenderer(void)
{
	uint32_t rendererFlags = 0;
	if (video.vsync60HzPresent)
		rendererFlags |= SDL_RENDERER_PRESENTVSYNC;

	video.renderer = SDL_CreateRenderer(video.window, -1, rendererFlags);
	if (video.renderer == NULL)
	{
		if (video.vsync60HzPresent)
		{
			// try again without vsync flag
			video.vsync60HzPresent = false;

			rendererFlags &= ~SDL_RENDERER_PRESENTVSYNC;
			video.renderer = SDL_CreateRenderer(video.window, -1, rendererFlags);
		}

		if (video.renderer == NULL)
		{
			showErrorMsgBox("Couldn't create SDL renderer:\n\"%s\"\n\nIs your GPU (+ driver) too old?",
				SDL_GetError());
			return false;
		}
	}

	SDL_SetRenderDrawBlendMode(video.renderer, SDL_BLENDMODE_NONE);

	if (!recreateTexture())
	{
		showErrorMsgBox("Couldn't create a %dx%d GPU texture:\n\"%s\"\n\nIs your GPU (+ driver) too old?",
			video.textureW, video.textureH, SDL_GetError());
		return false;
	}

	// framebuffer used by SDL (for texture)
	video.frameBuffer = (uint32_t *)malloc(SCREEN_W * SCREEN_H * sizeof (uint32_t));
	if (video.frameBuffer == NULL)
	{
		showErrorMsgBox("Not enough memory!");
		return false;
	}

	video.frameSnapshot = (uint32_t *)malloc(
		SCREEN_W * SCREEN_H * sizeof (uint32_t));
	if (video.frameSnapshot == NULL)
	{
		showErrorMsgBox("Not enough memory for the video damage snapshot!");
		return false;
	}
	video.frameSnapshotValid = false;

	if (video.hdRendererActive)
	{
		video.presentBuffer = (uint32_t *)malloc((size_t)video.textureW * video.textureH * sizeof (uint32_t));
		if (video.presentBuffer == NULL)
		{
			showErrorMsgBox("Not enough memory for the Tapehead HD framebuffer!");
			return false;
		}
	}

	if (!setupSprites())
		return false;

	updateRenderSizeVars();
	updateMouseScaling();

	if (config.specialFlags2 & HARDWARE_MOUSE)
		SDL_ShowCursor(SDL_TRUE);
	else
		SDL_ShowCursor(SDL_FALSE);

	SDL_SetRenderDrawColor(video.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);

	dFrameDurationDiv = (1000.0 * FPS_SCAN_FRAMES) / hpcFreq.dFreqMulMs;
	return true;
}

void handleRedrawing(void)
{
	updatePatternCursorBlink();

	if (!ui.configScreenShown && !ui.helpScreenShown)
	{
		if (ui.aboutScreenShown)
		{
			renderAboutScreenFrame();
		}
		else if (ui.nibblesShown)
		{
			if (editor.NI_Play)
				moveNibblesPlayers();
		}
		else
		{
			if (ui.updatePosSections)
			{
				ui.updatePosSections = false;

				if (!ui.diskOpShown)
				{
					drawSongLoopStart();
					drawSongLength();
					drawPosEdNums(editor.songPos);
					drawEditPattern(editor.editPattern);
					drawPatternLength(editor.editPattern);
					drawSongBPM(editor.BPM);
					drawSongSpeed(editor.speed);
					drawGlobalVol(editor.globalVolume);

					if (!songPlaying || editor.wavIsRendering)
						setScrollBarPos(SB_POS_ED, editor.songPos, DONT_TRIGGER_CALLBACK);

					// draw current mode text

					int16_t auditionNotes[3];
					char auditionNoteText[12];
					const int16_t auditionNoteCount = getLowestAuditionNotes(auditionNotes, 3);

					const char *str = NULL;
					if (auditionNoteCount > 0)
					{
						formatAuditionNote(auditionNoteText, auditionNotes[0]);

						if (auditionNoteCount > 1)
						{
							auditionNoteText[3] = ' ';
							formatAuditionNote(&auditionNoteText[4], auditionNotes[1]);

							if (auditionNoteCount > 2)
							{
								auditionNoteText[7] = ' ';
								formatAuditionNote(&auditionNoteText[8], auditionNotes[2]);
							}
						}

						str = auditionNoteText;
					}
					else if (playMode == PLAYMODE_PATT)    str = "> Play ptn. <";
					else if (playMode == PLAYMODE_EDIT)    str = "> Editing <";
					else if (playMode == PLAYMODE_RECSONG) str = "> Rec. sng. <";
					else if (playMode == PLAYMODE_RECPATT) str = "> Rec. ptn. <";

					uint16_t areaWidth = 78;
					uint16_t maxStrWidth = 76; // wide enough
					uint16_t x = 125;
					uint16_t y = 80;

					if (ui.extendedPatternEditor)
					{
						y = 56;
						areaWidth = 443;
					}
					if (!ui.patternEditorOnly)
					{
						// silent recording indicator
						fillRect(101, y, 24, FONT1_CHAR_H+1, PAL_DESKTOP);

						if (config.specialFlags2 & SILENT_REC_ENTRY)
							textOut(108, y, PAL_FORGRND, "SR");

						// clear area
						const uint16_t clrX = x + ((areaWidth - maxStrWidth) / 2);
						fillRect(clrX, y, maxStrWidth, FONT1_CHAR_H+1, PAL_DESKTOP);

						// draw text (if needed)
						if (str != NULL)
						{
							x += (areaWidth - textWidth(str)) / 2;
							textOut(x, y, PAL_FORGRND, str);
						}
					}
				}
			}

			if (ui.updatePosEdScrollBar)
			{
				ui.updatePosEdScrollBar = false;
				setScrollBarPos(SB_POS_ED, song.songPos, DONT_TRIGGER_CALLBACK);
				setScrollBarEnd(SB_POS_ED, (song.songLength - 1) + 5);
			}

			if (!ui.diskOpShown)
				drawPlaybackTime();

			if (!ui.extendedPatternEditor)
			{
				if (ui.sampleEditorExtShown)
					handleSampleEditorExtRedrawing();
				else if (ui.scopesShown)
					drawScopes();
			}
		}
	}

	drawReplayerData();

	if (ui.instEditorShown)
		handleInstEditorRedrawing();
	else if (ui.sampleEditorShown)
		handleSamplerRedrawing();

	// blink text edit cursor
	if (editor.editTextFlag && mouse.lastEditBox != -1)
	{
		ASSERT(mouse.lastEditBox >= 0 && mouse.lastEditBox < NUM_TEXTBOXES);

		textBox_t *txt = &textBoxes[mouse.lastEditBox];
		if (editor.textCursorBlinkCounter < 256/2 && !textIsMarked() && !(mouse.leftButtonPressed | mouse.rightButtonPressed))
			setSpritePos(SPRITE_TEXT_CURSOR, getTextCursorX(txt), getTextCursorY(txt) - 1); // show text cursor
		else
			hideSprite(SPRITE_TEXT_CURSOR); // hide text cursor

		editor.textCursorBlinkCounter += TEXT_CURSOR_BLINK_RATE;
	}

	if (editor.busy)
		animateBusyMouse();

	renderLoopPins();
	instrumentTransformDrawPanel();
}

static void drawReplayerData(void)
{
	if (songPlaying)
	{
		if (ui.drawReplayerPianoFlag)
		{
			ui.drawReplayerPianoFlag = false;
			if (ui.instEditorShown && chSyncEntry != NULL)
				drawPiano(chSyncEntry);
		}

		bool drawPosText = true;
		if (ui.configScreenShown || ui.nibblesShown     ||
			ui.helpScreenShown   || ui.aboutScreenShown ||
			ui.diskOpShown)
		{
			drawPosText = false;
		}

		if (drawPosText)
		{
			if (ui.drawBPMFlag)
			{
				ui.drawBPMFlag = false;
				drawSongBPM(editor.BPM);
			}

			if (ui.drawSpeedFlag)
			{
				ui.drawSpeedFlag = false;
				drawSongSpeed(editor.speed);
			}

			if (ui.drawGlobVolFlag)
			{
				ui.drawGlobVolFlag = false;
				drawGlobalVol(editor.globalVolume);
			}

			if (ui.drawPosEdFlag)
			{
				ui.drawPosEdFlag = false;
				drawPosEdNums(editor.songPos);
				setScrollBarPos(SB_POS_ED, editor.songPos, DONT_TRIGGER_CALLBACK);
			}

			if (ui.drawPattNumLenFlag)
			{
				ui.drawPattNumLenFlag = false;
				drawEditPattern(editor.editPattern);
				drawPatternLength(editor.editPattern);
			}
		}
	}
	else if (ui.instEditorShown)
	{
		drawPiano(NULL);
	}

	// handle pattern data updates
	if (ui.updatePatternEditor)
	{
		ui.updatePatternEditor = false;
		if (ui.patternEditorShown)
			writePattern(editor.row, editor.editPattern);
	}
}
