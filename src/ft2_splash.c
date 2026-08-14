#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "third_party/stb_image.h"

#include "ft2_splash.h"
#include "ft2_video.h"

#define TAPEHEAD_SPLASH_FILENAME "tapeheadSplash.png"
#define TAPEHEAD_SPLASH_DURATION_MS 5000

static bool suppressedKeys[SDL_NUM_SCANCODES];
static uint32_t suppressedMouseButtons;

static SDL_Texture *loadSplashTexture(int32_t *width, int32_t *height)
{
	SDL_Texture *texture = NULL;
	unsigned char *pixels = NULL;
	char path[PATH_MAX + 1];
	int channels;

	char *basePath = SDL_GetBasePath();
	if (basePath != NULL)
	{
		const int32_t charsWritten = snprintf(path, sizeof (path), "%s%s",
			basePath, TAPEHEAD_SPLASH_FILENAME);
		if (charsWritten > 0 && charsWritten < (int32_t)sizeof (path))
			pixels = stbi_load(path, width, height, &channels, STBI_rgb_alpha);
		SDL_free(basePath);
	}

	/* Development-tree fallback; release builds package the asset by the binary. */
	if (pixels == NULL)
	{
		pixels = stbi_load("release/other/" TAPEHEAD_SPLASH_FILENAME,
			width, height, &channels, STBI_rgb_alpha);
	}

	if (pixels == NULL)
	{
		fprintf(stderr, "Tapehead splash skipped: %s\n", stbi_failure_reason());
		return NULL;
	}

	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels,
		*width, *height, 32, *width * 4, SDL_PIXELFORMAT_RGBA32);
	if (surface != NULL)
		texture = SDL_CreateTextureFromSurface(video.renderer, surface);

	if (texture == NULL)
		fprintf(stderr, "Tapehead splash skipped: %s\n", SDL_GetError());

	if (surface != NULL)
		SDL_FreeSurface(surface);
	stbi_image_free(pixels);
	return texture;
}

static SDL_Rect aspectFitRect(int32_t imageWidth, int32_t imageHeight,
	int32_t outputWidth, int32_t outputHeight)
{
	SDL_Rect destination = { 0, 0, outputWidth, outputHeight };
	if (imageWidth <= 0 || imageHeight <= 0 || outputWidth <= 0 || outputHeight <= 0)
		return destination;

	destination.h = (int32_t)(((int64_t)outputWidth * imageHeight) / imageWidth);
	if (destination.h > outputHeight)
	{
		destination.h = outputHeight;
		destination.w = (int32_t)(((int64_t)outputHeight * imageWidth) / imageHeight);
	}

	destination.x = (outputWidth - destination.w) / 2;
	destination.y = (outputHeight - destination.h) / 2;
	return destination;
}

static bool handleSplashEvent(const SDL_Event *event, bool *dismissed)
{
	if (event->type == SDL_QUIT)
		return false;

	if (event->type == SDL_KEYDOWN)
	{
		const SDL_Scancode scancode = event->key.keysym.scancode;
		if (scancode >= 0 && scancode < SDL_NUM_SCANCODES)
			suppressedKeys[scancode] = true;
		*dismissed = true;
	}
	else if (event->type == SDL_KEYUP)
	{
		const SDL_Scancode scancode = event->key.keysym.scancode;
		if (scancode >= 0 && scancode < SDL_NUM_SCANCODES)
			suppressedKeys[scancode] = false;
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button < 32)
			suppressedMouseButtons |= 1u << event->button.button;
		*dismissed = true;
	}
	else if (event->type == SDL_MOUSEBUTTONUP && event->button.button < 32)
	{
		suppressedMouseButtons &= ~(1u << event->button.button);
	}

	return true;
}

bool showTapeheadSplash(void)
{
	int32_t imageWidth = 0;
	int32_t imageHeight = 0;
	SDL_Texture *texture = loadSplashTexture(&imageWidth, &imageHeight);
	if (texture == NULL)
		return true;

	memset(suppressedKeys, 0, sizeof (suppressedKeys));
	suppressedMouseButtons = 0;
	SDL_ShowWindow(video.window);

	const uint32_t startTime = SDL_GetTicks();
	bool dismissed = false;
	bool keepRunning = true;
	while (!dismissed && SDL_GetTicks() - startTime < TAPEHEAD_SPLASH_DURATION_MS)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (!handleSplashEvent(&event, &dismissed))
			{
				keepRunning = false;
				break;
			}
		}

		if (!keepRunning || dismissed)
			break;

		int32_t outputWidth, outputHeight;
		if (SDL_GetRendererOutputSize(video.renderer, &outputWidth, &outputHeight) != 0)
			break;

		const SDL_Rect destination = aspectFitRect(imageWidth, imageHeight,
			outputWidth, outputHeight);
		SDL_SetRenderDrawColor(video.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(video.renderer);
		SDL_RenderCopy(video.renderer, texture, NULL, &destination);
		SDL_RenderPresent(video.renderer);
		SDL_Delay(8);
	}

	SDL_DestroyTexture(texture);
	return keepRunning;
}

bool tapeheadSplashConsumeDismissEvent(const SDL_Event *event)
{
	if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP)
	{
		const SDL_Scancode scancode = event->key.keysym.scancode;
		if (scancode >= 0 && scancode < SDL_NUM_SCANCODES && suppressedKeys[scancode])
		{
			if (event->type == SDL_KEYUP)
				suppressedKeys[scancode] = false;
			return true;
		}
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP)
	{
		const uint8_t button = event->button.button;
		if (button < 32 && (suppressedMouseButtons & (1u << button)) != 0)
		{
			if (event->type == SDL_MOUSEBUTTONUP)
				suppressedMouseButtons &= ~(1u << button);
			return true;
		}
	}

	return false;
}
