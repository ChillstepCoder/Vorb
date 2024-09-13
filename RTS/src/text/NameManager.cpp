#include "stdafx.h"
#include "NameManager.h"

#include "NameStrings.inl"

WorldNameContext::WorldNameContext() {
    mAvailableLargeIslandNames.resize(std::size(LARGE_ISLAND_NAMES));
    for (size_t i = 0; i < std::size(LARGE_ISLAND_NAMES); ++i) {
        mAvailableLargeIslandNames[i] = i;
    };
    mAvailableSmallIslandNames.resize(std::size(SMALL_ISLAND_NAMES));
    for (size_t i = 0; i < std::size(SMALL_ISLAND_NAMES); ++i) {
        mAvailableSmallIslandNames[i] = i;
    };
    mAvailableLakeNames.resize(std::size(LAKE_NAMES));
    for (size_t i = 0; i < std::size(LAKE_NAMES); ++i) {
        mAvailableLakeNames[i] = i;
    };
    mAvailableOceanNames.resize(std::size(OCEAN_NAMES));
    for (size_t i = 0; i < std::size(OCEAN_NAMES); ++i) {
        mAvailableOceanNames[i] = i;
    };

}

const char* WorldNameContext::getRandomUniqueSmallIslandName(RandomGenerator& gen) {
    if (mAvailableSmallIslandNames.empty()) {
        mAvailableSmallIslandNames.resize(std::size(SMALL_ISLAND_NAMES));
        for (size_t i = 0; i < std::size(SMALL_ISLAND_NAMES); ++i) {
            mAvailableSmallIslandNames[i] = i;
        };
    }
    const ui32 index = gen.getRandomUIntInRange(0, mAvailableSmallIslandNames.size());
    mAvailableSmallIslandNames[index] = mAvailableSmallIslandNames.back();
    mAvailableSmallIslandNames.pop_back();
    return SMALL_ISLAND_NAMES[index];
}

const char* WorldNameContext::getRandomUniqueLargeIslandName(RandomGenerator& gen) {
    if (mAvailableLargeIslandNames.empty()) {
        mAvailableLargeIslandNames.resize(std::size(LARGE_ISLAND_NAMES));
        for (size_t i = 0; i < std::size(LARGE_ISLAND_NAMES); ++i) {
            mAvailableLargeIslandNames[i] = i;
        };
    }
    const ui32 index = gen.getRandomUIntInRange(0, mAvailableLargeIslandNames.size());
    mAvailableLargeIslandNames[index] = mAvailableLargeIslandNames.back();
    mAvailableLargeIslandNames.pop_back();
    return LARGE_ISLAND_NAMES[index];
}

const char* WorldNameContext::getRandomUniqueLakeName(RandomGenerator& gen) {
    if (mAvailableLakeNames.empty()) {
        mAvailableLakeNames.resize(std::size(LAKE_NAMES));
        for (size_t i = 0; i < std::size(LAKE_NAMES); ++i) {
            mAvailableLakeNames[i] = i;
        };
    }
    const ui32 index = gen.getRandomUIntInRange(0, mAvailableLakeNames.size());
    mAvailableLakeNames[index] = mAvailableLakeNames.back();
    mAvailableLakeNames.pop_back();
    return LAKE_NAMES[index];
}

const char* WorldNameContext::getRandomUniqueOceanName(RandomGenerator& gen) {
    if (mAvailableOceanNames.empty()) {
        mAvailableOceanNames.resize(std::size(OCEAN_NAMES));
        for (size_t i = 0; i < std::size(OCEAN_NAMES); ++i) {
            mAvailableOceanNames[i] = i;
        };
    }
    const ui32 index = gen.getRandomUIntInRange(0, mAvailableOceanNames.size());
    mAvailableOceanNames[index] = mAvailableOceanNames.back();
    mAvailableOceanNames.pop_back();
    return OCEAN_NAMES[index];
}

const char* NameManager::getRandomFirstName(RandomGenerator& gen, bool isFemale) {
    if (isFemale) {
        return FIRST_NAMES_FEMALE[gen.getRandomUIntInRange(0, std::size(FIRST_NAMES_FEMALE))];
    } else {
        return FIRST_NAMES_MALE[gen.getRandomUIntInRange(0, std::size(FIRST_NAMES_MALE))];
    }
}

const char* NameManager::getRandomLastName(RandomGenerator& gen) {
    return FIRST_NAMES_MALE[gen.getRandomUIntInRange(0, std::size(LAST_NAMES))];
}

SettlementNameContext::SettlementNameContext() {
    mAvailableSettlementNames.resize(std::size(SETTLEMENT_NAMES));
    for (size_t i = 0; i < std::size(SETTLEMENT_NAMES); ++i) {
        mAvailableSettlementNames[i] = i;
    };
}

const char* SettlementNameContext::getRandomUniqueSettlementName(RandomGenerator& gen) {
    if (mAvailableSettlementNames.empty()) {
        mAvailableSettlementNames.resize(std::size(SETTLEMENT_NAMES));
        for (size_t i = 0; i < std::size(SETTLEMENT_NAMES); ++i) {
            mAvailableSettlementNames[i] = i;
        };
    }
    const ui32 index = gen.getRandomUIntInRange(0, mAvailableSettlementNames.size());
    mAvailableSettlementNames[index] = mAvailableSettlementNames.back();
    mAvailableSettlementNames.pop_back();
    return SETTLEMENT_NAMES[index];
}
