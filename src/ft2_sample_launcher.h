#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "ft2_replayer.h"
#include "ft2_sample_launcher_state.h"

typedef enum sampleLauncherPlaceResult_t
{
	SAMPLE_LAUNCHER_PLACE_OK = 0,
	SAMPLE_LAUNCHER_PLACE_EMPTY_SOURCE,
	SAMPLE_LAUNCHER_PLACE_SAME_SAMPLE,
	SAMPLE_LAUNCHER_PLACE_NO_INSTRUMENT,
	SAMPLE_LAUNCHER_PLACE_NO_MEMORY
} sampleLauncherPlaceResult_t;

typedef enum sampleLauncherTileMapState_t
{
	SAMPLE_LAUNCHER_MAP_AUTO = 0,
	SAMPLE_LAUNCHER_MAP_REFERENCE = 1,
	SAMPLE_LAUNCHER_MAP_EMPTY = 2
} sampleLauncherTileMapState_t;

typedef struct sampleLauncherUndoState_t
{
	uint8_t instruments[SAMPLE_LAUNCHER_BANK_COUNT][2];
	uint8_t outputBus[SAMPLE_LAUNCHER_BANK_COUNT][SAMPLE_LAUNCHER_TILES_PER_BANK];
	uint8_t tileState[SAMPLE_LAUNCHER_MAX_TILES];
	uint8_t tileInstrument[SAMPLE_LAUNCHER_MAX_TILES];
	uint8_t tileSample[SAMPLE_LAUNCHER_MAX_TILES];
} sampleLauncherUndoState_t;

bool sampleLauncherTileIsLoaded(uint16_t tile);
const char *sampleLauncherGetTileName(uint16_t tile);
uint8_t sampleLauncherGetTileBus(uint16_t tile);
void sampleLauncherCycleTileBus(uint16_t tile, uint8_t busCount);
uint8_t sampleLauncherGetBank(void);
void sampleLauncherSetBank(uint8_t bank);
bool sampleLauncherBankHasSamples(uint8_t bank);
void sampleLauncherGetBankInstruments(uint8_t bank, uint8_t *instrumentA, uint8_t *instrumentB);
bool sampleLauncherPrepareBankImport(uint8_t bank, uint8_t *instrumentA,
	uint8_t *instrumentB);
bool sampleLauncherPrepareRangeImport(uint8_t firstBank, uint32_t sampleCount,
	uint8_t instruments[SAMPLE_LAUNCHER_BANK_COUNT][2], uint8_t *bankCount);
void sampleLauncherAttachBank(uint8_t bank, uint8_t instrumentA,
	uint8_t instrumentB);
void sampleLauncherMakeInstrumentName(uint8_t bank, uint8_t half,
	const char *folderName, char name[23]);
int8_t sampleLauncherFindBankForInstrument(uint8_t instrument);
bool sampleLauncherClearBank(uint8_t bank);
bool sampleLauncherSelectTileInEditor(uint16_t tile);
bool sampleLauncherGetTileReference(uint16_t tile, uint8_t *instrument,
	uint8_t *sample);
bool sampleLauncherAssignTile(uint16_t tile, uint8_t instrument,
	uint8_t sample);
bool sampleLauncherUnassignTile(uint16_t tile);
bool sampleLauncherUnassignBank(uint8_t bank);
bool sampleLauncherDeleteTileSample(uint16_t tile);
bool sampleLauncherTileNaturalStorageIsLoaded(uint16_t tile);
bool sampleLauncherCanImportToTiles(const uint16_t *tiles, uint32_t count);
sampleLauncherPlaceResult_t sampleLauncherCopySampleToTile(uint16_t tile,
	uint8_t sourceInstrument, uint8_t sourceSample);
sampleLauncherPlaceResult_t sampleLauncherMoveDecodedSampleToTile(uint16_t tile,
	sample_t *source);
bool sampleLauncherWriteXMMetadata(FILE *f);
void sampleLauncherBeginModuleLoad(void);
void sampleLauncherReadXMMetadata(FILE *f, uint32_t fileSize);
void sampleLauncherCommitXMMetadata(void);
void sampleLauncherForgetBanks(void);
void sampleLauncherCaptureUndoState(sampleLauncherUndoState_t *state);
void sampleLauncherRestoreUndoState(const sampleLauncherUndoState_t *state);
bool sampleLauncherInstrumentIsMapped(uint8_t instrument);
int16_t sampleLauncherGetQCurrent(void);
void sampleLauncherClearQQueue(void);
bool sampleLauncherQStopPending(void);
int8_t sampleLauncherGetQQueuePos(uint16_t tile);
int8_t sampleLauncherGetPolySlot(uint16_t tile);
bool sampleLauncherPolyStopPending(uint16_t tile);
bool sampleLauncherPolyStartPending(uint16_t tile);
uint8_t sampleLauncherGetPolyCount(void);
bool sampleLauncherHasTransportWork(void);
bool sampleLauncherHasQWork(void);
bool sampleLauncherHasPolyWork(void);
bool sampleLauncherRequestQ(uint16_t tile);
bool sampleLauncherTogglePoly(uint16_t tile);
bool sampleLauncherScheduleStop(uint16_t tile);
bool sampleLauncherStopQ(void);
bool sampleLauncherStopPoly(void);
void sampleLauncherHardStop(uint16_t tile);
void sampleLauncherHandleBoundary(void);
void sampleLauncherTick(void);
void sampleLauncherReset(void);
void sampleLauncherFree(void);
