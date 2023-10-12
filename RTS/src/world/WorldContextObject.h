#pragma once

#include "network/WorldNetMode.h"

class IWorld;

class WorldContextObject {
public:
    WorldContextObject(IWorld& world) : mWorld(world) {}
    virtual ~WorldContextObject() = default;

    WorldNetMode getNetMode() const;
    bool isEditor() const;
protected:
    IWorld& mWorld;
};

// How net works
// HostObject : IObject {
//    CliObject;
// }

// World {
//  IObject obj1;
//  IObject obj2;
// }
