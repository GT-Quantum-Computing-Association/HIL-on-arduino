#pragma once
#include <Arduino.h>
#include "tdc_contract.h"

// Write spi_peripheral.cpp. You are an SPI peripheral (mode 0, MSB first) on the
// hardware pins: CS = D10 (PB2), COPI = D11, CIPO = D12 (PB4), SCK = D13.
// Do NOT call SPI.begin() (that makes you a controller). Use SPCR/SPDR and ISR(SPI_STC_vect).
//
// Register image: keep a byte array (0x30 bytes is enough) for CONFIG1/CONFIG2/INT_STATUS/SEQ
// and a uint32_t array of N_RESULT_REGS for 0x10..0x1C (24-bit registers, sent MSB first).
//
// Byte state machine:
//   1. First byte after CS falls is the command: bit7 auto-increment, bit6 write, bits5:0 address.
//      The reply to the command byte is don't-care.
//   2. Following bytes are data. Read: send the register's bytes (3 bytes for result regs, 1 for
//      the others). Write: store the byte. With auto-increment, move to the next register after
//      each register's last byte.
//   3. Load the next reply byte into SPDR inside the ISR, before the next byte arrives.
//   4. CS rising resets the state machine to "expect command". Use a pin-change interrupt on D10
//      (PCICR/PCMSK0, ISR(PCINT0_vect)).
//   5. Two peripherals share CIPO: float CIPO (D12 as input) while CS is high, drive it while
//      CS is low.
//
// Register behaviors:
//   - Writing CFG1_START_MEAS to CONFIG1: clear that bit, clear INT_STATUS, release INTB,
//     and set a start flag for the main loop.
//   - Reading INT_STATUS releases INTB.
//   - INT_STATUS and the result registers are read-only to the controller.
//   - CONFIG2 powers up as CFG2_DEFAULT.
//
// INTB is D8 (PB0), open-drain style: pull LOW by driving the pin low, release by making it
// an input (high-Z). The controller has the pull-up.

extern bool intbFault;                 // when true, publish() never pulls INTB low

void spiPeripheralBegin();             // configure pins, SPCR, interrupts, defaults
void intbRelease();                    // INTB to high-Z
bool takeStartRequest();               // true once per START_MEAS write, then clears
uint8_t getReg(uint8_t addr);          // read a byte from the register image
// Copy r[] into the result registers, set INT_STATUS = status, then pull INTB low
// (unless intbFault). Do the copy with interrupts off so a read can't see half a result.
void publish(const uint32_t r[N_RESULT_REGS], uint8_t status);