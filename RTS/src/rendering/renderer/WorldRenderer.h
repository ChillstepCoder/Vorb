#pragma once

#include "world/WorldType.h"

class IWorld;

class WorldRenderer
{
public:
    
    void registerWorld(IWorld* world);
    void renderActiveWorld();

private:

    WorldType mActiveWorld = WorldType::Game;
    const IWorld* mWorlds[e_cast(WorldType::COUNT)] = {};
};

