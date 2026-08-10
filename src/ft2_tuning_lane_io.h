#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

bool tuningLaneWriteXMExtension(FILE *f, uint16_t numPatterns);
bool tuningLaneReadXMExtension(FILE *f, uint32_t fileSize);
