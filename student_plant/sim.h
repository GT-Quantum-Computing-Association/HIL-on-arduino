#pragma once
#include <Arduino.h>

// Write sim.cpp. Recipe for one measurement window of WINDOW_NS (500 ns):
//
// Settings (set from serial in the .ino):
extern long simRate;       // pairs per second
extern long simDelayNs;    // optical delay added to arm B only
extern long simDark;       // dark counts per second

void simInit(char arm);    // remember 'A' or 'B', seed your private random stream
                           // (use different seeds for A and B)
//
// 1. Pairs: number of pairs in the window ~ Poisson(simRate * window seconds).
//    Both plants must draw the SAME pair count and emission times: seed a shared random
//    stream from seq (e.g. seq * 2654435761 + 12345), and draw count and times from it.
//    Number pairs 1,2,3... (the pair ID).
// 2. For each pair, draw the emission time uniformly in the window (shared stream), then
//    keep the photon with probability P = 0.2 * 0.6 = 0.12 (PRIVATE stream, so A and B
//    survive independently).
// 3. Detection time = emission + (arm B only: simDelayNs) + Gaussian jitter.
//    sigma = 1000 ps / 2.355 = 425 ps.
// 4. Dark counts: Poisson(simDark * window seconds), uniform in the window, pair ID 0.
// 5. Sort all events by time.
// 6. Detector dead time 45 ns: drop an event less than 45 ns after the previous detection.
// 7. TDC limits: drop events before 12 ns or after the window; drop a stop less than 67 ns
//    after the previous recorded stop. Stop after numStop stops.
// Poisson and Gaussian helpers: write your own (xorshift random, Knuth Poisson,
// Box-Muller). Keep arrays small, an Uno has 2 KB of RAM.
//
// Returns the number of recorded stops. stopT[] gets times in picoseconds (from START),
// stopId[] gets the pair IDs.
uint8_t simulate(uint32_t seq, uint8_t numStop, long stopT[], uint8_t stopId[]);