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
    }

    i32v3 getTileXYZOffsetWithZScale(TileIndex i) const {
        const i32 layerSize = mTileDims.x * mTileDims.y;
        return i32v3(i % mTileDims.x, (i % layerSize) / mTileDims.x, (i / layerSize) * mFloorHeight);
    }
    static i32v3 getTileXYZOffsetWithZScale(TileIndex i, const i32v3& dims, i32 floorHeight) {
        const i32 layerSize = dims.x * dims.y;
        return i32v3(i % dims.x, (i % layerSize) / dims.x, (i / layerSize) * floorHeight);
    }
    i32v3 getTileXYZOffset(TileIndex i) const {
        const i32 layerSize = mTileDims.x * mTileDims.y;
        return i32v3(i % mTileDims.x, (i % layerSize) / mTileDims.x, i / layerSize);
    }
    static i32v3 getTileXYZOffset(TileIndex i, const i32v3& dims) {
        const i32 layerSize = dims.x * dims.y;
        return i32v3(i % dims.x, (i % layerSize) / dims.x, i / layerSize);
    }
    i32v2 getTileXYOffset(TileIndex i) const {
        const i32 layerSize = mTileDims.x * mTileDims.y;
        return i32v2(i % mTileDims.x, (i % layerSize) / mTileDims.x);
    }
    static i32v2 getTileXYOffset(TileIndex i, const i32v2& dims) {
        const i32 layerSize = dims.x * dims.y;
        return i32v2(i % dims.x, (i % layerSize) / dims.x);
    }
    i32v2 getTileBaseWorldPos2D(TileIndex i) const {
        const i32v2 worldRoot(mAABB.pos.x, mAABB.pos.y);
        return i32v2(worldRoot.x + (i % mTileDims.x), worldRoot.y + (i / mTileDims.x));
    }
    i32v3 getTileBaseWorldPos3D(TileIndex i) const {
        const i32 layerSize = mTileDims.x * mTileDims.y;
        const i32v3 worldRoot(mAABB.pos.x, mAABB.pos.y, mAABB.pos.z);
        return i32v3(worldRoot.x + (i % mTileDims.x), worldRoot.y + ((i % layerSize) / mTileDims.x), worldRoot.z + (i / layerSize) * mFloorHeight);
    }
    f32v3 getTileCenterWorldPos3D(TileIndex i, f32 tileGroundZOffset) const {
        const i32 layerSize = mTileDims.x * mTileDims.y;
        return f32v3(mAABB.pos.x + (i % mTileDims.x) + 0.5f, mAABB.pos.y + ((i % layerSize) / mTileDims.x) + 0.5f, mAABB.pos.z + (i / layerSize) * mFloorHeight + tileGroundZOffset);
    }
    f32v3 getTileWorldPos3D(TileIndex i, f32 tileGroundZOffset) const {
        const i32 layerSize = mTileDims.x * mTileDims.y;
        return f32v3(mAABB.pos.x + (i % mTileDims.x), mAABB.pos.y + ((i % layerSize) / mTileDims.x), mAABB.pos.z + (i / layerSize) * mFloorHeight + tileGroundZOffset);
    }
    static TileIndex getTileIndexFromXYZOffset(const ui32v3& xyz, const ui32v3& dims) {
        return xyz.x + xyz.y * dims.x + xyz.z * dims.x * dims.y;
    }
    static TileIndex getTileIndexFromXYZOffset(const i32v3& xyz, const i32v3& dims) {
        return xyz.x + xyz.y * dims.x + xyz.z * dims.x * dims.y;
    }
    static TileIndex getBaseTileIndexFromXYOffset(const i32v2& xy, const i32v3& dims) {
        return xy.x + xy.y * dims.x;
    }
    TileIndex getTileIndexFromXYZOffset(const ui32v3& xyz) const {
        return xyz.x + xyz.y * mTileDims.x + xyz.z * mTileDims.x * mTileDims.y;
    }
    TileIndex getTileIndexFromXYZOffset(const i32v3& xyz) const {
        return (TileIndex)(xyz.x + xyz.y * mTileDims.x + xyz.z * mTileDims.x * mTileDims.y);
    }
    TileIndex getTileIndexFromXYZOffset(i32 x, i32 y, i32 z) const {
        return (TileIndex)(x + y * mTileDims.x + z * mTileDims.x * mTileDims.y);
    }

    const i32v2& getDims2D() const { return reinterpret_cast<const i32v2&>(mTileDims); }
    const i32v3& getDims() const { return mTileDims; }
    const i32AABB3 getAABB() const { return mAABB; }
    i32 getFloorStride() const { return mTileDims.x * mTileDims.y; }
    i32 getNumTiles() const { return mNumTiles; }
    i32 getFloorHeight() const { return mFloorHeight; }
    
    const i32v2& getWorldPos2D() const { return reinterpret_cast<const i32v2&>(mAABB.pos); }
    const i32v3& getWorldPos3D() const { return mAABB.pos; }
    const f32v3 getWorldPosCenter3D() const { return f32v3(mAABB.pos) + f32v3(mAABB.dims) * 0.5f; }

private:

    i32AABB3 mAABB;
    i32v3 mTileDims;
    i32 mNumTiles = 0;
    i32 mFloorHeight = 0;
};