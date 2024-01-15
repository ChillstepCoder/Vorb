#pragma once

#include "network/WorldNetMode.h"

class World;

class WorldContextObject {
public:
    WorldContextObject(World& world) : mWorld(world) {}
    virtual ~WorldContextObject() = default;

    World& getWorld() const { return mWorld; }
    WorldNetMode getNetMode() const;
    bool isEditor() const;
    bool isHost() const;
protected:
    World& mWorld;
};

// How net works
// HostObject : IObject {
//    CliObject;
// }

// World {
//  IObject obj1;
//  IObject obj2;
// }
