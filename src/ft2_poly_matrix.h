#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_replayer.h"

#define POLY_MATRIX_MAX_SPOOLS 4
#define POLY_MATRIX_MAX_THREADS 8

/*
** Runtime-only Poly Matrix proof of concept.
**
** A spool is the bundle of populated source tracks in a Matrix pattern. Each
** source track is routed to an automatically selected physical channel
** ("tunnel"). The source supplies note data while the destination keeps its
** mixer trim/output and FasTracks ratio/direction personality.
*/
bool polyMatrixTogglePattern(uint8_t patternNum, bool immediate);
bool polyMatrixSchedulePatternStop(uint8_t patternNum);
bool polyMatrixIsPatternActive(uint8_t patternNum);
bool polyMatrixPatternStopPending(uint8_t patternNum);
bool polyMatrixPatternQHandoffPending(uint8_t patternNum);
uint8_t polyMatrixGetPatternSlot(uint8_t patternNum);
int32_t polyMatrixGetDestination(uint8_t patternNum);
int32_t polyMatrixGetDestinationForSource(uint8_t patternNum,
	uint8_t sourceChannel);
uint8_t polyMatrixGetActiveCount(void);
void polyMatrixReset(void);

bool polyMatrixRequestQHandoff(uint8_t patternNum);
bool polyMatrixClaimReadyQHandoff(uint8_t *patternNum);
void polyMatrixCompleteQHandoff(uint8_t patternNum);
void polyMatrixCompleteQHandoffAtBoundary(uint8_t patternNum);

/*
** Poly Matrix owns an audio clock independent of the ordinary Pattern/Song
** transport. Release work remains pending for one audio tick after a spool is
** pulled so its destination voice receives a clean note-off.
*/
bool polyMatrixHasAudioWork(void);
bool polyMatrixConsumeDestinationRelease(int32_t destinationChannel);
void polyMatrixIsolateEventFromMainTransport(note_t *event);
bool polyMatrixDestinationAvailableToQ(int32_t destinationChannel,
	int16_t handoffPattern);

/* Audio-thread interface. */
bool polyMatrixStartPatternAtBoundary(uint8_t patternNum);
bool polyMatrixOwnsDestination(int32_t destinationChannel);
int32_t polyMatrixAdvanceAudio(int32_t destinationChannel, uint16_t tpl,
	const note_t **notes, int32_t maxNotes);
