#pragma once

#include "math/fastPRNG.h"

// Fast
namespace Random {
    extern ui32 xorshf96();
    // Returns number between 0 and 1
    extern float xorshf96f();

    // Low quality but extremely fast RNG
    extern void initCachedRandom(unsigned count);
    extern ui32 getCachedRandom();
    extern ui32 getCachedRandomSpecific(ui32 i);
    extern float getCachedRandomf();
    extern float getCachedRandomfSpecific(ui32 i);

    extern ui32 getThreadSafe(ui32 x, ui32 y);
    extern float getThreadSafef(ui32 x, ui32 y);
    extern float getThreadSafef(ui64 x);

    class RandomPermutationTable {
    public:
        RandomPermutationTable(unsigned count);

        const std::vector<unsigned>& getPerm() const { return mPerm; }
        unsigned getAt(unsigned index) const { return mPerm[index]; }
    private:
        std::vector<unsigned> mPerm;
    };

}

class RandomGenerator {
public:
    static constexpr ui32 DEFAULT_SEED = 0xB25D9A7B; // Idk just random bits
    RandomGenerator() : mGen(DEFAULT_SEED), mSeed(DEFAULT_SEED) {}
    RandomGenerator(ui32 seed) : mGen(seed), mSeed(seed) {}

    void reset() {
        mGen = fastPRNG::fastXS32(mSeed);
    }

    // [0, 1]
    f32 getRandomFloatUnsigned() {
        return mGen.xoroshiro64x_UNI<f32>();
    }
    // [-1, 1]
    f32 getRandomFloatSigned() {
        return mGen.xoroshiro64x_VNI<f32>();
    }
    f32 getRandomFloatInRange(f32 min, f32 max) {
        return mGen.xoroshiro64x_Range<f32>(min, max);
    }
    // [0, UINT32_MAX]
    ui32 getRandomUint() {
        return mGen.xoroshiro64x();
    }
    bool getRandomBool() {
        return (bool)(mGen.xoroshiro64x() % 2);
    }

    ui32 getRandomUIntInRange(ui32 min, ui32 max) {
        return min + (getRandomUint() % (max - min));
    }

    fastPRNG::fastXS32 mGen;
    ui32 mSeed;
};

inline f32 randFromf32v3(const f32v3& x, ui64 additional) {
    return Random::getThreadSafef((ui64)f32v3hash()(x) + additional);
}