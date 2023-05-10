#pragma once

#include "generation/WorldGeneratorType.h"

class IWorldGenerator;
class IWorld;

class WorldGeneratorFactory
{
public:
    static std::unique_ptr<IWorldGenerator> makeWorldGenerator(WorldGeneratorType type, IWorld& world);
};

