#include "ft2_tapesister_protocol.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define FT2_EXCHANGE_MAX_INSTRUMENTS 128
#define FT2_EXCHANGE_MAX_SAMPLES 16

static void setError(char *error, size_t errorSize, const char *message)
{
	if (error == NULL || errorSize == 0)
		return;

	if (message == NULL)
		message = "";
	strncpy(error, message, errorSize - 1);
	error[errorSize - 1] = '\0';
}

void tapeheadExchangeOfferInit(tapeheadExchangeOffer_t *offer)
{
	if (offer != NULL)
		memset(offer, 0, sizeof (*offer));
}

const char *tapeheadExchangeLayoutName(tapeheadExchangeLayout_t layout)
{
	switch (layout)
	{
		case TAPEHEAD_EXCHANGE_LAYOUT_INSTRUMENT_SAMPLES:
			return "instrument_samples";
		case TAPEHEAD_EXCHANGE_LAYOUT_SEPARATE_INSTRUMENTS:
			return "separate_instruments";
		default:
			return "invalid";
	}
}

bool tapeheadExchangeFilenameIsSafe(const char *filename)
{
	if (filename == NULL || filename[0] == '\0' ||
		strlen(filename) >= TAPEHEAD_EXCHANGE_FILENAME_CAPACITY ||
		strstr(filename, "..") != NULL)
	{
		return false;
	}

	for (const unsigned char *p = (const unsigned char *)filename; *p != '\0'; p++)
	{
		if (!isalnum(*p) && *p != '.' && *p != '-' && *p != '_')
			return false;
	}

	return true;
}

static bool parseNumber(const char *text, unsigned int maximum,
	unsigned int *value)
{
	if (text == NULL || text[0] == '\0' || value == NULL)
		return false;
	for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++)
		if (!isdigit(*p))
			return false;

	errno = 0;
	char *end = NULL;
	const unsigned long parsed = strtoul(text, &end, 10);
	if (errno == ERANGE || end == text || *end != '\0' || parsed > maximum)
		return false;

	*value = (unsigned int)parsed;
	return true;
}

static bool parseItem(char *value, tapeheadExchangeItem_t *item)
{
	char *fields[4];
	char *cursor = value;
	for (uint8_t i = 0; i < 4; i++)
	{
		fields[i] = cursor;
		char *comma = strchr(cursor, ',');
		if (i < 3)
		{
			if (comma == NULL)
				return false;
			*comma = '\0';
			cursor = comma + 1;
		}
		else if (comma != NULL)
		{
			return false;
		}
	}

	unsigned int tile, instrument, sample;
	if (!parseNumber(fields[0], TAPEHEAD_EXCHANGE_MAX_ITEMS, &tile) || tile == 0 ||
		!parseNumber(fields[1], FT2_EXCHANGE_MAX_INSTRUMENTS, &instrument) ||
		!parseNumber(fields[2], FT2_EXCHANGE_MAX_SAMPLES, &sample) || sample == 0 ||
		!tapeheadExchangeFilenameIsSafe(fields[3]))
	{
		return false;
	}

	item->tapeSisterTile = (uint8_t)tile;
	item->ft2Instrument = (uint8_t)instrument;
	item->ft2Sample = (uint8_t)sample;
	strncpy(item->filename, fields[3], sizeof (item->filename) - 1);
	item->filename[sizeof (item->filename) - 1] = '\0';
	return true;
}

static bool knownEndpoint(const char *name)
{
	return strcmp(name, "tapesister") == 0 || strcmp(name, "tapehead") == 0;
}

static bool validateOffer(const tapeheadExchangeOffer_t *offer,
	char *error, size_t errorSize)
{
	if (!knownEndpoint(offer->sender) || !knownEndpoint(offer->recipient) ||
		strcmp(offer->sender, offer->recipient) == 0)
	{
		setError(error, errorSize, "Invalid sender or recipient");
		return false;
	}
	if (offer->layout == TAPEHEAD_EXCHANGE_LAYOUT_INVALID)
	{
		setError(error, errorSize, "Unsupported exchange layout");
		return false;
	}
	if (offer->count == 0 || offer->count > TAPEHEAD_EXCHANGE_MAX_ITEMS)
	{
		setError(error, errorSize, "Exchange count must be 1 through 16");
		return false;
	}

	bool usedTiles[TAPEHEAD_EXCHANGE_MAX_ITEMS + 1] = { false };
	bool usedSamples[FT2_EXCHANGE_MAX_SAMPLES + 1] = { false };
	bool usedInstruments[FT2_EXCHANGE_MAX_INSTRUMENTS + 1] = { false };
	for (uint8_t i = 0; i < offer->count; i++)
	{
		const tapeheadExchangeItem_t *item = &offer->items[i];
		if (usedTiles[item->tapeSisterTile])
		{
			setError(error, errorSize, "Duplicate TapeSister tile target");
			return false;
		}
		usedTiles[item->tapeSisterTile] = true;

		if (offer->layout == TAPEHEAD_EXCHANGE_LAYOUT_INSTRUMENT_SAMPLES)
		{
			if (usedSamples[item->ft2Sample])
			{
				setError(error, errorSize, "Duplicate FT2 sample target");
				return false;
			}
			usedSamples[item->ft2Sample] = true;
			if ((strcmp(offer->sender, "tapesister") == 0 && item->ft2Instrument != 0) ||
				(strcmp(offer->sender, "tapehead") == 0 && item->ft2Instrument == 0))
			{
				setError(error, errorSize, "Invalid instrument_samples instrument value");
				return false;
			}
		}
		else
		{
			if (item->ft2Instrument == 0 ||
				(strcmp(offer->sender, "tapesister") == 0 &&
				 item->ft2Instrument > TAPEHEAD_EXCHANGE_MAX_ITEMS))
			{
				setError(error, errorSize, "Invalid separate-instruments position");
				return false;
			}
			if (usedInstruments[item->ft2Instrument])
			{
				setError(error, errorSize, "Duplicate FT2 instrument mapping");
				return false;
			}
			usedInstruments[item->ft2Instrument] = true;
		}
	}

	setError(error, errorSize, "");
	return true;
}

bool tapeheadExchangeParseManifest(FILE *file, tapeheadExchangeOffer_t *offer,
	char *error, size_t errorSize)
{
	if (file == NULL || offer == NULL)
	{
		setError(error, errorSize, "Invalid manifest input");
		return false;
	}

	tapeheadExchangeOffer_t parsed;
	tapeheadExchangeOfferInit(&parsed);
	bool headerSeen = false, senderSeen = false, recipientSeen = false;
	bool layoutSeen = false, countSeen = false;
	unsigned int declaredCount = 0;
	char line[1024];
	while (fgets(line, sizeof (line), file) != NULL)
	{
		const size_t length = strlen(line);
		if (length == sizeof (line) - 1 && line[length - 1] != '\n' && !feof(file))
		{
			setError(error, errorSize, "Manifest line is too long");
			return false;
		}
		while (line[0] != '\0')
		{
			const size_t last = strlen(line) - 1;
			if (line[last] != '\n' && line[last] != '\r')
				break;
			line[last] = '\0';
		}
		if (line[0] == '\0')
			continue;

		if (!headerSeen)
		{
			if (strcmp(line, "TAPESISTER_EXCHANGE 1") != 0)
			{
				setError(error, errorSize, "Unsupported exchange version");
				return false;
			}
			headerSeen = true;
			continue;
		}

		char *equals = strchr(line, '=');
		if (equals == NULL)
		{
			setError(error, errorSize, "Malformed manifest field");
			return false;
		}
		*equals = '\0';
		char *value = equals + 1;
		if (strcmp(line, "sender") == 0)
		{
			if (senderSeen || strlen(value) >= sizeof (parsed.sender))
				goto malformed;
			strcpy(parsed.sender, value);
			senderSeen = true;
		}
		else if (strcmp(line, "recipient") == 0)
		{
			if (recipientSeen || strlen(value) >= sizeof (parsed.recipient))
				goto malformed;
			strcpy(parsed.recipient, value);
			recipientSeen = true;
		}
		else if (strcmp(line, "layout") == 0)
		{
			if (layoutSeen)
				goto malformed;
			if (strcmp(value, "instrument_samples") == 0)
				parsed.layout = TAPEHEAD_EXCHANGE_LAYOUT_INSTRUMENT_SAMPLES;
			else if (strcmp(value, "separate_instruments") == 0)
				parsed.layout = TAPEHEAD_EXCHANGE_LAYOUT_SEPARATE_INSTRUMENTS;
			else
			{
				setError(error, errorSize, "Unsupported exchange layout");
				return false;
			}
			layoutSeen = true;
		}
		else if (strcmp(line, "count") == 0)
		{
			if (countSeen || !parseNumber(value, TAPEHEAD_EXCHANGE_MAX_ITEMS,
				&declaredCount) || declaredCount == 0)
			{
				goto malformed;
			}
			countSeen = true;
		}
		else if (strcmp(line, "item") == 0)
		{
			if (parsed.count >= TAPEHEAD_EXCHANGE_MAX_ITEMS ||
				!parseItem(value, &parsed.items[parsed.count]))
			{
				setError(error, errorSize, "Malformed exchange item");
				return false;
			}
			parsed.count++;
		}
		else
		{
			setError(error, errorSize, "Unsupported manifest field");
			return false;
		}
	}

	if (ferror(file))
	{
		setError(error, errorSize, "Could not read exchange manifest");
		return false;
	}
	if (!headerSeen || !senderSeen || !recipientSeen || !layoutSeen || !countSeen ||
		declaredCount != parsed.count)
	{
		setError(error, errorSize, "Incomplete manifest or malformed count");
		return false;
	}
	if (!validateOffer(&parsed, error, errorSize))
		return false;

	*offer = parsed;
	return true;

malformed:
	setError(error, errorSize, "Duplicate or malformed manifest field");
	return false;
}

bool tapeheadExchangeParseManifestPath(const char *path,
	tapeheadExchangeOffer_t *offer, char *error, size_t errorSize)
{
	if (path == NULL)
	{
		setError(error, errorSize, "Invalid manifest path");
		return false;
	}
	FILE *file = fopen(path, "rb");
	if (file == NULL)
	{
		setError(error, errorSize, "Could not open exchange manifest");
		return false;
	}
	const bool ok = tapeheadExchangeParseManifest(file, offer, error, errorSize);
	fclose(file);
	return ok;
}

bool tapeheadExchangeResolveDestinations(const tapeheadExchangeOffer_t *offer,
	uint8_t startingInstrument, tapeheadExchangeDestination_t *destinations,
	char *error, size_t errorSize)
{
	if (offer == NULL || destinations == NULL || startingInstrument == 0 ||
		startingInstrument > FT2_EXCHANGE_MAX_INSTRUMENTS ||
		strcmp(offer->sender, "tapesister") != 0 ||
		strcmp(offer->recipient, "tapehead") != 0)
	{
		setError(error, errorSize, "Transfer is not addressed to Tapehead");
		return false;
	}

	bool used[FT2_EXCHANGE_MAX_INSTRUMENTS + 1][FT2_EXCHANGE_MAX_SAMPLES + 1] =
		{ { false } };
	for (uint8_t i = 0; i < offer->count; i++)
	{
		const tapeheadExchangeItem_t *item = &offer->items[i];
		unsigned int instrument = startingInstrument;
		if (offer->layout == TAPEHEAD_EXCHANGE_LAYOUT_SEPARATE_INSTRUMENTS)
			instrument += item->ft2Instrument - 1;
		if (instrument == 0 || instrument > FT2_EXCHANGE_MAX_INSTRUMENTS)
		{
			setError(error, errorSize, "Destination instrument range exceeds 128");
			return false;
		}
		if (used[instrument][item->ft2Sample])
		{
			setError(error, errorSize, "Duplicate resolved FT2 destination");
			return false;
		}
		used[instrument][item->ft2Sample] = true;
		destinations[i].instrument = (uint8_t)instrument;
		destinations[i].sample = item->ft2Sample;
	}

	setError(error, errorSize, "");
	return true;
}
