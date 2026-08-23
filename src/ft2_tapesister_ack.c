#include "ft2_tapesister_ack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool tapeheadExchangeWriteAcknowledgement(const UNICHAR *pathU)
{
	if (pathU == NULL)
		return false;
	const size_t length = UNICHAR_STRLEN(pathU);
	UNICHAR *temporaryU = malloc((length + 5) * sizeof (UNICHAR));
	if (temporaryU == NULL)
		return false;
	UNICHAR_STRCPY(temporaryU, pathU);
#ifdef _WIN32
	UNICHAR_STRCAT(temporaryU, L".tmp");
#else
	UNICHAR_STRCAT(temporaryU, ".tmp");
#endif
	FILE *file = UNICHAR_FOPEN(temporaryU, "wb");
	bool ok = false;
	if (file != NULL)
	{
		const bool wrote = fputs("recipient=tapehead\nstatus=imported\n", file) >= 0;
		const bool closed = fclose(file) == 0;
		file = NULL;
		ok = wrote && closed;
		if (ok)
			ok = UNICHAR_RENAME(temporaryU, pathU) == 0;
	}
	if (!ok)
	{
		if (file != NULL)
			fclose(file);
		UNICHAR_REMOVE(temporaryU);
	}
	free(temporaryU);
	return ok;
}
