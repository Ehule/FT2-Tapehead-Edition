#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TAPEHEAD_EXCHANGE_MAX_V1_ITEMS 16
#define TAPEHEAD_EXCHANGE_MAX_PAGE_INSTRUMENTS 255
#define TAPEHEAD_EXCHANGE_MAX_ITEMS \
	(TAPEHEAD_EXCHANGE_MAX_PAGE_INSTRUMENTS * TAPEHEAD_EXCHANGE_MAX_V1_ITEMS)
#define TAPEHEAD_EXCHANGE_FILENAME_CAPACITY 256
#define TAPEHEAD_EXCHANGE_APP_CAPACITY 16

typedef enum tapeheadExchangeLayout_t
{
	TAPEHEAD_EXCHANGE_LAYOUT_INVALID = 0,
	TAPEHEAD_EXCHANGE_LAYOUT_INSTRUMENT_SAMPLES,
	TAPEHEAD_EXCHANGE_LAYOUT_SEPARATE_INSTRUMENTS,
	TAPEHEAD_EXCHANGE_LAYOUT_PAGE_INSTRUMENTS
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
	uint8_t version;
	char sender[TAPEHEAD_EXCHANGE_APP_CAPACITY];
	char recipient[TAPEHEAD_EXCHANGE_APP_CAPACITY];
	tapeheadExchangeLayout_t layout;
	uint16_t count, itemCapacity;
	tapeheadExchangeItem_t *items;
} tapeheadExchangeOffer_t;

typedef struct tapeheadExchangeDestination_t
{
	uint8_t instrument;
	uint8_t sample;
} tapeheadExchangeDestination_t;

void tapeheadExchangeOfferInit(tapeheadExchangeOffer_t *offer);
void tapeheadExchangeOfferFree(tapeheadExchangeOffer_t *offer);
bool tapeheadExchangeOfferReserve(tapeheadExchangeOffer_t *offer,
	uint16_t capacity);
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
uint16_t tapeheadExchangeRelativeInstrumentSpan(
	const tapeheadExchangeOffer_t *offer);
