#pragma once

// Fast
namespace Random {
    extern ui32 xorshf96();
    // Returns number between 0 and 1
    extern float xorshf96f();

    // Low quality but extremely fast RNG
    extern void initCachedRandom(unsigned count);
    extern ui32 getCachedRandom();
    extern float getCachedRandomf();

    class RandomPermutationTable {
    public:
        RandomPermutationTable(unsigned count);

        const std::vector<unsigned>& getPerm() const { return mPerm; }
        unsigned getAt(unsigned index) const { return mPerm[index]; }
    private:
        std::vector<unsigned> mPerm;
    };
}