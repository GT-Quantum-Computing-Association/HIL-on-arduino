#include <SPI.h>
#include "spi_controller.h"

static const SPISettings spiCfg(1000000, MSBFIRST, SPI_MODE0);

static void csBegin(uint8_t cs) {
  SPI.beginTransaction(spiCfg);
  digitalWrite(cs, LOW);
  delayMicroseconds(INTERBYTE_US);
}

static void csEnd(uint8_t cs) {
  delayMicroseconds(INTERBYTE_US);      // let the peripheral's last ISR finish
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
  delayMicroseconds(INTERBYTE_US);
}

static uint8_t xfer(uint8_t b) {
  uint8_t r = SPI.transfer(b);
  delayMicroseconds(INTERBYTE_US);
  return r;
}

void spiControllerBegin(uint8_t csA, uint8_t csB) {
  pinMode(csA, OUTPUT); digitalWrite(csA, HIGH);
  pinMode(csB, OUTPUT); digitalWrite(csB, HIGH);
  SPI.begin();
}

void writeReg(uint8_t cs, uint8_t addr, uint8_t val) {
  csBegin(cs);
  xfer(CMD_WRITE(addr));
  xfer(val);
  csEnd(cs);
}

void writeBurst(uint8_t cs, uint8_t addr, const uint8_t *d, uint8_t n) {
  csBegin(cs);
  xfer(CMD_WRITE(addr));
  for (uint8_t i = 0; i < n; i++) xfer(d[i]);
  csEnd(cs);
}

uint8_t readReg(uint8_t cs, uint8_t addr) {
  csBegin(cs);
  xfer(CMD_READ(addr));
  uint8_t v = xfer(0x00);
  csEnd(cs);
  return v;
}

// TIME1..CALIBRATION2: 1 command byte + 39 data bytes
void readResult(uint8_t cs, uint32_t out[N_RESULT_REGS]) {
  uint8_t rx[1 + 3 * N_RESULT_REGS];
  csBegin(cs);
  rx[0] = xfer(CMD_READ(REG_TIME1));    // reply to the command byte is ignored
  for (uint8_t i = 1; i < sizeof(rx); i++) rx[i] = xfer(0x00);
  csEnd(cs);
  for (uint8_t i = 0; i < N_RESULT_REGS; i++) {
    out[i] = (((uint32_t)rx[1 + 3 * i] << 16) | ((uint32_t)rx[2 + 3 * i] << 8) |
              rx[3 + 3 * i]) & 0x7FFFFFUL;   // bit 23 is parity, ignored
  }
}