#include <iostream> 
#include <math.h>
#include "sim.h"

static uint32_t x = 8, y = 1, z = 13, w= 1;
 
uint32_t xorshift128(void) {
    uint32_t t = x ^ (x << 11);
    x = y; y = z; z = w;
    return w = w ^ (w >> 19) ^ t ^ (t >> 8);
}

void set_seed(uint32_t s) {
    x = 8; z = 13;
    y = w = s;
    for (int i = 0; i < 20; ++i) xorshift128();
}


void simInit(char arm) {
    if (arm == 'A') {
        set_seed(seq * 184833 + 4542);
    }
    else if (arm == 'B') {
        set_seed(seq * 318754 + 503);
    }
}

float uniform_dist() { /*so that we can take random numbers between 0,1*/
    return ((xorshift128() >> 8) + 1) * (1.0f / 16777216.0f);
}

float generateGaussianNoise(float mu, float sigma) /*via box-muller, for gaussian jitter*/
{
    constexpr float two_pi = 2.0 * M_PI;
    float u1, u2;
    do
    {
        u1 = uniform_dist();
    }
    while (u1 == 0);
    u2 = uniform_dist();

    auto mag = sigma * sqrt(-2.0 * log(u1));
    auto z0  = mag * cos(two_pi * u2) + mu;
    auto z1  = mag * sin(two_pi * u2) + mu;
    return z0;
}

int knuth_poisson(float lambda) {
    int k = -1;
    double logp = 0;
    do {
        k++;
        logp += log(uniform_dist());
    } while( logp > -lambda);
    return k;
}

void sort_pair(float t[], uint8_t id[], int n) {
    for (int i = 1; i < n; ++i) {
        float kt = t[i];
        int kid = id[i];
        int j = i - 1;
        while (j >= 0 && t[j] > kt) {
            t[j + 1] = t[j];
            id[j + 1] = id[j];
            --j;
        }
        t[j + 1] = kt;
        id[j + 1] = kid;
    }
}

uint8_t simulate(uint32_t seq, uint8_t numStop, long stopT[], uint8_t stopId[]) {
    const uint32_t seed = seq * 9354 + 45 ;
    set_seed(seed);

    float events[numStop] = {};
    uint8_t pair_id[numStop] = {};
    int current_pair = 1;

    int numevents = knuth_poisson(simRate * 5e-7);
    if (numevents > 0) {
        if (numevents > numStop) {
            numevents = numStop;
        }
        for (int i = 0; i < numevents; ++i) {
            pair_id[i] = current_pair;
            current_pair += 1;
        }
        for (int j = 0; j < sizeof(events)/sizeof(events[0]); ++j) {
            if (j < numevents){
                events[j] = uniform_dist() * 500;
            }
            else if (j >= numevents){
                events[j] = 1000;
            }
        }
    }
    sort_pair(events,pair_id,numStop);
    
    simInit(arm, seq);
    for (int i = 0; i < numStop; ++ i) {
        float prob = uniform_dist();
        if (arm == 'B') {
            events[i] += simDelayNs/1000;
        }
        if (prob >= .12 and not events[i] < 500) {
            events[i] = 1000;
            numevents -= 1;
        }

        if (events[i] < 500) {
            events[i] += generateGaussianNoise(0,.425);
        }
    }
    if (numevents < numStop){
        int dark_counts = knuth_poisson(simDark * 5e-7);
        if (numevents + dark_counts > numStop){
            dark_counts = numStop - numevents;
        }
    sort_pair(events,pair_id,numStop);
    int i = 0;
    while (dark_counts > 0 && i < numStop) {
        if (events[i] >= 500) {            // empty/lost slot
            events[i] = uniform_dist() * 500;
            pair_id[i] = 0;
            --dark_counts;
    }
    ++i;
}
    
    }

    sort_pair(events,pair_id,numStop);
    
    float last_valid_time = events[0];
    for (int i = 0; i < numStop-1; ++i){
        if (events[i] < 12){
            events[i] = 1000;
        }
        if (fabs(events[i+1] - last_valid_time) <= 45){
            events[i+1] = 1000;
            last_valid_time = events[i];
        }
    }
    uint8_t count = 0;
    for (int i = 0; i < numStop; ++i) {
        if (events[i] < 500) {
            stopT[count] = (long)events[i];
            stopId[count] = pair_id[i];
            ++count;
        }
    }

    return count;
}