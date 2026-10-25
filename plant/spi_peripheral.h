#pragma once
#include <Arduino.h>
#include "tdc_contract.h"

extern bool intbFault;                 // true = never pull INTB low

void spiPeripheralBegin();
void intbRelease();
bool takeStartRequest();               // true once per START_MEAS write
uint8_t getReg(uint8_t addr);
void publish(const uint32_t r[N_RESULT_REGS], uint8_t status);