#include "stdafx.h"
#include "Random.h"

static unsigned long x = 123456789, y = 362436069, z = 521288629;

unsigned long Random::xorshf96() {          //period 2^96-1
    unsigned long t;
    x ^= x << 16;
    x ^= x >> 5;
    x ^= x << 1;

    t = x;
    x = y;
    y = z;
    z = t ^ x ^ y;

    return z;
}

extern float Random::xorshf96f() {
    return (xorshf96() & 0x01fffffff) / (float)0x01fffffff;
}
