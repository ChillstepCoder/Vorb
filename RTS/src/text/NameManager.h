#pragma once

class RandomGenerator;

class NameManager {
public:
    static const char* getRandomFirstName(RandomGenerator& gen, bool isFemale);
    static const char* getRandomLastName(RandomGenerator& gen);
};

