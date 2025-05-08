#pragma once


// 3D container which can be indexed by a TileIndex
class TileSpatialGrid {
public:
    TileSpatialGrid() {};

    void init(const i32v3& rootPos, const i32v3& tileDims, i32 floorHeight) {
        mAABB.pos = rootPos;
        mAABB.dims = i32v3(tileDims.x, tileDims.y, tileDims.z * floorHeight);
        mTileDims = tileDims;
        mFloorHeight = floorHeight;
        mNumTiles = tileDims.x * tileDims.y * tileDims.z;
        mAABB.pos = rootPos;
        mFloorStride = tileDims.x * tileDims.y;
    }
    inline bool isPosAtSouthBorder(const i32v3& tilePos) const { return tilePos.y <= 0; }
    inline bool isPosAtSouthBorder(const i32v2& tilePos) const { return tilePos.y <= 0; }
    inline bool isPosAtWestBorder(const i32v3& tilePos) const { return tilePos.x <= 0; }
    inline bool isPosAtWestBorder(const i32v2& tilePos) const { return tilePos.x <= 0; }
    inline bool isPosAtEastBorder(const i32v3& tilePos) const { return tilePos.x >= mTileDims.x - 1; }
    inline bool isPosAtEastBorder(const i32v2& tilePos) const { return tilePos.x >= mTileDims.x - 1; }
    inline bool isPosAtNorthBorder(const i32v3& tilePos) const { return tilePos.y >= mTileDims.y - 1; }
    inline bool isPosAtNorthBorder(const i32v2& tilePos) const { return tilePos.y >= mTileDims.y - 1; }
    inline bool isXAtWestBorder(i32 x) const { return x <= 0; }
    inline bool isXAtEastBorder(i32 x) const { return x >= mTileDims.x - 1; }
    inline bool isYAtSouthBorder(i32 y) const { return y <= 0; }
    inline bool isYAtNorthBorder(i32 y) const { return y >= mTileDims.y - 1; }
    inline TileIndex getSouthWestTileIndex(TileIndex tileIndex) const { return tileIndex - mTileDims.x - 1; }
    inline TileIndex getSouthTileIndex(TileIndex tileIndex) const { return tileIndex - mTileDims.x; }
    inline TileIndex getSouthEastTileIndex(TileIndex tileIndex) const { return tileIndex - mTileDims.x + 1; }
    inline TileIndex getWestTileIndex(TileIndex tileIndex) const {  return tileIndex - 1; }
    inline TileIndex getEastTileIndex(TileIndex tileIndex) const { return tileIndex + 1; }
    inline TileIndex getNorthWestTileIndex(TileIndex tileIndex) const { return tileIndex + mTileDims.x - 1; }
    inline TileIndex getNorthTileIndex(TileIndex tileIndex) const { return tileIndex + mTileDims.x; }
    inline TileIndex getNorthEastIndex(TileIndex tileIndex) const { return tileIndex + mTileDims.x + 1; }

    i32v3 getTileXYZOffsetWithZScale(TileIndex i) const { return i32v3(i % mTileDims.x, (i % mFloorStride) / mTileDims.x, (i / mFloorStride) * mFloorHeight); }
    static i32v3 getTileXYZOffsetWithZScale(TileIndex i, const i32v3& dims, i32 floorHeight) {
        const i32 layerSize = dims.x * dims.y;
        return i32v3(i % dims.x, (i % layerSize) / dims.x, (i / layerSize) * floorHeight);
    }
    i32v3 getTileXYZOffset(TileIndex i) const { return i32v3(i % mTileDims.x, (i % mFloorStride) / mTileDims.x, i / mFloorStride); }
    static i32v3 getTileXYZOffset(TileIndex i, const i32v3& dims) {
        const i32 layerSize = dims.x * dims.y;
        return i32v3(i % dims.x, (i % layerSize) / dims.x, i / layerSize);
    }
    i32v2 getTileXYOffset(TileIndex i) const { return i32v2(i % mTileDims.x, (i % mFloorStride) / mTileDims.x); }
    static i32v2 getTileXYOffset(TileIndex i, const i32v2& dims) {
        const i32 layerSize = dims.x * dims.y;
        return i32v2(i % dims.x, (i % layerSize) / dims.x);
    }
    i32v2 getTileBaseWorldPos2D(TileIndex i) const {
        const i32v2 worldRoot(mAABB.pos.x, mAABB.pos.y);
        return i32v2(worldRoot.x + (i % mTileDims.x), worldRoot.y + (i / mTileDims.x));
    }
    i32v3 getTileBaseWorldPos3D(TileIndex i) const {
        const i32v3 worldRoot(mAABB.pos.x, mAABB.pos.y, mAABB.pos.z);
        return i32v3(worldRoot.x + (i % mTileDims.x), worldRoot.y + ((i % mFloorStride) / mTileDims.x), worldRoot.z + (i / mFloorStride) * mFloorHeight);
    }
    f32v3 getTileCenterWorldPos3D(TileIndex i, f32 tileGroundZOffset) const {
        return f32v3(mAABB.pos.x + (i % mTileDims.x) + 0.5f, mAABB.pos.y + ((i % mFloorStride) / mTileDims.x) + 0.5f, mAABB.pos.z + (i / mFloorStride) * mFloorHeight + tileGroundZOffset);
    }
    f32v3 getTileWorldPos3D(TileIndex i, f32 tileGroundZOffset) const {
        return f32v3(mAABB.pos.x + (i % mTileDims.x), mAABB.pos.y + ((i % mFloorStride) / mTileDims.x), mAABB.pos.z + (i / mFloorStride) * mFloorHeight + tileGroundZOffset);
    }
    static TileIndex getTileIndexFromXYZOffset(const ui32v3& xyz, const ui32v3& dims) { return xyz.x + xyz.y * dims.x + xyz.z * dims.x * dims.y; }
    static TileIndex getTileIndexFromXYZOffset(const i32v3& xyz, const i32v3& dims) { return xyz.x + xyz.y * dims.x + xyz.z * dims.x * dims.y; }
    static TileIndex getBaseTileIndexFromXYOffset(const i32v2& xy, const i32v3& dims) { return xy.x + xy.y * dims.x; }
    TileIndex getTileIndexFromXYZOffset(const ui32v3& xyz) const { return xyz.x + xyz.y * mTileDims.x + xyz.z * mFloorStride; }
    TileIndex getTileIndexFromXYZOffset(const i32v3& xyz) const { return (TileIndex)(xyz.x + xyz.y * mTileDims.x + xyz.z * mFloorStride); }
    TileIndex getTileIndexFromXYZOffset(i32 x, i32 y, i32 z) const { return (TileIndex)(x + y * mTileDims.x + z * mFloorStride); }
    TileIndex getBaseTileIndexFromXYOffset(i32 x, i32 y) const { return (TileIndex)(x + y * mTileDims.x); }
    // Returns INVALID_INDEX if out of bounds
    TileIndex tryGetBaseTileIndexFromXYOffsetf(const f32v2& offset) const {
        if (offset.x <= 0.0f || offset.y <= 0.0f || (int)offset.x > mTileDims.x - 1 || (int)offset.y > mTileDims.y - 1) {
            return INVALID_TILE_INDEX;
        }
        return getBaseTileIndexFromXYOffset((i32)offset.x, (i32)offset.y);
    }

    // Returns dims in tiles, not accounting floor height
    i32v3 getDims() const { return mTileDims; }
    // True world size AABB including floor height
    const i32AABB3 getAABB() const { return mAABB; }
    i32 getFloorStride() const { return mFloorStride; }
    i32 getNumTiles() const { return mNumTiles; }
    i32 getFloorHeight() const { return mFloorHeight; }
    
    i32v3 getWorldPos() const { return mAABB.pos; }
    f32v3 getWorldPosCenter() const { return f32v3(mAABB.pos) + f32v3(mAABB.dims) * 0.5f; }

private:

    i32AABB3 mAABB;
    i32v3 mTileDims;
    i32 mNumTiles = 0;
    i32 mFloorHeight = 0;
    i32 mFloorStride = 0;
};

// TODO Refactor this somewhere

// FOR HELPERS
#include "util/BitArray.h"
namespace TileSpatialGridHelpers {
    inline int countAdjacentSetOwnershipBits(const TileSpatialGrid& spatialGrid, const i32v3& bitOffsetXYZ, const BitArray& ownershipBits) {
        int numAdjacent = 0;
        TileIndex tileIndex = spatialGrid.getTileIndexFromXYZOffset(bitOffsetXYZ.x, bitOffsetXYZ.y, bitOffsetXYZ.z);
        if (!spatialGrid.isPosAtSouthBorder(bitOffsetXYZ)) {
            if (ownershipBits.getBit(spatialGrid.getSouthTileIndex(tileIndex))) {
                ++numAdjacent;
            }
        }
        if (!spatialGrid.isPosAtWestBorder(bitOffsetXYZ)) {
            if (ownershipBits.getBit(spatialGrid.getWestTileIndex(tileIndex))) {
                ++numAdjacent;
            }
        }
        if (!spatialGrid.isPosAtEastBorder(bitOffsetXYZ)) {
            if (ownershipBits.getBit(spatialGrid.getEastTileIndex(tileIndex))) {
                ++numAdjacent;
            }
        }
        if (!spatialGrid.isPosAtNorthBorder(bitOffsetXYZ)) {
            if (ownershipBits.getBit(spatialGrid.getNorthTileIndex(tileIndex))) {
                ++numAdjacent;
            }
        }
        return numAdjacent;
    }
}