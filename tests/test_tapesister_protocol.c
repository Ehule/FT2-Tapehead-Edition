#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ft2_tapesister_protocol.h"

static bool parseText(const char *text, tapeheadExchangeOffer_t *offer,
	char *error, size_t errorSize)
{
	FILE *file = tmpfile();
	assert(file != NULL);
	assert(fputs(text, file) >= 0);
	rewind(file);
	const bool result = tapeheadExchangeParseManifest(file, offer, error,
		errorSize);
	fclose(file);
	return result;
}

static void testInstrumentSamplesAndSparseMapping(void)
{
	static const char manifest[] =
		"TAPESISTER_EXCHANGE 1\n"
		"sender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=2\n"
		"item=1,0,1,01_Kick.wav\n"
		"item=4,0,4,04_Noise.wav\n";
	tapeheadExchangeOffer_t offer;
	char error[192];
	assert(parseText(manifest, &offer, error, sizeof (error)));
	assert(offer.layout == TAPEHEAD_EXCHANGE_LAYOUT_INSTRUMENT_SAMPLES);
	assert(offer.count == 2);
	assert(offer.items[1].tapeSisterTile == 4);
	assert(offer.items[1].ft2Sample == 4);

	tapeheadExchangeDestination_t destinations[TAPEHEAD_EXCHANGE_MAX_ITEMS];
	assert(tapeheadExchangeResolveDestinations(&offer, 27, destinations,
		error, sizeof (error)));
	assert(destinations[0].instrument == 27 && destinations[0].sample == 1);
	assert(destinations[1].instrument == 27 && destinations[1].sample == 4);
}

static void testSeparateInstrumentRelativeMapping(void)
{
	static const char manifest[] =
		"TAPESISTER_EXCHANGE 1\n"
		"sender=tapesister\nrecipient=tapehead\n"
		"layout=separate_instruments\ncount=3\n"
		"item=1,1,1,01_A.wav\n"
		"item=6,3,2,06_B.wav\n"
		"item=9,8,1,09_C.wav\n";
	tapeheadExchangeOffer_t offer;
	tapeheadExchangeDestination_t destinations[TAPEHEAD_EXCHANGE_MAX_ITEMS];
	char error[192];
	assert(parseText(manifest, &offer, error, sizeof (error)));
	assert(tapeheadExchangeResolveDestinations(&offer, 100, destinations,
		error, sizeof (error)));
	assert(destinations[0].instrument == 100 && destinations[0].sample == 1);
	assert(destinations[1].instrument == 102 && destinations[1].sample == 2);
	assert(destinations[2].instrument == 107 && destinations[2].sample == 1);
	assert(!tapeheadExchangeResolveDestinations(&offer, 122, destinations,
		error, sizeof (error)));
	assert(strstr(error, "exceeds 128") != NULL);
}

static void expectRejected(const char *manifest, const char *errorPart)
{
	tapeheadExchangeOffer_t offer;
	char error[192];
	assert(!parseText(manifest, &offer, error, sizeof (error)));
	assert(strstr(error, errorPart) != NULL);
}

static void testUnsafeAndMalformedManifests(void)
{
	expectRejected(
		"TAPESISTER_EXCHANGE 2\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=1\nitem=1,0,1,A.wav\n",
		"version");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=command\ncount=1\nitem=1,0,1,A.wav\n", "layout");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=1\nitem=1,0,1,../A.wav\n",
		"item");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=1\nitem=1,0,1,/A.wav\n",
		"item");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=1\nitem=1,0,1,C:A.wav\n",
		"item");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=1\nitem=1,0,1,dir\\A.wav\n",
		"item");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=2\nitem=1,0,1,A.wav\n"
		"item=1,0,2,B.wav\n", "Duplicate TapeSister tile");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=2\nitem=1,0,1,A.wav\n"
		"item=2,0,1,B.wav\n", "Duplicate FT2 sample");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=2\nitem=1,0,1,A.wav\n",
		"count");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=1\nitem=17,0,1,A.wav\n",
		"item");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=+1\nitem=1,0,1,A.wav\n",
		"field");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=separate_instruments\ncount=1\nitem=1,17,1,A.wav\n",
		"position");
}

static void testTapeheadOutgoingSemantics(void)
{
	static const char instrumentManifest[] =
		"TAPESISTER_EXCHANGE 1\n"
		"sender=tapehead\nrecipient=tapesister\n"
		"layout=instrument_samples\ncount=2\n"
		"item=1,64,1,I064_S01.wav\n"
		"item=4,64,4,I064_S04.wav\n";
	tapeheadExchangeOffer_t instrumentOffer;
	char error[192];
	assert(parseText(instrumentManifest, &instrumentOffer, error, sizeof (error)));
	assert(instrumentOffer.items[1].tapeSisterTile == 4);
	assert(instrumentOffer.items[1].ft2Sample == 4);

	static const char manifest[] =
		"TAPESISTER_EXCHANGE 1\n"
		"sender=tapehead\nrecipient=tapesister\n"
		"layout=separate_instruments\ncount=2\n"
		"item=1,64,3,I064_S03.wav\n"
		"item=2,128,1,I128_S01.wav\n";
	tapeheadExchangeOffer_t offer;
	assert(parseText(manifest, &offer, error, sizeof (error)));
	assert(offer.items[1].ft2Instrument == 128);
	tapeheadExchangeDestination_t destinations[TAPEHEAD_EXCHANGE_MAX_ITEMS];
	assert(!tapeheadExchangeResolveDestinations(&offer, 1, destinations,
		error, sizeof (error)));
	assert(strstr(error, "not addressed") != NULL);
}

int main(void)
{
	testInstrumentSamplesAndSparseMapping();
	testSeparateInstrumentRelativeMapping();
	testUnsafeAndMalformedManifests();
	testTapeheadOutgoingSemantics();
	puts("TapeSister exchange protocol tests passed.");
	return 0;
}
