#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ft2_tapesister_protocol.h"

static bool parseText(const char *text, tapeheadExchangeOffer_t *offer,
	char *error, size_t errorSize)
{
	tapeheadExchangeOfferInit(offer);
	FILE *file = tmpfile();
	assert(file != NULL);
	assert(fputs(text, file) >= 0);
	rewind(file);
	const bool result = tapeheadExchangeParseManifest(file, offer, error,
		errorSize);
	fclose(file);
	return result;
}

static void expectRejected(const char *manifest, const char *errorPart)
{
	tapeheadExchangeOffer_t offer;
	char error[192];
	assert(!parseText(manifest, &offer, error, sizeof (error)));
	assert(strstr(error, errorPart) != NULL);
	tapeheadExchangeOfferFree(&offer);
}

static void testVersion1InstrumentSamples(void)
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
	assert(offer.version == 1);
	assert(offer.layout == TAPEHEAD_EXCHANGE_LAYOUT_INSTRUMENT_SAMPLES);
	assert(offer.count == 2);
	assert(offer.items[1].tapeSisterTile == 4);
	assert(offer.items[1].ft2Sample == 4);

	tapeheadExchangeDestination_t destinations[2];
	assert(tapeheadExchangeResolveDestinations(&offer, 27, destinations,
		error, sizeof (error)));
	assert(destinations[0].instrument == 27 && destinations[0].sample == 1);
	assert(destinations[1].instrument == 27 && destinations[1].sample == 4);
	tapeheadExchangeOfferFree(&offer);
}

static void testVersion1SeparateInstruments(void)
{
	static const char manifest[] =
		"TAPESISTER_EXCHANGE 1\n"
		"sender=tapesister\nrecipient=tapehead\n"
		"layout=separate_instruments\ncount=3\n"
		"item=1,1,1,01_A.wav\n"
		"item=6,3,2,06_B.wav\n"
		"item=9,8,1,09_C.wav\n";
	tapeheadExchangeOffer_t offer;
	tapeheadExchangeDestination_t destinations[3];
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
	tapeheadExchangeOfferFree(&offer);
}

static void testVersion2TwoPagesAndSelectedStart(void)
{
	static const char manifest[] =
		"TAPESISTER_EXCHANGE 2\n"
		"sender=tapesister\nrecipient=tapehead\n"
		"layout=page_instruments\ncount=3\n"
		"item=1,1,1,P001_01_Kick.wav\n"
		"item=4,1,4,P001_04_Noise.wav\n"
		"item=1,2,1,P002_01_Bass.wav\n";
	tapeheadExchangeOffer_t offer;
	tapeheadExchangeDestination_t destinations[3];
	char error[192];
	assert(parseText(manifest, &offer, error, sizeof (error)));
	assert(offer.version == 2);
	assert(offer.layout == TAPEHEAD_EXCHANGE_LAYOUT_PAGE_INSTRUMENTS);
	assert(offer.count == 3);
	assert(tapeheadExchangeRelativeInstrumentSpan(&offer) == 2);

	/* Page 1 starts at the user's chosen instrument 5, page 2 at 6. The
	** repeated TapeSister tile/sample 1 is valid on the second page. */
	assert(tapeheadExchangeResolveDestinations(&offer, 5, destinations,
		error, sizeof (error)));
	assert(destinations[0].instrument == 5 && destinations[0].sample == 1);
	assert(destinations[1].instrument == 5 && destinations[1].sample == 4);
	assert(destinations[2].instrument == 6 && destinations[2].sample == 1);
	tapeheadExchangeOfferFree(&offer);
}

static void testVersion2SparsePagesAndSlots(void)
{
	static const char manifest[] =
		"TAPESISTER_EXCHANGE 2\n"
		"sender=tapesister\nrecipient=tapehead\n"
		"layout=page_instruments\ncount=3\n"
		"item=2,1,2,P001_02_A.wav\n"
		"item=16,1,16,P001_16_B.wav\n"
		"item=7,3,7,P003_07_C.wav\n";
	tapeheadExchangeOffer_t offer;
	tapeheadExchangeDestination_t destinations[3];
	char error[192];
	assert(parseText(manifest, &offer, error, sizeof (error)));
	assert(tapeheadExchangeRelativeInstrumentSpan(&offer) == 3);
	assert(tapeheadExchangeResolveDestinations(&offer, 40, destinations,
		error, sizeof (error)));
	assert(destinations[0].instrument == 40 && destinations[0].sample == 2);
	assert(destinations[1].instrument == 40 && destinations[1].sample == 16);
	assert(destinations[2].instrument == 42 && destinations[2].sample == 7);
	assert(!tapeheadExchangeResolveDestinations(&offer, 127, destinations,
		error, sizeof (error)));
	assert(strstr(error, "exceeds 128") != NULL);
	tapeheadExchangeOfferFree(&offer);
}

static void testVersion2DynamicItemStorage(void)
{
	char manifest[4096];
	size_t used = (size_t)snprintf(manifest, sizeof (manifest),
		"TAPESISTER_EXCHANGE 2\n"
		"sender=tapesister\nrecipient=tapehead\n"
		"layout=page_instruments\ncount=17\n");
	for (uint8_t page = 1; page <= 2; page++)
	{
		const uint8_t last = page == 1 ? 16 : 1;
		for (uint8_t tile = 1; tile <= last; tile++)
		{
			used += (size_t)snprintf(manifest + used, sizeof (manifest) - used,
				"item=%u,%u,%u,P%03u_%02u.wav\n", tile, page, tile,
				page, tile);
		}
	}
	assert(used < sizeof (manifest));
	tapeheadExchangeOffer_t offer;
	char error[192];
	assert(parseText(manifest, &offer, error, sizeof (error)));
	assert(offer.count == 17);
	assert(offer.itemCapacity >= 17);
	tapeheadExchangeOfferFree(&offer);
}

static void testVersion2ProtocolMaximumRelativeInstrument(void)
{
	static const char manifest[] =
		"TAPESISTER_EXCHANGE 2\n"
		"sender=tapesister\nrecipient=tapehead\n"
		"layout=page_instruments\ncount=1\n"
		"item=16,255,16,P255_16.wav\n";
	tapeheadExchangeOffer_t offer;
	tapeheadExchangeDestination_t destination[1];
	char error[192];
	assert(parseText(manifest, &offer, error, sizeof (error)));
	assert(tapeheadExchangeRelativeInstrumentSpan(&offer) == 255);
	assert(!tapeheadExchangeResolveDestinations(&offer, 1, destination,
		error, sizeof (error)));
	assert(strstr(error, "exceeds 128") != NULL);
	tapeheadExchangeOfferFree(&offer);
}

static void testVersion2RejectedMappings(void)
{
	expectRejected(
		"TAPESISTER_EXCHANGE 2\nsender=tapesister\nrecipient=tapehead\n"
		"layout=page_instruments\ncount=2\nitem=1,1,1,A.wav\n"
		"item=1,1,1,B.wav\n", "Duplicate page_instruments");
	expectRejected(
		"TAPESISTER_EXCHANGE 2\nsender=tapesister\nrecipient=tapehead\n"
		"layout=page_instruments\ncount=1\nitem=4,1,3,A.wav\n",
		"tile/instrument/sample");
	expectRejected(
		"TAPESISTER_EXCHANGE 2\nsender=tapesister\nrecipient=tapehead\n"
		"layout=page_instruments\ncount=1\nitem=1,0,1,A.wav\n",
		"tile/instrument/sample");
	expectRejected(
		"TAPESISTER_EXCHANGE 2\nsender=tapehead\nrecipient=tapesister\n"
		"layout=page_instruments\ncount=1\nitem=1,1,1,A.wav\n",
		"TapeSister to Tapehead");
	expectRejected(
		"TAPESISTER_EXCHANGE 2\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\ncount=1\nitem=1,0,1,A.wav\n",
		"version 2");
	expectRejected(
		"TAPESISTER_EXCHANGE 3\nsender=tapesister\nrecipient=tapehead\n"
		"layout=page_instruments\ncount=1\nitem=1,1,1,A.wav\n",
		"version");
}

static void testUnsafeAndMalformedManifests(void)
{
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
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nsender=tapesister\n"
		"recipient=tapehead\nlayout=instrument_samples\ncount=1\n"
		"item=1,0,1,A.wav\n", "field");
	expectRejected(
		"TAPESISTER_EXCHANGE 1\nsender=tapesister\nrecipient=tapehead\n"
		"layout=instrument_samples\nitem=1,0,1,A.wav\n", "Incomplete");
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
	assert(parseText(instrumentManifest, &instrumentOffer, error,
		sizeof (error)));
	assert(instrumentOffer.items[1].tapeSisterTile == 4);
	assert(instrumentOffer.items[1].ft2Sample == 4);
	tapeheadExchangeOfferFree(&instrumentOffer);

	static const char manifest[] =
		"TAPESISTER_EXCHANGE 1\n"
		"sender=tapehead\nrecipient=tapesister\n"
		"layout=separate_instruments\ncount=2\n"
		"item=1,64,3,I064_S03.wav\n"
		"item=2,128,1,I128_S01.wav\n";
	tapeheadExchangeOffer_t offer;
	assert(parseText(manifest, &offer, error, sizeof (error)));
	assert(offer.items[1].ft2Instrument == 128);
	tapeheadExchangeDestination_t destinations[2];
	assert(!tapeheadExchangeResolveDestinations(&offer, 1, destinations,
		error, sizeof (error)));
	assert(strstr(error, "not addressed") != NULL);
	tapeheadExchangeOfferFree(&offer);
}

int main(void)
{
	testVersion1InstrumentSamples();
	testVersion1SeparateInstruments();
	testVersion2TwoPagesAndSelectedStart();
	testVersion2SparsePagesAndSlots();
	testVersion2DynamicItemStorage();
	testVersion2ProtocolMaximumRelativeInstrument();
	testVersion2RejectedMappings();
	testUnsafeAndMalformedManifests();
	testTapeheadOutgoingSemantics();
	puts("TapeSister exchange protocol tests passed.");
	return 0;
}
