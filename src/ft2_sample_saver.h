#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_unicode.h"
#include "ft2_replayer.h"

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

/* Authoritative WAV writer for an arbitrary instrument/sample pair. It does
** not change the editor selection and is shared by EXS and TapeSister export. */
bool saveWAVSampleDirect(const UNICHAR *filenameU, instr_t *instrument,
	sample_t *sample);
