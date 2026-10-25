#include "tdc_contract.h"
#include "spi_peripheral.h"
#include "sim.h"

const char MY_ARM = 'A';          // CHANGE TO 'B' FOR THE OTHER BOARD

bool golden = false;              // g<0|1>: send the packet-exercise example

static void measure() {
  uint32_t seq = ((uint32_t)getReg(REG_SEQ) << 24) | ((uint32_t)getReg(REG_SEQ + 1) << 16) |
                 ((uint32_t)getReg(REG_SEQ + 2) << 8) | getReg(REG_SEQ + 3);
  uint8_t numStop = (getReg(REG_CONFIG2) & 0x07) + 1;
  if (numStop > 5) numStop = 5;

  uint32_t r[N_RESULT_REGS] = {0};
  uint8_t status;

  if (golden) {
    r[0] = 860; r[2] = 3865;                    // TIME1, TIME2
    r[11] = CAL1_COUNTS; r[12] = CAL2_COUNTS;
    status = STATUS_TIMEOUT;                    // 2 of 3 stops -> 0x1A
  } else {
    long stopT[5]; uint8_t stopId[5];
    uint8_t n = simulate(seq, numStop, stopT, stopId);
    for (uint8_t i = 0; i < n; i++) {
      r[2 * i] = (uint32_t)(stopT[i] / 1000.0f / TDC_LSB_NS + 0.5f);
    }
    r[11] = CAL1_COUNTS; r[12] = CAL2_COUNTS;
    status = (n == numStop) ? STATUS_DONE : STATUS_TIMEOUT;
    if (n > 0) {                                // truth line
      Serial.print(F("T,")); Serial.print(seq); Serial.print(',');
      Serial.print(MY_ARM); Serial.print(','); Serial.print(n);
      for (uint8_t i = 0; i < n; i++) {
        Serial.print(','); Serial.print(stopT[i]);
        Serial.print(','); Serial.print(stopId[i]);
      }
      Serial.println();
    }
  }
  publish(r, status);
}

static void handleLine(char *s) {
  long v = atol(s + 1);
  switch (s[0]) {
    case 'r': simRate = v;        break;
    case 'd': simDelayNs = v;     break;
    case 'k': simDark = v;        break;
    case 'f': intbFault = (v != 0); if (intbFault) intbRelease(); break;
    case 'g': golden = (v != 0);  break;
    default: Serial.println(F("? use r d k f g")); return;
  }
  Serial.print(F("OK arm=")); Serial.print(MY_ARM);
  Serial.print(F(" r="));  Serial.print(simRate);
  Serial.print(F(" d="));  Serial.print(simDelayNs);
  Serial.print(F(" k="));  Serial.print(simDark);
  Serial.print(F(" f="));  Serial.print(intbFault);
  Serial.print(F(" g="));  Serial.println(golden);
}

static void pollSerial() {
  static char buf[16];
  static uint8_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (len) { buf[len] = 0; handleLine(buf); len = 0; }
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  simInit(MY_ARM);
  spiPeripheralBegin();
  Serial.print(F("Plant ")); Serial.print(MY_ARM); Serial.println(F(" ready"));
}

void loop() {
  pollSerial();
  if (takeStartRequest()) measure();
}