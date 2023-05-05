#pragma once

#include "network/WorldNetMode.h"

class IWorld;
class CliWorld;
class HostWorld;
class DedicatedSrvWorld;

class WorldFactory
{
public:
    static std::unique_ptr<IWorld> makeWorld(WorldNetMode type, ui32 worldWidthTiles);
    static void destroyWorld();
private:
    static std::unique_ptr<CliWorld> makeClientWorld(ui32 worldWidthTiles);
    static std::unique_ptr<CliWorld> makeEditorWorld(ui32 worldWidthTiles);
    static std::unique_ptr<HostWorld> makeHostWorld(ui32 worldWidthTiles);
    static std::unique_ptr<DedicatedSrvWorld> makeServerWorld(ui32 worldWidthTiles);
};
