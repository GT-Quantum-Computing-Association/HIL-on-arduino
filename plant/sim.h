#pragma once
#include <Arduino.h>

extern long simRate;       // pairs/s   (r)
extern long simDelayNs;    // B delay   (d)
extern long simDark;       // dark/s    (k)

void simInit(char arm);
// Returns number of recorded stops; fills stop times (ps) and pair IDs
uint8_t simulate(uint32_t seq, uint8_t numStop, long stopT[], uint8_t stopId[]);