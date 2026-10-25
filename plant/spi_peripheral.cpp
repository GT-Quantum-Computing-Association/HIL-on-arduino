#include <avr/interrupt.h>
#include "spi_peripheral.h"

bool intbFault = false;

static volatile uint8_t  regs8[0x30];
static volatile uint32_t res[N_RESULT_REGS];
static volatile uint8_t  spiState = 0;        // 0 = expect command, 1 = data
static volatile uint8_t  spiAddr = 0, spiSub = 0;
static volatile bool     spiWrite = false, spiAuto = false;
static volatile bool     startReq = false;

// INTB on D8 (PB0), open-drain style
static inline void intbLow() { PORTB &= ~_BV(PB0); DDRB |= _BV(PB0); }
void intbRelease()           { DDRB &= ~_BV(PB0); }

uint8_t getReg(uint8_t addr) { return regs8[addr]; }

bool takeStartRequest() {
  if (!startReq) return false;
  startReq = false;
  return true;
}

void publish(const uint32_t r[N_RESULT_REGS], uint8_t status) {
  noInterrupts();
  for (uint8_t i = 0; i < N_RESULT_REGS; i++) res[i] = r[i];
  regs8[REG_INT_STATUS] = status;
  interrupts();
  if (!intbFault) intbLow();
}

// Next byte of a read, honoring 24-bit result registers and auto-increment
static inline uint8_t nextByte() {
  uint8_t b, width = 1;
  if (spiAddr >= REG_TIME1 && spiAddr <= REG_CALIBRATION2) {
    uint32_t v = res[spiAddr - REG_TIME1];
    b = (uint8_t)(v >> (8 * (2 - spiSub)));
    width = 3;
  } else if (spiAddr < sizeof(regs8)) {
    b = regs8[spiAddr];
  } else {
    b = 0;
  }
  if (++spiSub >= width) { spiSub = 0; if (spiAuto) spiAddr++; }
  return b;
}

static inline void storeByte(uint8_t c) {
  uint8_t width = 1;
  if (spiAddr >= REG_TIME1 && spiAddr <= REG_CALIBRATION2) width = 3;  // read-only
  else if (spiAddr < sizeof(regs8) && spiAddr != REG_INT_STATUS) {
    regs8[spiAddr] = c;
    if (spiAddr == REG_CONFIG1 && (c & CFG1_START_MEAS)) {
      regs8[REG_CONFIG1] = c & ~CFG1_START_MEAS;   // self-clearing
      regs8[REG_INT_STATUS] = 0;
      intbRelease();
      startReq = true;
    }
  }
  if (++spiSub >= width) { spiSub = 0; if (spiAuto) spiAddr++; }
}

ISR(SPI_STC_vect) {
  uint8_t c = SPDR;
  if (spiState == 0) {
    spiWrite = (c & 0x40) != 0;
    spiAuto  = (c & 0x80) != 0;
    spiAddr  = c & 0x3F;
    spiSub   = 0;
    spiState = 1;
    if (!spiWrite) {
      if (spiAddr == REG_INT_STATUS) intbRelease();
      SPDR = nextByte();
    } else {
      SPDR = 0;
    }
  } else {
    if (spiWrite) { storeByte(c); SPDR = 0; }
    else          { SPDR = nextByte(); }
  }
}

// CS (D10 = PB2) change: tri-state CIPO (PB4) when deselected, reset state
ISR(PCINT0_vect) {
  spiState = 0;
  if (PINB & _BV(PB2)) {
    DDRB &= ~_BV(PB4);
  } else {
    DDRB |= _BV(PB4);
    SPDR = 0;
  }
}

void spiPeripheralBegin() {
  // Raw SPI peripheral on the hardware pins (do not call SPI.begin())
  DDRB &= ~(_BV(PB2) | _BV(PB3) | _BV(PB4) | _BV(PB5));  // CS, COPI, CIPO, SCK inputs
  PORTB |= _BV(PB2);                                      // CS pull-up
  PORTB &= ~_BV(PB0);                                     // INTB: latch low, high-Z
  DDRB &= ~_BV(PB0);
  regs8[REG_CONFIG2] = CFG2_DEFAULT;
  SPCR = _BV(SPE) | _BV(SPIE);                            // peripheral mode, mode 0, MSB first
  (void)SPSR; (void)SPDR;
  PCICR  |= _BV(PCIE0);
  PCMSK0 |= _BV(PCINT2);                                  // D10 only
  sei();
}