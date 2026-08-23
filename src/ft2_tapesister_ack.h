#pragma once

#include <stdbool.h>

#include "ft2_unicode.h"

/* Write the receiver acknowledgement through a sibling temporary file and a
** single rename, so inbox observers can never see partial contents. */
bool tapeheadExchangeWriteAcknowledgement(const UNICHAR *pathU);
