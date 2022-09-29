#pragma once

#include "network/WorldType.h"

class IWorld;

class WorldFactory
{
public:
    static IWorld& makeWorld(WorldType type);
private:
    static IWorld& makeClientWorld();
    static IWorld& makeHostWorld();
    static IWorld& makeServerWorld();
};
