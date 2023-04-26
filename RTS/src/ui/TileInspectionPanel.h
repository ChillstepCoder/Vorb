#pragma once
#include "tile/TileHandle.h"

class IWorld;

class TileInspectionPanel
{
public:
    TileInspectionPanel(IWorld& world, const f32v2& screenPos, const TileHandle& tileHandle);
    void updateAndRender();

private:
    IWorld& mWorld;
    f32v2 mScreenPos;
    TileHandle mTileHandle;
    bool mDidInit = false;
};

