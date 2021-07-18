#include "stdafx.h"
#include "Random.h"

static ui32 x = 123456789, y = 362436069, z = 521288629;
bool hasInitCachedRandom = false;
std::vector<ui32> cachedRandom;
static unsigned cachedRandomIndex = 0;

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

extern ui32 Random::getCachedRandom() {
    assert(hasInitCachedRandom);
    return cachedRandom[++cachedRandomIndex % (unsigned)cachedRandom.size()];
}

extern float Random::getCachedRandomf() {
    assert(hasInitCachedRandom);
    return (cachedRandom[++cachedRandomIndex % (unsigned)cachedRandom.size()] & 0x01fffffff) / (float)0x01fffffff;
}

extern ui32 Random::getThreadSafe(ui32 x, ui32 y) {
    ui32 a = x * 2366207 + y * 2745229 - 23747;
    a = (a ^ 61) ^ (a >> 16);
    a = a + (a << 3);
    a = a ^ (a >> 4);
    a = a * 0x27d4eb2d;
    a = a ^ (a >> 15);
    return a;
};

extern float Random::getThreadSafef(ui32 x, ui32 y) {
    // random large prime
    return (getThreadSafe(x, y) % 6421343) / 6421343.0f;
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
