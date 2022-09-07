#pragma once

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