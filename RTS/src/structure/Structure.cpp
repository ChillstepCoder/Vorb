#include "stdafx.h"
#include "Structure.h"

i32v3 Structure::getWorldPositionOfTile(TileIndex tile) const {
    return i32v3(i32v3(mTileContainer.getTileXYZOffset(tile)) + mTileContainer.getWorldPos3D());
}
