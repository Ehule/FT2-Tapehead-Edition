#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum bakerAssetError_t
{
	BAKER_ASSET_OK = 0,
	BAKER_ASSET_INSTRUMENT_LIMIT,
	BAKER_ASSET_MEMORY
} bakerAssetError_t;

void bakerAssetsReset(void);
void bakerAssetsFree(void);
uint8_t bakerAssetsResolveInstrument(uint8_t note, uint8_t sourceInstrument,
	uint8_t resolvedSample);
bool bakerAssetsInstall(void);
void bakerAssetsUninstall(void);
bakerAssetError_t bakerAssetsGetError(void);
uint16_t bakerAssetsGetPrivateCount(void);
