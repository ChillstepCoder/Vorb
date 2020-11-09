#include "stdafx.h"
#include "Random.h"

static ui32 x = 123456789, y = 362436069, z = 521288629;
bool hasInitCachedRandom = false;
std::vector<ui32> cachedRandom;

ui32 Random::xorshf96() {          //period 2^96-1
    ui32 t;
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

extern void Random::initCachedRandom(unsigned count) {
    assert(!hasInitCachedRandom);
    hasInitCachedRandom = true;
    cachedRandom.resize(count);
    for (auto&& cached : cachedRandom) {
        cached = xorshf96();
    }
}

extern ui32 Random::getCachedRandom(unsigned index) {
    return cachedRandom[index % (unsigned)cachedRandom.size()];
}

extern ui32 Random::getCachedRandomf(unsigned index) {
    return (cachedRandom[index % (unsigned)cachedRandom.size()] & 0x01fffffff) / (float)0x01fffffff;
}

Random::RandomPermutationTable::RandomPermutationTable(unsigned count) :
    mPerm(count)
{
    // Initialize and shuffle the permutation table
    const ui32 size = (ui32)mPerm.size();
    for (ui32 i = 0; i < size; ++i) {
        mPerm[i] = i;
    }
    // Fisher–Yates
    for (ui32 i = size - 1; i != 0; --i) {
        int j = xorshf96() % i;
        std::swap(mPerm[i], mPerm[j]);
    }
}
