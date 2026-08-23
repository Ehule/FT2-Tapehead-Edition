#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ft2_tapesister_ack.h"

int main(void)
{
	char directory[] = "/tmp/tapehead-ack-XXXXXX";
	assert(mkdtemp(directory) != NULL);

	char acknowledgement[512], temporary[516];
	assert(snprintf(acknowledgement, sizeof (acknowledgement),
		"%s/tapehead.received", directory) > 0);
	assert(snprintf(temporary, sizeof (temporary), "%s.tmp",
		acknowledgement) > 0);
	assert(tapeheadExchangeWriteAcknowledgement(acknowledgement));
	assert(access(temporary, F_OK) != 0);

	FILE *file = fopen(acknowledgement, "rb");
	assert(file != NULL);
	char contents[128] = { 0 };
	assert(fread(contents, 1, sizeof (contents) - 1, file) > 0);
	assert(fclose(file) == 0);
	assert(strcmp(contents,
		"recipient=tapehead\nstatus=imported\n") == 0);

	char failed[512];
	assert(snprintf(failed, sizeof (failed), "%s/missing/tapehead.received",
		directory) > 0);
	assert(!tapeheadExchangeWriteAcknowledgement(failed));
	assert(access(failed, F_OK) != 0);
	assert(!tapeheadExchangeWriteAcknowledgement(NULL));
	assert(remove(acknowledgement) == 0);
	assert(rmdir(directory) == 0);
	puts("TapeSister acknowledgement tests passed.");
	return 0;
}
