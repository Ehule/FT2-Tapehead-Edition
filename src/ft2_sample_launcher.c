#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "ft2_audio.h"
#include "ft2_header.h"
#include "ft2_sample_launcher.h"
#include "ft2_sample_ed.h"
#include "ft2_structs.h"
#include "ft2_undo.h"

typedef struct sampleLauncherBank_t
{
	uint8_t instrument[2];
	uint8_t outputBus[SAMPLE_LAUNCHER_TILES_PER_BANK];
} sampleLauncherBank_t;

typedef struct sampleLauncherTileMap_t
{
	uint8_t state, instrument, sample;
} sampleLauncherTileMap_t;

#define SAMPLE_LAUNCHER_XM_META_MAGIC "THDMSM01"
#define SAMPLE_LAUNCHER_XM_META_SIZE (SAMPLE_LAUNCHER_MAX_TILES * 3)

static sampleLauncherBank_t banks[SAMPLE_LAUNCHER_BANK_COUNT];
static sampleLauncherTileMap_t tileMap[SAMPLE_LAUNCHER_MAX_TILES];
static sampleLauncherTileMap_t pendingTileMap[SAMPLE_LAUNCHER_MAX_TILES];
static sampleLauncherState_t launcherState;
static uint8_t currentBank;
static bool initialized, bankMapScanned, pendingTileMapValid;
static bool standaloneClockRunning;
static uint16_t standaloneClockTick, standaloneClockRow;

static void ensureInitialized(void)
{
	if (!initialized)
	{
		sampleLauncherStateInit(&launcherState);
		initialized = true;
	}
}

static int8_t hexDigit(char c)
{
	if (c >= '0' && c <= '9') return (int8_t)(c - '0');
	c = (char)toupper((unsigned char)c);
	if (c >= 'A' && c <= 'F') return (int8_t)(10 + c - 'A');
	return -1;
}

static void scanBankInstrumentTags(void)
{
	if (bankMapScanned)
		return;

	for (uint16_t instrument = 1; instrument <= MAX_INST; instrument++)
	{
		const char *name = song.instrName[instrument];
		if (name[0] != 'S' || name[1] != 'B')
			continue;

		const int8_t high = hexDigit(name[2]);
		const int8_t low = hexDigit(name[3]);
		const char halfChar = (char)toupper((unsigned char)name[4]);
		if (high < 0 || low < 0 || (halfChar != 'A' && halfChar != 'B'))
			continue;

		const uint16_t firstTile = (uint16_t)((high << 4) | low);
		if ((firstTile % SAMPLE_LAUNCHER_TILES_PER_BANK) != 0)
			continue;

		const uint8_t bank = (uint8_t)(firstTile / SAMPLE_LAUNCHER_TILES_PER_BANK);
		if (bank < SAMPLE_LAUNCHER_BANK_COUNT)
			banks[bank].instrument[halfChar == 'B'] = (uint8_t)instrument;
	}

	bankMapScanned = true;
}


static bool beginLauncherEditUndo(const char *description, bool *ownsTransaction)
{
	*ownsTransaction = !undoTransactionIsActive();
	if (*ownsTransaction && !undoTransactionBegin(description))
		return false;
	if (!undoTransactionAddSampleLauncher())
	{
		if (*ownsTransaction)
			undoCancelTransaction();
		return false;
	}
	return true;
}

static void finishLauncherEditUndo(bool ownsTransaction)
{
	setSongModifiedFlag();
	if (ownsTransaction)
		undoTransactionCommit();
}

static bool instrumentSlotIsEmpty(uint8_t instrument)
{
	return instrument > 0 && instrument <= MAX_INST && instr[instrument] == NULL &&
		song.instrName[instrument][0] == '\0';
}

static bool getNaturalTileReference(uint16_t tile, uint8_t *instrument,
	uint8_t *sample)
{
	if (tile >= SAMPLE_LAUNCHER_MAX_TILES)
		return false;

	scanBankInstrumentTags();
	const uint8_t bank = (uint8_t)(tile / SAMPLE_LAUNCHER_TILES_PER_BANK);
	const uint8_t localTile = (uint8_t)(tile % SAMPLE_LAUNCHER_TILES_PER_BANK);
	const uint8_t naturalInstrument =
		banks[bank].instrument[localTile / MAX_SMP_PER_INST];
	if (naturalInstrument == 0 || naturalInstrument > MAX_INST)
		return false;

	if (instrument != NULL)
		*instrument = naturalInstrument;
	if (sample != NULL)
		*sample = localTile % MAX_SMP_PER_INST;
	return true;
}

bool sampleLauncherGetTileReference(uint16_t tile, uint8_t *instrument,
	uint8_t *sample)
{
	if (tile >= SAMPLE_LAUNCHER_MAX_TILES)
		return false;

	const sampleLauncherTileMap_t *mapping = &tileMap[tile];
	if (mapping->state == SAMPLE_LAUNCHER_MAP_EMPTY)
		return false;
	if (mapping->state == SAMPLE_LAUNCHER_MAP_REFERENCE)
	{
		if (mapping->instrument == 0 || mapping->instrument > MAX_INST ||
			mapping->sample >= MAX_SMP_PER_INST)
		{
			return false;
		}
		if (instrument != NULL)
			*instrument = mapping->instrument;
		if (sample != NULL)
			*sample = mapping->sample;
		return true;
	}

	return getNaturalTileReference(tile, instrument, sample);
}

static sample_t *getTileSample(uint16_t tile)
{
	uint8_t instrument, sample;
	if (!sampleLauncherGetTileReference(tile, &instrument, &sample) ||
		instr[instrument] == NULL)
	{
		return NULL;
	}

	return &instr[instrument]->smp[sample];
}

static bool hasTransportWork(void)
{
	if (launcherState.qCurrent >= 0 || launcherState.qQueueCount > 0 ||
		launcherState.qStopPending || launcherState.polyStartCount > 0)
	{
		return true;
	}

	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_MAX_POLY; i++)
	{
		if (launcherState.polyTile[i] >= 0)
			return true;
	}
	return false;
}

bool sampleLauncherHasTransportWork(void)
{
	ensureInitialized();
	return hasTransportWork();
}

static void executeBoundaryActions(void)
{
	sampleLauncherAction_t actions[SAMPLE_LAUNCHER_MAX_ACTIONS];
	const uint8_t count = sampleLauncherStateCommitBoundary(&launcherState,
		actions);
	for (uint8_t i = 0; i < count; i++)
	{
		const sampleLauncherAction_t *action = &actions[i];
		if (action->type == SAMPLE_LAUNCHER_ACTION_STOP_Q ||
			action->type == SAMPLE_LAUNCHER_ACTION_STOP_POLY)
		{
			audioSampleLauncherStop((uint8_t)action->voice);
		}
		else if ((action->type == SAMPLE_LAUNCHER_ACTION_START_Q ||
			action->type == SAMPLE_LAUNCHER_ACTION_START_POLY) &&
			action->tile >= 0)
		{
			sample_t *sample = getTileSample((uint16_t)action->tile);
			const uint8_t outputBus = sampleLauncherGetTileBus((uint16_t)action->tile);
			audioSampleLauncherTrigger((uint8_t)action->voice, sample, outputBus);
		}
	}
}

bool sampleLauncherTileIsLoaded(uint16_t tile)
{
	const sample_t *sample = getTileSample(tile);
	return sample != NULL && sample->dataPtr != NULL && sample->length > 0;
}

const char *sampleLauncherGetTileName(uint16_t tile)
{
	const sample_t *sample = getTileSample(tile);
	return sample != NULL ? sample->name : "";
}

uint8_t sampleLauncherGetTileBus(uint16_t tile)
{
	if (tile >= SAMPLE_LAUNCHER_MAX_TILES)
		return 0;
	const uint8_t bank = (uint8_t)(tile / SAMPLE_LAUNCHER_TILES_PER_BANK);
	const uint8_t localTile = (uint8_t)(tile % SAMPLE_LAUNCHER_TILES_PER_BANK);
	return banks[bank].outputBus[localTile];
}

void sampleLauncherCycleTileBus(uint16_t tile, uint8_t busCount)
{
	if (!sampleLauncherTileIsLoaded(tile))
		return;
	if (busCount < 1)
		busCount = 1;
	if (busCount > TAPEHEAD_MAX_OUTPUT_BUSES)
		busCount = TAPEHEAD_MAX_OUTPUT_BUSES;

	const uint8_t bank = (uint8_t)(tile / SAMPLE_LAUNCHER_TILES_PER_BANK);
	const uint8_t localTile = (uint8_t)(tile % SAMPLE_LAUNCHER_TILES_PER_BANK);
	uint8_t *outputBus = &banks[bank].outputBus[localTile];
	*outputBus = (uint8_t)((*outputBus + 1) % busCount);
	if (launcherState.qCurrent == tile)
		audioSampleLauncherSetOutputBus(0, *outputBus);
	const int8_t polySlot = sampleLauncherStateGetPolySlot(&launcherState, tile);
	if (polySlot >= 0)
		audioSampleLauncherSetOutputBus((uint8_t)(polySlot + 1), *outputBus);
}

uint8_t sampleLauncherGetBank(void)
{
	return currentBank;
}

void sampleLauncherSetBank(uint8_t bank)
{
	currentBank = bank & (SAMPLE_LAUNCHER_BANK_COUNT - 1);
	scanBankInstrumentTags();
}

void sampleLauncherGetBankInstruments(uint8_t bank, uint8_t *instrumentA, uint8_t *instrumentB)
{
	if (instrumentA != NULL) *instrumentA = 0;
	if (instrumentB != NULL) *instrumentB = 0;
	if (bank >= SAMPLE_LAUNCHER_BANK_COUNT) return;
	scanBankInstrumentTags();
	if (instrumentA != NULL) *instrumentA = banks[bank].instrument[0];
	if (instrumentB != NULL) *instrumentB = banks[bank].instrument[1];
}

bool sampleLauncherBankHasSamples(uint8_t bank)
{
	if (bank >= SAMPLE_LAUNCHER_BANK_COUNT)
		return false;
	const uint16_t firstTile = bank * SAMPLE_LAUNCHER_TILES_PER_BANK;
	for (uint16_t i = 0; i < SAMPLE_LAUNCHER_TILES_PER_BANK; i++)
	{
		if (sampleLauncherTileIsLoaded(firstTile + i))
			return true;
	}
	return false;
}

bool sampleLauncherPrepareBankImport(uint8_t bank, uint8_t *instrumentA,
	uint8_t *instrumentB)
{
	if (bank >= SAMPLE_LAUNCHER_BANK_COUNT || instrumentA == NULL ||
		instrumentB == NULL)
	{
		return false;
	}

	scanBankInstrumentTags();
	uint8_t destinations[2] = { banks[bank].instrument[0], banks[bank].instrument[1] };
	for (uint8_t half = 0; half < 2; half++)
	{
		if (destinations[half] > 0 && destinations[half] <= MAX_INST)
			continue;

		for (uint16_t candidate = 1; candidate <= MAX_INST; candidate++)
		{
			if (candidate != destinations[0] && candidate != destinations[1] &&
				instrumentSlotIsEmpty((uint8_t)candidate))
			{
				destinations[half] = (uint8_t)candidate;
				break;
			}
		}
		if (destinations[half] == 0)
			return false;
	}

	*instrumentA = destinations[0];
	*instrumentB = destinations[1];
	return true;
}

bool sampleLauncherPrepareRangeImport(uint8_t firstBank, uint32_t sampleCount,
	uint8_t instruments[SAMPLE_LAUNCHER_BANK_COUNT][2], uint8_t *bankCount)
{
	if (firstBank >= SAMPLE_LAUNCHER_BANK_COUNT || sampleCount == 0 ||
		instruments == NULL || bankCount == NULL)
	{
		return false;
	}

	const uint32_t capacity =
		(SAMPLE_LAUNCHER_BANK_COUNT - firstBank) * SAMPLE_LAUNCHER_TILES_PER_BANK;
	if (sampleCount > capacity)
		return false;

	memset(instruments, 0,
		SAMPLE_LAUNCHER_BANK_COUNT * 2 * sizeof (uint8_t));
	*bankCount = (uint8_t)((sampleCount + SAMPLE_LAUNCHER_TILES_PER_BANK - 1) /
		SAMPLE_LAUNCHER_TILES_PER_BANK);

	scanBankInstrumentTags();
	bool reserved[MAX_INST + 1] = { false };
	const uint32_t instrumentCount =
		(sampleCount + MAX_SMP_PER_INST - 1) / MAX_SMP_PER_INST;
	for (uint32_t index = 0; index < instrumentCount; index++)
	{
		const uint8_t bankOffset = (uint8_t)(index / 2);
		const uint8_t half = (uint8_t)(index & 1);
		const uint8_t bank = firstBank + bankOffset;
		uint8_t destination = banks[bank].instrument[half];
		if (destination == 0 || destination > MAX_INST || reserved[destination])
		{
			destination = 0;
			for (uint16_t candidate = 1; candidate <= MAX_INST; candidate++)
			{
				if (!reserved[candidate] && instrumentSlotIsEmpty((uint8_t)candidate))
				{
					destination = (uint8_t)candidate;
					break;
				}
			}
		}

		if (destination == 0)
		{
			*bankCount = 0;
			memset(instruments, 0,
				SAMPLE_LAUNCHER_BANK_COUNT * 2 * sizeof (uint8_t));
			return false;
		}

		instruments[bankOffset][half] = destination;
		reserved[destination] = true;
	}

	return true;
}

void sampleLauncherAttachBank(uint8_t bank, uint8_t instrumentA,
	uint8_t instrumentB)
{
	if (bank >= SAMPLE_LAUNCHER_BANK_COUNT)
		return;
	sampleLauncherReset();
	banks[bank].instrument[0] = instrumentA;
	banks[bank].instrument[1] = instrumentB;
	memset(banks[bank].outputBus, 0, sizeof (banks[bank].outputBus));
	memset(&tileMap[bank * SAMPLE_LAUNCHER_TILES_PER_BANK], 0,
		SAMPLE_LAUNCHER_TILES_PER_BANK * sizeof (sampleLauncherTileMap_t));
	bankMapScanned = true;
}

void sampleLauncherMakeInstrumentName(uint8_t bank, uint8_t half,
	const char *folderName, char name[23])
{
	const uint8_t first = (uint8_t)((bank * SAMPLE_LAUNCHER_TILES_PER_BANK) +
		(half * MAX_SMP_PER_INST));
	const uint8_t last = (uint8_t)(first + MAX_SMP_PER_INST - 1);
	if (folderName == NULL || folderName[0] == '\0')
		folderName = "BANK";
	snprintf(name, 23, "SB%02X%c %02X-%02X %.7s", bank * 32,
		half == 0 ? 'A' : 'B', first, last, folderName);
	name[22] = '\0';
}

int8_t sampleLauncherFindBankForInstrument(uint8_t instrument)
{
	scanBankInstrumentTags();
	for (uint8_t bank = 0; bank < SAMPLE_LAUNCHER_BANK_COUNT; bank++)
	{
		if (banks[bank].instrument[0] == instrument ||
			banks[bank].instrument[1] == instrument)
		{
			return (int8_t)bank;
		}
	}
	return -1;
}

bool sampleLauncherInstrumentIsMapped(uint8_t instrument)
{
	if (sampleLauncherFindBankForInstrument(instrument) >= 0)
		return true;
	for (uint16_t tile = 0; tile < SAMPLE_LAUNCHER_MAX_TILES; tile++)
	{
		if (tileMap[tile].state == SAMPLE_LAUNCHER_MAP_REFERENCE &&
			tileMap[tile].instrument == instrument)
		{
			return true;
		}
	}
	return false;
}

bool sampleLauncherClearBank(uint8_t bank)
{
	if (bank >= SAMPLE_LAUNCHER_BANK_COUNT)
		return false;
	scanBankInstrumentTags();
	const uint8_t instrumentA = banks[bank].instrument[0];
	const uint8_t instrumentB = banks[bank].instrument[1];
	if (instrumentA == 0 && instrumentB == 0)
		return false;

	sampleLauncherReset();
	banks[bank].instrument[0] = 0;
	banks[bank].instrument[1] = 0;
	memset(&tileMap[bank * SAMPLE_LAUNCHER_TILES_PER_BANK], 0,
		SAMPLE_LAUNCHER_TILES_PER_BANK * sizeof (sampleLauncherTileMap_t));
	if (instrumentA > 0)
	{
		freeInstr(instrumentA);
		memset(song.instrName[instrumentA], 0, sizeof (song.instrName[instrumentA]));
	}
	if (instrumentB > 0 && instrumentB != instrumentA)
	{
		freeInstr(instrumentB);
		memset(song.instrName[instrumentB], 0, sizeof (song.instrName[instrumentB]));
	}
	return true;
}

bool sampleLauncherSelectTileInEditor(uint16_t tile)
{
	if (!sampleLauncherTileIsLoaded(tile))
		return false;
	uint8_t instrument, sample;
	if (!sampleLauncherGetTileReference(tile, &instrument, &sample))
		return false;
	editor.curInstr = instrument;
	editor.curSmp = sample;
	return true;
}

bool sampleLauncherAssignTile(uint16_t tile, uint8_t instrument,
	uint8_t sample)
{
	if (tile >= SAMPLE_LAUNCHER_MAX_TILES || instrument == 0 ||
		instrument > MAX_INST || sample >= MAX_SMP_PER_INST ||
		instr[instrument] == NULL)
	{
		return false;
	}

	const sample_t *source = &instr[instrument]->smp[sample];
	if (source->dataPtr == NULL || source->length <= 0)
		return false;

	bool ownsUndo;
	if (!beginLauncherEditUndo("Assign Sample Matrix tile", &ownsUndo))
		return false;

	uint8_t naturalInstrument, naturalSample;
	const bool natural = getNaturalTileReference(tile, &naturalInstrument,
		&naturalSample) && naturalInstrument == instrument && naturalSample == sample;
	sampleLauncherHardStop(tile);
	tileMap[tile].state = natural ? SAMPLE_LAUNCHER_MAP_AUTO :
		SAMPLE_LAUNCHER_MAP_REFERENCE;
	tileMap[tile].instrument = instrument;
	tileMap[tile].sample = sample;
	finishLauncherEditUndo(ownsUndo);
	return true;
}

bool sampleLauncherUnassignTile(uint16_t tile)
{
	if (tile >= SAMPLE_LAUNCHER_MAX_TILES)
		return false;

	bool ownsUndo;
	if (!beginLauncherEditUndo("Clear Sample Matrix tile", &ownsUndo))
		return false;

	sampleLauncherHardStop(tile);
	tileMap[tile].state = SAMPLE_LAUNCHER_MAP_EMPTY;
	tileMap[tile].instrument = 0;
	tileMap[tile].sample = 0;
	finishLauncherEditUndo(ownsUndo);
	return true;
}

bool sampleLauncherUnassignBank(uint8_t bank)
{
	if (bank >= SAMPLE_LAUNCHER_BANK_COUNT)
		return false;

	scanBankInstrumentTags();
	const uint8_t bankInstruments[2] =
	{
		banks[bank].instrument[0], banks[bank].instrument[1]
	};

	bool ownsUndo;
	if (!beginLauncherEditUndo("Clear Sample Matrix bank", &ownsUndo))
		return false;
	for (uint8_t half = 0; half < 2; half++)
	{
		const uint8_t instrument = bankInstruments[half];
		if (instrument > 0 && !undoTransactionAddInstrument(instrument))
		{
			if (ownsUndo) undoCancelTransaction();
			return false;
		}
	}

	/* CLEAR BNK is intentionally non-destructive. Detach the tagged halves
	** from the Matrix and keep their samples as ordinary FT2 instruments. */
	const uint16_t first = bank * SAMPLE_LAUNCHER_TILES_PER_BANK;
	for (uint16_t i = 0; i < SAMPLE_LAUNCHER_TILES_PER_BANK; i++)
	{
		sampleLauncherHardStop(first + i);
		tileMap[first+i].state = SAMPLE_LAUNCHER_MAP_EMPTY;
		tileMap[first+i].instrument = 0;
		tileMap[first+i].sample = 0;
	}

	for (uint8_t half = 0; half < 2; half++)
	{
		const uint8_t instrument = bankInstruments[half];
		if (instrument == 0 || instrument > MAX_INST)
			continue;

		char *name = song.instrName[instrument];
		const size_t length = strlen(name);
		if (length >= 5 && name[0] == 'S' && name[1] == 'B')
		{
			const size_t prefixLength = length > 5 && name[5] == ' ' ? 6 : 5;
			memmove(name, name + prefixLength, length - prefixLength);
			memset(name + length - prefixLength, 0, 23 - (length - prefixLength));
		}
	}

	banks[bank].instrument[0] = 0;
	banks[bank].instrument[1] = 0;
	memset(banks[bank].outputBus, 0, sizeof (banks[bank].outputBus));
	bankMapScanned = true;
	finishLauncherEditUndo(ownsUndo);
	return true;
}

bool sampleLauncherDeleteTileSample(uint16_t tile)
{
	uint8_t instrument, sample;
	if (!sampleLauncherGetTileReference(tile, &instrument, &sample) ||
		instr[instrument] == NULL || instr[instrument]->smp[sample].dataPtr == NULL)
	{
		return false;
	}

	const bool ownsUndo = !undoTransactionIsActive();
	if (ownsUndo && !undoTransactionBegin("Delete Sample Matrix sample"))
		return false;
	if (!undoTransactionAddSample(instrument, sample))
	{
		if (ownsUndo) undoCancelTransaction();
		return false;
	}

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();
	sampleLauncherReset();
	freeSample(instrument, sample);
	setSongModifiedFlag();
	if (ownsUndo)
		undoTransactionCommit();
	if (audioWasntLocked)
		unlockAudio();
	return true;
}

bool sampleLauncherTileNaturalStorageIsLoaded(uint16_t tile)
{
	uint8_t instrument, sample;
	if (!getNaturalTileReference(tile, &instrument, &sample) ||
		instr[instrument] == NULL)
	{
		return false;
	}
	const sample_t *nativeSample = &instr[instrument]->smp[sample];
	return nativeSample->dataPtr != NULL && nativeSample->length > 0;
}

bool sampleLauncherCanImportToTiles(const uint16_t *tiles, uint32_t count)
{
	if (tiles == NULL || count == 0)
		return false;
	scanBankInstrumentTags();
	bool needed[SAMPLE_LAUNCHER_BANK_COUNT][2] = { { false } };
	uint32_t neededCount = 0;
	for (uint32_t i = 0; i < count; i++)
	{
		if (tiles[i] >= SAMPLE_LAUNCHER_MAX_TILES)
			return false;
		const uint8_t bank = tiles[i] / SAMPLE_LAUNCHER_TILES_PER_BANK;
		const uint8_t half = (tiles[i] % SAMPLE_LAUNCHER_TILES_PER_BANK) /
			MAX_SMP_PER_INST;
		if ((banks[bank].instrument[half] == 0 ||
			banks[bank].instrument[half] > MAX_INST) && !needed[bank][half])
		{
			needed[bank][half] = true;
			neededCount++;
		}
	}
	uint32_t emptyCount = 0;
	for (uint16_t instrument = 1; instrument <= MAX_INST; instrument++)
		emptyCount += instrumentSlotIsEmpty((uint8_t)instrument);
	return emptyCount >= neededCount;
}

sampleLauncherPlaceResult_t sampleLauncherCopySampleToTile(uint16_t tile,
	uint8_t sourceInstrument, uint8_t sourceSample)
{
	if (tile >= SAMPLE_LAUNCHER_MAX_TILES || sourceInstrument == 0 ||
		sourceInstrument > MAX_INST || sourceSample >= MAX_SMP_PER_INST ||
		instr[sourceInstrument] == NULL)
	{
		return SAMPLE_LAUNCHER_PLACE_EMPTY_SOURCE;
	}

	sample_t *source = &instr[sourceInstrument]->smp[sourceSample];
	if (source->dataPtr == NULL || source->length <= 0)
		return SAMPLE_LAUNCHER_PLACE_EMPTY_SOURCE;

	scanBankInstrumentTags();
	const uint8_t bank = (uint8_t)(tile / SAMPLE_LAUNCHER_TILES_PER_BANK);
	const uint8_t localTile = (uint8_t)(tile % SAMPLE_LAUNCHER_TILES_PER_BANK);
	const uint8_t half = localTile / MAX_SMP_PER_INST;
	const uint8_t destinationSample = localTile % MAX_SMP_PER_INST;
	uint8_t destinationInstrument = banks[bank].instrument[half];

	if (destinationInstrument > 0 && destinationInstrument <= MAX_INST &&
		instr[destinationInstrument] != NULL &&
		source == &instr[destinationInstrument]->smp[destinationSample])
	{
		return SAMPLE_LAUNCHER_PLACE_SAME_SAMPLE;
	}

	if (destinationInstrument == 0 || destinationInstrument > MAX_INST)
	{
		for (uint16_t candidate = 1; candidate <= MAX_INST; candidate++)
		{
			if (instrumentSlotIsEmpty((uint8_t)candidate))
			{
				destinationInstrument = (uint8_t)candidate;
				break;
			}
		}
		if (destinationInstrument == 0 || destinationInstrument > MAX_INST)
			return SAMPLE_LAUNCHER_PLACE_NO_INSTRUMENT;
	}

	sample_t stagedSample = { 0 };
	if (!cloneSample(source, &stagedSample))
		return SAMPLE_LAUNCHER_PLACE_NO_MEMORY;

	const bool createInstrument = instr[destinationInstrument] == NULL;
	bool ownsUndo;
	if (!beginLauncherEditUndo("Copy sample to Deck", &ownsUndo) ||
		!(createInstrument ? undoTransactionAddInstrument(destinationInstrument) :
		  undoTransactionAddSample(destinationInstrument, destinationSample)))
	{
		if (ownsUndo && undoTransactionIsActive()) undoCancelTransaction();
		freeSmpData(&stagedSample);
		return SAMPLE_LAUNCHER_PLACE_NO_MEMORY;
	}

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();
	sampleLauncherReset();
	if (createInstrument && !allocateInstr(destinationInstrument))
	{
		if (ownsUndo) undoCancelTransaction();
		if (audioWasntLocked) unlockAudio();
		freeSmpData(&stagedSample);
		return SAMPLE_LAUNCHER_PLACE_NO_MEMORY;
	}

	if (createInstrument)
	{
		char instrumentName[23];
		sampleLauncherMakeInstrumentName(bank, half, source->name, instrumentName);
		memset(song.instrName[destinationInstrument], 0,
			sizeof (song.instrName[destinationInstrument]));
		memcpy(song.instrName[destinationInstrument], instrumentName, 22);
	}

	sample_t *destination = &instr[destinationInstrument]->smp[destinationSample];
	freeSmpData(destination);
	*destination = stagedSample;
	banks[bank].instrument[half] = destinationInstrument;
	tileMap[tile].state = SAMPLE_LAUNCHER_MAP_AUTO;
	tileMap[tile].instrument = 0;
	tileMap[tile].sample = 0;
	bankMapScanned = true;
	fixInstrAndSampleNames(destinationInstrument);
	finishLauncherEditUndo(ownsUndo);

	if (audioWasntLocked)
		unlockAudio();
	return SAMPLE_LAUNCHER_PLACE_OK;
}

sampleLauncherPlaceResult_t sampleLauncherMoveDecodedSampleToTile(uint16_t tile,
	sample_t *source)
{
	if (tile >= SAMPLE_LAUNCHER_MAX_TILES || source == NULL ||
		source->dataPtr == NULL || source->length <= 0)
	{
		return SAMPLE_LAUNCHER_PLACE_EMPTY_SOURCE;
	}

	scanBankInstrumentTags();
	const uint8_t bank = (uint8_t)(tile / SAMPLE_LAUNCHER_TILES_PER_BANK);
	const uint8_t localTile = (uint8_t)(tile % SAMPLE_LAUNCHER_TILES_PER_BANK);
	const uint8_t half = localTile / MAX_SMP_PER_INST;
	const uint8_t destinationSample = localTile % MAX_SMP_PER_INST;
	uint8_t destinationInstrument = banks[bank].instrument[half];
	if (destinationInstrument == 0 || destinationInstrument > MAX_INST)
	{
		for (uint16_t candidate = 1; candidate <= MAX_INST; candidate++)
		{
			if (instrumentSlotIsEmpty((uint8_t)candidate))
			{
				destinationInstrument = (uint8_t)candidate;
				break;
			}
		}
		if (destinationInstrument == 0 || destinationInstrument > MAX_INST)
			return SAMPLE_LAUNCHER_PLACE_NO_INSTRUMENT;
	}

	const bool createInstrument = instr[destinationInstrument] == NULL;
	bool ownsUndo;
	if (!beginLauncherEditUndo("Import sample to Matrix", &ownsUndo) ||
		!(createInstrument ? undoTransactionAddInstrument(destinationInstrument) :
		  undoTransactionAddSample(destinationInstrument, destinationSample)))
	{
		if (ownsUndo && undoTransactionIsActive()) undoCancelTransaction();
		return SAMPLE_LAUNCHER_PLACE_NO_MEMORY;
	}

	const bool audioWasntLocked = !audio.locked;
	if (audioWasntLocked)
		lockAudio();
	sampleLauncherReset();
	if (createInstrument && !allocateInstr(destinationInstrument))
	{
		if (ownsUndo) undoCancelTransaction();
		if (audioWasntLocked) unlockAudio();
		return SAMPLE_LAUNCHER_PLACE_NO_MEMORY;
	}
	if (createInstrument)
	{
		char instrumentName[23];
		sampleLauncherMakeInstrumentName(bank, half, source->name, instrumentName);
		memset(song.instrName[destinationInstrument], 0,
			sizeof (song.instrName[destinationInstrument]));
		memcpy(song.instrName[destinationInstrument], instrumentName, 22);
	}

	sample_t *destination = &instr[destinationInstrument]->smp[destinationSample];
	freeSmpData(destination);
	*destination = *source;
	memset(source, 0, sizeof (*source));
	sanitizeSample(destination);
	fixSample(destination);
	banks[bank].instrument[half] = destinationInstrument;
	tileMap[tile].state = SAMPLE_LAUNCHER_MAP_AUTO;
	tileMap[tile].instrument = 0;
	tileMap[tile].sample = 0;
	bankMapScanned = true;
	fixInstrAndSampleNames(destinationInstrument);
	finishLauncherEditUndo(ownsUndo);
	if (audioWasntLocked)
		unlockAudio();
	return SAMPLE_LAUNCHER_PLACE_OK;
}

void sampleLauncherCaptureUndoState(sampleLauncherUndoState_t *state)
{
	if (state == NULL)
		return;

	scanBankInstrumentTags();
	memset(state, 0, sizeof (*state));
	for (uint8_t bank = 0; bank < SAMPLE_LAUNCHER_BANK_COUNT; bank++)
	{
		state->instruments[bank][0] = banks[bank].instrument[0];
		state->instruments[bank][1] = banks[bank].instrument[1];
		memcpy(state->outputBus[bank], banks[bank].outputBus,
			sizeof (banks[bank].outputBus));
	}

	for (uint16_t tile = 0; tile < SAMPLE_LAUNCHER_MAX_TILES; tile++)
	{
		state->tileState[tile] = tileMap[tile].state;
		state->tileInstrument[tile] = tileMap[tile].instrument;
		state->tileSample[tile] = tileMap[tile].sample;
	}
}

void sampleLauncherRestoreUndoState(const sampleLauncherUndoState_t *state)
{
	if (state == NULL)
		return;

	sampleLauncherReset();
	for (uint8_t bank = 0; bank < SAMPLE_LAUNCHER_BANK_COUNT; bank++)
	{
		banks[bank].instrument[0] = state->instruments[bank][0];
		banks[bank].instrument[1] = state->instruments[bank][1];
		memcpy(banks[bank].outputBus, state->outputBus[bank],
			sizeof (banks[bank].outputBus));
	}

	for (uint16_t tile = 0; tile < SAMPLE_LAUNCHER_MAX_TILES; tile++)
	{
		tileMap[tile].state = state->tileState[tile];
		tileMap[tile].instrument = state->tileInstrument[tile];
		tileMap[tile].sample = state->tileSample[tile];
	}
	bankMapScanned = true;
}

void sampleLauncherForgetBanks(void)
{
	ensureInitialized();
	sampleLauncherReset();
	memset(banks, 0, sizeof (banks));
	memset(tileMap, 0, sizeof (tileMap));
	currentBank = 0;
	bankMapScanned = false;
}

bool sampleLauncherWriteXMMetadata(FILE *f)
{
	if (f == NULL)
		return false;
	const uint32_t payloadSize = SAMPLE_LAUNCHER_XM_META_SIZE;
	if (fwrite(SAMPLE_LAUNCHER_XM_META_MAGIC, 1, 8, f) != 8 ||
		fwrite(&payloadSize, sizeof (payloadSize), 1, f) != 1)
	{
		return false;
	}
	for (uint16_t tile = 0; tile < SAMPLE_LAUNCHER_MAX_TILES; tile++)
	{
		const uint8_t entry[3] = { tileMap[tile].state,
			tileMap[tile].instrument, tileMap[tile].sample };
		if (fwrite(entry, 1, sizeof (entry), f) != sizeof (entry))
			return false;
	}
	return true;
}

void sampleLauncherBeginModuleLoad(void)
{
	pendingTileMapValid = false;
	memset(pendingTileMap, 0, sizeof (pendingTileMap));
}

void sampleLauncherReadXMMetadata(FILE *f, uint32_t fileSize)
{
	if (f == NULL)
		return;
	const long position = ftell(f);
	if (position < 0 || (uint64_t)position + 12 + SAMPLE_LAUNCHER_XM_META_SIZE > fileSize)
		return;

	char magic[8];
	uint32_t payloadSize;
	if (fread(magic, 1, sizeof (magic), f) != sizeof (magic) ||
		fread(&payloadSize, sizeof (payloadSize), 1, f) != 1 ||
		memcmp(magic, SAMPLE_LAUNCHER_XM_META_MAGIC, sizeof (magic)) != 0 ||
		payloadSize != SAMPLE_LAUNCHER_XM_META_SIZE)
	{
		return;
	}

	for (uint16_t tile = 0; tile < SAMPLE_LAUNCHER_MAX_TILES; tile++)
	{
		uint8_t entry[3];
		if (fread(entry, 1, sizeof (entry), f) != sizeof (entry))
		{
			pendingTileMapValid = false;
			return;
		}
		pendingTileMap[tile].state = entry[0] <= SAMPLE_LAUNCHER_MAP_EMPTY
			? entry[0] : SAMPLE_LAUNCHER_MAP_AUTO;
		pendingTileMap[tile].instrument = entry[1];
		pendingTileMap[tile].sample = entry[2] < MAX_SMP_PER_INST ? entry[2] : 0;
	}
	pendingTileMapValid = true;
}

void sampleLauncherCommitXMMetadata(void)
{
	if (pendingTileMapValid)
		memcpy(tileMap, pendingTileMap, sizeof (tileMap));
	pendingTileMapValid = false;
}

int16_t sampleLauncherGetQCurrent(void)
{
	ensureInitialized();
	return launcherState.qCurrent;
}

bool sampleLauncherQStopPending(void)
{
	ensureInitialized();
	return launcherState.qStopPending;
}

int8_t sampleLauncherGetQQueuePos(uint16_t tile)
{
	ensureInitialized();
	return sampleLauncherStateGetQQueuePos(&launcherState, tile);
}

int8_t sampleLauncherGetPolySlot(uint16_t tile)
{
	ensureInitialized();
	return sampleLauncherStateGetPolySlot(&launcherState, tile);
}

bool sampleLauncherPolyStopPending(uint16_t tile)
{
	ensureInitialized();
	return sampleLauncherStatePolyStopPending(&launcherState, tile);
}

bool sampleLauncherPolyStartPending(uint16_t tile)
{
	ensureInitialized();
	return sampleLauncherStatePolyStartPending(&launcherState, tile);
}

uint8_t sampleLauncherGetPolyCount(void)
{
	ensureInitialized();
	uint8_t count = launcherState.polyStartCount;
	for (uint8_t i = 0; i < SAMPLE_LAUNCHER_MAX_POLY; i++)
	{
		if (launcherState.polyTile[i] >= 0 && !launcherState.polyStopPending[i])
			count++;
	}
	return count;
}

bool sampleLauncherRequestQ(uint16_t tile)
{
	ensureInitialized();
	const bool hadClockWork = hasTransportWork();
	if (!sampleLauncherTileIsLoaded(tile) ||
		!sampleLauncherStateRequestQ(&launcherState, tile))
	{
		return false;
	}
	if (!songPlaying && !hadClockWork)
	{
		standaloneClockRunning = true;
		standaloneClockTick = standaloneClockRow = 0;
		executeBoundaryActions();
	}
	return true;
}

bool sampleLauncherTogglePoly(uint16_t tile)
{
	ensureInitialized();
	const bool hadClockWork = hasTransportWork();
	if (!sampleLauncherTileIsLoaded(tile) ||
		!sampleLauncherStateTogglePoly(&launcherState, tile))
	{
		return false;
	}
	if (!songPlaying && !hadClockWork)
	{
		standaloneClockRunning = true;
		standaloneClockTick = standaloneClockRow = 0;
		executeBoundaryActions();
	}
	return true;
}

void sampleLauncherHardStop(uint16_t tile)
{
	ensureInitialized();
	sampleLauncherAction_t actions[SAMPLE_LAUNCHER_MAX_ACTIONS];
	const uint8_t count = sampleLauncherStateHardStop(&launcherState, tile,
		actions);
	for (uint8_t i = 0; i < count; i++)
	{
		if (actions[i].type == SAMPLE_LAUNCHER_ACTION_STOP_Q ||
			actions[i].type == SAMPLE_LAUNCHER_ACTION_STOP_POLY)
		{
			audioSampleLauncherStop((uint8_t)actions[i].voice);
		}
	}
}

void sampleLauncherHandleBoundary(void)
{
	ensureInitialized();
	standaloneClockRunning = false;
	standaloneClockTick = standaloneClockRow = 0;
	executeBoundaryActions();
}

void sampleLauncherTick(void)
{
	ensureInitialized();
	if (songPlaying)
	{
		standaloneClockRunning = false;
		standaloneClockTick = standaloneClockRow = 0;
		return;
	}

	if (!hasTransportWork())
	{
		standaloneClockRunning = false;
		standaloneClockTick = standaloneClockRow = 0;
		return;
	}

	if (!standaloneClockRunning)
	{
		standaloneClockRunning = true;
		standaloneClockTick = standaloneClockRow = 0;
	}

	const uint16_t ticksPerRow = song.speed > 0 ? song.speed : 1;
	if (++standaloneClockTick < ticksPerRow)
		return;
	standaloneClockTick = 0;

	uint16_t rowsPerBoundary = song.currNumRows;
	if (rowsPerBoundary < 1 || rowsPerBoundary > MAX_PATT_LEN)
		rowsPerBoundary = 64;
	if (++standaloneClockRow < rowsPerBoundary)
		return;

	standaloneClockRow = 0;
	executeBoundaryActions();
}

void sampleLauncherReset(void)
{
	ensureInitialized();
	audioSampleLauncherStopAll();
	sampleLauncherStateInit(&launcherState);
	standaloneClockRunning = false;
	standaloneClockTick = standaloneClockRow = 0;
}

void sampleLauncherFree(void)
{
	sampleLauncherForgetBanks();
}
