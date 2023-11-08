#include "stdafx.h"
#include "TileVisibilityContainer.h"

#include "tile/Tile.h"
#include "tile/TileWallContainer.h"

void TileVisibilityContainer::init(const TileSpatialGrid* tileSpatialGrid, const std::vector<Tile>& tiles, const TileWallContainer& tileWalls)
{
    mTileSpatialGrid = tileSpatialGrid;
    resize(mTileSpatialGrid);
    mVisibilityEdges.zeroAllBits();

    // TODO: Care about BitArray OwnedTiles?
    // Init visibility tile
    mTileVisibility.resizeAndZero(tileSpatialGrid->getNumTiles() * 4);
    assert(false);
}

void TileVisibilityContainer::resize(const TileSpatialGrid* tileSpatialGrid)
{
    assert(mTileSpatialGrid);
    assert(mTileSpatialGrid->getNumTiles());
    const i32v3& dims = mTileSpatialGrid->getDims();
    mEdgesDims.x = dims.x + 1;
    mEdgesDims.y = dims.y + 1;
    mEdgesDims.z = dims.z;
    mFloorStride = mEdgesDims.x * mEdgesDims.y;
    mVisibilityEdges.resize(mEdgesDims.x * mEdgesDims.y * mEdgesDims.z);
}

void TileVisibilityContainer::copyFrom(const TileVisibilityContainer& other)
{
    assert(mVisibilityEdges.getNumBits() == other.mVisibilityEdges.getNumBits());
    assert(mEdgesDims == other.mEdgesDims);
    mTileSpatialGrid = other.mTileSpatialGrid;
    memcpy(mVisibilityEdges.data(), other.mVisibilityEdges.data(), mVisibilityEdges.getNumBytes());
}

void TileVisibilityContainer::destroy()
{
    mVisibilityEdges.freeData();
    mTileSpatialGrid = nullptr;
}

bool TileVisibilityContainer::isTileVisibleFromDirection(TileIndex tileIndex, Cartesian dir)
{
    const i32v3 xyzOffset = mTileSpatialGrid->getTileXYZOffset(tileIndex);
    i32 index = xyzOffset.x * mEdgesDims.x + xyzOffset.y * mEdgesDims.y + mFloorStride * xyzOffset.z;
    switch (dir) {
        case Cartesian::SOUTH: {
            break;
        }
        case Cartesian::WEST: {
            index += 1;
            break;
        }
        case Cartesian::EAST: {
            index += 2;
            break;
        }
        case Cartesian::NORTH: {
            index += mEdgesDims.x;
            break;
        }
    }
    return mVisibilityEdges.getBit(index);
}
