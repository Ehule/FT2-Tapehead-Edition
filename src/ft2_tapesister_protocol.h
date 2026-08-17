#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TAPEHEAD_EXCHANGE_MAX_ITEMS 16
#define TAPEHEAD_EXCHANGE_FILENAME_CAPACITY 256
#define TAPEHEAD_EXCHANGE_APP_CAPACITY 16

typedef enum tapeheadExchangeLayout_t
{
	TAPEHEAD_EXCHANGE_LAYOUT_INVALID = 0,
	TAPEHEAD_EXCHANGE_LAYOUT_INSTRUMENT_SAMPLES,
	TAPEHEAD_EXCHANGE_LAYOUT_SEPARATE_INSTRUMENTS
} tapeheadExchangeLayout_t;

typedef struct tapeheadExchangeItem_t
{
	uint8_t tapeSisterTile;
	uint8_t ft2Instrument;
	uint8_t ft2Sample;
	char filename[TAPEHEAD_EXCHANGE_FILENAME_CAPACITY];
} tapeheadExchangeItem_t;

typedef struct tapeheadExchangeOffer_t
{
	char sender[TAPEHEAD_EXCHANGE_APP_CAPACITY];
	char recipient[TAPEHEAD_EXCHANGE_APP_CAPACITY];
	tapeheadExchangeLayout_t layout;
	uint8_t count;
	tapeheadExchangeItem_t items[TAPEHEAD_EXCHANGE_MAX_ITEMS];
} tapeheadExchangeOffer_t;

typedef struct tapeheadExchangeDestination_t
{
	uint8_t instrument;
	uint8_t sample;
} tapeheadExchangeDestination_t;

void tapeheadExchangeOfferInit(tapeheadExchangeOffer_t *offer);
const char *tapeheadExchangeLayoutName(tapeheadExchangeLayout_t layout);
bool tapeheadExchangeFilenameIsSafe(const char *filename);
bool tapeheadExchangeParseManifest(FILE *file, tapeheadExchangeOffer_t *offer,
	char *error, size_t errorSize);
bool tapeheadExchangeParseManifestPath(const char *path,
	tapeheadExchangeOffer_t *offer, char *error, size_t errorSize);

/* Resolve a TapeSister -> Tapehead offer against the destination chosen by
** the user. Separate-instrument manifest values are relative positions. */
bool tapeheadExchangeResolveDestinations(const tapeheadExchangeOffer_t *offer,
	uint8_t startingInstrument, tapeheadExchangeDestination_t *destinations,
	char *error, size_t errorSize);
