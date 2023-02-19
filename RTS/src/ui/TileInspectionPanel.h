#pragma once
#include "tile/TileHandle.h"

class TileInspectionPanel
{
public:
    TileInspectionPanel(const f32v2& screenPos, const TileHandle& tileHandle);
    void updateAndRender();

private:
    f32v2 mScreenPos;
    TileHandle mTileHandle;
    bool mDidInit = false;
};

