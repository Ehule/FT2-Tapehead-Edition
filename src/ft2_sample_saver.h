#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_unicode.h"

enum
{
	SAVE_NORMAL = 0,
	SAVE_RANGE = 1
};

void saveSample(UNICHAR *filenameU, bool saveAsRange);
void setEXSExportUsedOnly(bool usedOnly);

/*
** Exports every populated sample to numbered WAV files in SampleSet/.
** This is currently the non-UI proof-of-concept entry point.
*/
bool exportSampleSet(const UNICHAR *directoryU, bool usedOnly);
