#pragma once
#include <Arduino.h>

// Write decode.cpp.
// calCount = (cal2 - cal1) / (cal2periods - 1)
// normLSB  = clkNs / calCount
// return time * normLSB        (nanoseconds, use float)
// Check: decodeNs(4175, 2110, 21121, 10, 125.0f) must give 247.061
float decodeNs(uint32_t time, uint32_t cal1, uint32_t cal2,
               uint8_t cal2periods, float clkNs);