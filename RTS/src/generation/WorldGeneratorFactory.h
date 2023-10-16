#pragma once

#include "generation/WorldGeneratorType.h"

class IWorldGenerator;
class World;

class WorldGeneratorFactory
{
public:
    static std::unique_ptr<IWorldGenerator> makeWorldGenerator(WorldGeneratorType type, World& world);
};

