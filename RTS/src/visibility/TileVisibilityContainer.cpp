#include "stdafx.h"
#include "TileVisibilityContainer.h"

#include "tile/Tile.h"
#include "tile/TileWallContainer.h"

#include "resources/TileRepository.h"

void TileVisibilityContainer::init(const TileSpatialGrid* tileSpatialGrid, const std::vector<Tile>& tiles, const TileWallContainer& tileWalls)
{
    mTileSpatialGrid = tileSpatialGrid;
    resize(mTileSpatialGrid);

    // TODO: Care about BitArray OwnedTiles?
    // Init visibility tile
    mOccludedEdges.zeroAllBits();

    // Build tile visibility data
    mTileOcclusion.resizeAndZero(tileSpatialGrid->getNumTiles());
    mTileVisibilityBlockerCounts.resize(tileSpatialGrid->getNumTiles(), 0);
    for (TileIndex i = 0; i < tileSpatialGrid->getNumTiles(); ++i) {
        const TileID id = tiles[i].getMainID();
        if (isTileNone(id)) {
            continue;
        }

        const TileDef& def = TileRepository::get().getLoadedOrUnloadedAsset(id);

        if (def.blocksVisibility) {
            mTileOcclusion.setBit(i);
            ++mTileVisibilityBlockerCounts[i];
            occludeAllEdgesForTile(i);
        }
    }

    // Add wall occluders (There are no east and north walls at the edge of a tile container)
    for (TileIndex i = 0; i < tileSpatialGrid->getNumTiles(); ++i) {
        if (tileWalls.getSouthWallAtTile(i).isValid()) {
            mOccludedEdges.setBit(getEdgeIndex(i, Cartesian::SOUTH));
        }
        if (tileWalls.getWestWallAtTile(i).isValid()) {
            mOccludedEdges.setBit(getEdgeIndex(i, Cartesian::WEST));
        }
        if (tileWalls.getEastWallAtTile(i).isValid()) {
            mOccludedEdges.setBit(getEdgeIndex(i, Cartesian::EAST));
        }
        if (tileWalls.getNorthWallAtTile(i).isValid()) {
            mOccludedEdges.setBit(getEdgeIndex(i, Cartesian::NORTH));
        }
    }
}

void TileVisibilityContainer::resize(const TileSpatialGrid* tileSpatialGrid) {
    assert(mTileSpatialGrid);
    assert(mTileSpatialGrid->getNumTiles());
    const i32v3& dims = mTileSpatialGrid->getDims();
    mFloorStride = dims.x * dims.y * 4;
    mOccludedEdges.resize(mFloorStride * dims.z);
}

void TileVisibilityContainer::copyFrom(const TileVisibilityContainer& other) {
    assert(mOccludedEdges.getNumBits() == other.mOccludedEdges.getNumBits());
    assert(mEdgesDims == other.mEdgesDims);
    mTileSpatialGrid = other.mTileSpatialGrid;
    memcpy(mOccludedEdges.data(), other.mOccludedEdges.data(), mOccludedEdges.getNumBytes());
}

void TileVisibilityContainer::destroy() {
    mOccludedEdges.freeData();
    mTileOcclusion.freeData();
    std::vector<ui8>().swap(mTileVisibilityBlockerCounts);
    mTileSpatialGrid = nullptr;
}


void TileVisibilityContainer::refreshTileVisibility(TileIndex tileIndex, const Tile& prevTile, const Tile& newTile) {

}


void TileVisibilityContainer::refreshTileVisibility(TileIndex tileIndex, const TileWalls& prevTileWalls, const TileWalls& newTileWalls) {

}

void TileVisibilityContainer::refreshTileVisibility(TileIndex tileIndex, TileWall prevWall, TileWall newWall, Cartesian dir) {
    VisEdgeIndex edgeIndex = getEdgeIndex(getEdgeIndexBase(tileIndex), dir);
    const bool isBitSet = mOccludedEdges.getBit(edgeIndex);
    bool changed = false;
    if (isBitSet) {
        if (!newWall.isValid()) {
            // We are potentially clearing the direction
            if (!mTileOcclusion.getBit(tileIndex)) {
                mOccludedEdges.clearBit(edgeIndex);
                changed = true;
            }
        }
    }
    else if (newWall.isValid()) {
        // We are occluding the direction
        mOccludedEdges.setBit(edgeIndex);
        changed = true;
    }
    if (changed) {
        assert(false);
    }
}


bool TileVisibilityContainer::getOccludedEdge(TileIndex tileIndex, Cartesian dir) const {
    VisEdgeIndex edgeIndex = getEdgeIndexBase(tileIndex);
    return mOccludedEdges.getBit(edgeIndex);
}

void TileVisibilityContainer::debugRender() const
{

}

void TileVisibilityContainer::occludeAllEdgesForTile(TileIndex tileIndex) {
    const i32v3 xyzOffset = mTileSpatialGrid->getTileXYZOffset(tileIndex);
    const i32 index = xyzOffset.x * mEdgesDims.x + xyzOffset.y * mEdgesDims.y + mFloorStride * xyzOffset.z;
    mOccludedEdges.setBit(index);
    mOccludedEdges.setBit(index + 1);
    mOccludedEdges.setBit(index + 2);
    mOccludedEdges.setBit(index + mEdgesDims.x);
}
