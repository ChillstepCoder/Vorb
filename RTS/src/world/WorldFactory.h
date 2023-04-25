#pragma once

#include "network/WorldNetMode.h"

class IWorld;

class WorldFactory
{
public:
    static IWorld& makeWorld(WorldNetMode type);
    static void destroyWorld();
private:
    static IWorld& makeClientWorld();
    static IWorld& makeHostWorld();
    static IWorld& makeServerWorld();
};
