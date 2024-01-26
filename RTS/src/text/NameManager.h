#pragma once

class RandomGenerator;

class WorldNameContext {
public:
    WorldNameContext();

    const char* getRandomUniqueSmallIslandName(RandomGenerator& gen);
    const char* getRandomUniqueLargeIslandName(RandomGenerator& gen);
    const char* getRandomUniqueLakeName(RandomGenerator& gen);
    const char* getRandomUniqueOceanName(RandomGenerator& gen);

private:
    std::vector<ui32> mAvailableLargeIslandNames;
    std::vector<ui32> mAvailableSmallIslandNames;
    std::vector<ui32> mAvailableLakeNames;
    std::vector<ui32> mAvailableOceanNames;
};

class NameManager {
public:
    static const char* getRandomFirstName(RandomGenerator& gen, bool isFemale);
    static const char* getRandomLastName(RandomGenerator& gen);
};

