#include "stdafx.h"
#include "Structure.h"

ui32v3 Structure::getWorldPositionOfTile(TileIndex tile) const {
    return f32v3(mTileContainer.getTileXYZOffset(tile) + mTileContainer.getWorldPos3D());
}
