#include <math.h>
#include "sim.h"
#include "tdc_contract.h"

long simRate    = 100000;
long simDelayNs = 20;
long simDark    = 250;

static char arm = 'A';

static const float P_DET      = 0.12f;      // 0.2 collection * 0.6 detection
static const float SIGMA_PS   = 425.0f;     // 1000 ps FWHM / 2.355
static const long  DEAD_PS    = 45000L;     // detector dead time
static const long  MIN_GAP_PS = 67000L;     // TDC min gap between stops
static const long  MIN_T_PS   = 12000L;     // mode 1 lower limit
static const long  WINDOW_PS  = (long)WINDOW_NS * 1000L;

struct Rng { uint32_t s; };
static Rng privRng;                         // survival, jitter, darks

static inline uint32_t rngNext(Rng &r) {
  uint32_t x = r.s;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  r.s = x;
  return x;
}
static inline float u01(Rng &r) {           // uniform in (0,1)
  return ((rngNext(r) >> 8) + 1) * (1.0f / 16777217.0f);
}
static float gauss(Rng &r) {
  float a = u01(r), b = u01(r);
  return sqrt(-2.0f * log(a)) * cos(6.2831853f * b);
}
static int poisson(Rng &r, float lambda, int cap) {
  float L = exp(-lambda), p = 1.0f;
  int k = 0;
  do { k++; p *= u01(r); } while (p > L && k < cap);
  return k - 1;
}

struct Ev { long t; uint8_t id; };
static Ev ev[40];
static uint8_t nEv;

static void addEv(float t_ps, uint8_t id) {
  if (nEv < 40 && t_ps > 0) { ev[nEv].t = (long)t_ps; ev[nEv].id = id; nEv++; }
}

void simInit(char a) {
  arm = a;
  privRng.s = (a == 'A') ? 0x1234ABCDUL : 0x9E3779B9UL;
}

uint8_t simulate(uint32_t seq, uint8_t numStop, long stopT[], uint8_t stopId[]) {
  nEv = 0;

  // Shared stream: both plants seed it from seq, so they draw the same pairs
  Rng pr;
  pr.s = seq * 2654435761UL + 12345UL;
  if (pr.s == 0) pr.s = 1;

  // Private stream gets a little real entropy each measurement
  privRng.s ^= micros() | 1;
  if (privRng.s == 0) privRng.s = 1;

  float delayPs = (arm == 'B') ? simDelayNs * 1000.0f : 0.0f;

  // Photon pairs
  int np = poisson(pr, (float)simRate * WINDOW_NS * 1e-9f, 24);
  for (int k = 1; k <= np; k++) {
    float tEmit = u01(pr) * WINDOW_PS;                  // shared draw (always taken)
    if (u01(privRng) < P_DET) {                         // private survival
      addEv(tEmit + delayPs + gauss(privRng) * SIGMA_PS, k);
    }
  }

  // Dark counts (pair ID 0)
  int nd = poisson(privRng, (float)simDark * WINDOW_NS * 1e-9f, 6);
  for (int k = 0; k < nd; k++) addEv(u01(privRng) * WINDOW_PS, 0);

  // Sort by time (insertion sort)
  for (uint8_t i = 1; i < nEv; i++) {
    Ev x = ev[i]; int8_t j = i - 1;
    while (j >= 0 && ev[j].t > x.t) { ev[j + 1] = ev[j]; j--; }
    ev[j + 1] = x;
  }

  // Detector dead time, window limits, TDC minimum gap
  uint8_t n = 0;
  long lastDet = -1000000000L, lastStop = -1000000000L;
  for (uint8_t i = 0; i < nEv && n < numStop; i++) {
    if (ev[i].t - lastDet < DEAD_PS) continue;          // detector dead
    lastDet = ev[i].t;
    if (ev[i].t < MIN_T_PS || ev[i].t >= WINDOW_PS) continue;   // outside TDC mode 1 range
    if (ev[i].t - lastStop < MIN_GAP_PS) continue;      // TDC can't separate
    lastStop = ev[i].t;
    stopT[n] = ev[i].t; stopId[n] = ev[i].id; n++;
  }
  return n;
}