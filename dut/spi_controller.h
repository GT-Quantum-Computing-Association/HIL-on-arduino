#pragma once
#include <Arduino.h>
#include "tdc_contract.h"

void spiControllerBegin(uint8_t csA, uint8_t csB);
void writeReg(uint8_t cs, uint8_t addr, uint8_t val);
void writeBurst(uint8_t cs, uint8_t addr, const uint8_t *d, uint8_t n);
uint8_t readReg(uint8_t cs, uint8_t addr);
void readResult(uint8_t cs, uint32_t out[N_RESULT_REGS]);