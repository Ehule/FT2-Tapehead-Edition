#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_unicode.h"
#include "ft2_tapesister_protocol.h"

enum
{
	STEREO_SAMPLE_READ_LEFT_CHANNEL = 1,
	STEREO_SAMPLE_READ_RIGHT_CHANNEL = 2,
	STEREO_SAMPLE_MIX_TO_MONO = 3,
};

void normalizeSigned32Bit(int32_t *sampleData, uint32_t sampleLength);
void normalize32BitFloatToSigned16Bit(float *fSampleData, uint32_t sampleLength);
void normalize64BitFloatToSigned16Bit(double *dSampleData, uint32_t sampleLength);

bool loadSample(UNICHAR *filenameU, uint8_t sampleSlot, bool loadAsInstrFlag);
enum
{
	SAMPLE_FOLDER_IMPORT_INSTRUMENTS = 0,
	SAMPLE_FOLDER_IMPORT_CURRENT_INSTRUMENT = 1,
	SAMPLE_FOLDER_IMPORT_LAUNCHER = 2,
	SAMPLE_FOLDER_IMPORT_MATRIX_OPEN = 3,
	SAMPLE_FOLDER_IMPORT_TAPESISTER = 4
};

bool loadSampleFolder(const UNICHAR *folderPathU, const UNICHAR *const *fileNamesU,
	uint32_t fileCount, uint8_t mode, bool autoMap);
bool loadSamplesToMatrix(const UNICHAR *folderPathU,
	const UNICHAR *const *fileNamesU, uint32_t fileCount, uint16_t startTile,
	bool replaceExactTile);
bool sampleMatrixImportTakeResult(uint32_t *added, uint32_t *requested,
	uint32_t *omitted);
void removeSampleIsLoadingFlag(void);
bool sampleLoaderIsBusy(void);
bool loadTapeSisterExchange(const UNICHAR *folderPathU,
	const tapeheadExchangeOffer_t *offer,
	const tapeheadExchangeDestination_t *destinations);

// globals for sample loaders
extern bool loadAsInstrFlag, smpFilenameSet;
extern char *smpFilename;
extern uint8_t sampleSlot;
extern sample_t tmpSmp;
// --------------------------

// file extensions accepted by Disk Op. in sample mode
extern char *supportedSmpExtensions[];
