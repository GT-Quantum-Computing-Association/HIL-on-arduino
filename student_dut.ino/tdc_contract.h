#pragma once
// CANONICAL CONTRACT. Do not edit.

// Registers (TDC7200-like)
#define REG_CONFIG1      0x00
#define REG_CONFIG2      0x01
#define REG_INT_STATUS   0x02
#define REG_TIME1        0x10   // results 0x10..0x1C, 24 bits each, MSB first
#define REG_CALIBRATION1 0x1B
#define REG_CALIBRATION2 0x1C
#define REG_SEQ          0x20   // NOT a TDC7200 register: 4 bytes, big endian
#define N_RESULT_REGS    13     // 0x10..0x1C

// Command byte: bit7 auto-increment, bit6 write(1)/read(0), bits5:0 address
#define CMD_READ(a)   (0x80 | (a))
#define CMD_WRITE(a)  (0xC0 | (a))

// CONFIG1 bits
#define CFG1_START_MEAS 0x01    // self-clearing
#define CFG1_FORCE_CAL  0x80

// CONFIG2 used: bits7:6 CAL2 periods (01 = 10), bits2:0 NUM_STOP-1
#define CFG2_DEFAULT    0x44    // 10 cal periods, 5 stops

// INT_STATUS values
#define STATUS_DONE     0x19    // complete + started + new_meas
#define STATUS_TIMEOUT  0x1A    // complete + started + coarse overflow

// Chip numbers (mode 1, 16 MHz clock)
#define TDC_CLK_NS      62.5f
#define CAL2_PERIODS    10
#define TDC_LSB_NS      0.055f
#define CAL1_COUNTS     1136UL
#define CAL2_COUNTS     11364UL

// Timing / protocol
#define INTERBYTE_US    20      // controller pauses this long between bytes
#define WINDOW_NS       500