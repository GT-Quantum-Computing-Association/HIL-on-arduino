#include "decode.h"

float decodeNs(uint32_t time, uint32_t cal1, uint32_t cal2, uint8_t cal2periods, float clkNs) {
  float calCount = (float)(cal2 - cal1) / (cal2periods - 1);
  float normLSB = clkNs / calCount;
  return time * normLSB;
}
