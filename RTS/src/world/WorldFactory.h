#pragma once

#include "network/WorldNetMode.h"

class IWorld;
class CliWorld;
class HostWorld;
class DedicatedSrvWorld;

class WorldFactory
{
public:
    static std::unique_ptr<IWorld> makeWorld(WorldNetMode type);
    static void destroyWorld();
private:
    static std::unique_ptr<CliWorld> makeClientWorld();
    static std::unique_ptr<HostWorld> makeHostWorld();
    static std::unique_ptr<DedicatedSrvWorld> makeServerWorld();
};
