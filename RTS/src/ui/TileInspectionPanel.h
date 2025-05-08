#pragma once
#include "tile/TileHandle.h"

class World;

class TileInspectionPanel
{
public:
    TileInspectionPanel(World& world, const f32v2& screenPos, const TileHandle& tileHandle);
    void updateAndRender();

private:
    World& mWorld;
    f32v2 mScreenPos;
    TileHandle mTileHandle;
    bool mDidInit = false;
};

