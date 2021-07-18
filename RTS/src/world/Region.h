#pragma once

#include "Chunk.h"

#include "world/WorldData.h"

const ui32 REGION_ID_INVALID = UINT32_MAX;

struct RegionRenderData {
    ~RegionRenderData() {
        if (mLODTexture) {
            glDeleteTextures(1, &mLODTexture);
        }
    }
    VGTexture mLODTexture = 0;
    bool mLODDirty = true;
    bool mIsBuildingLOD = false; // When true, we are waiting for our mesh to be completed
};

struct RegionID {
    RegionID() : id(REGION_ID_INVALID), pos(REGION_ID_INVALID) {}
    RegionID(const RegionID& other) { *this = other; }
    RegionID(ui32 id);
    RegionID(const ui32v2& pos) : pos(pos) { initIdFromPos(); };
    RegionID(ui32v2&& pos) : pos(pos) { initIdFromPos(); };
    RegionID(const f32v2 worldPos);

    // For std::map
    bool operator<(const RegionID& other) const { return id < other.id; }
    bool operator!=(const RegionID& other) const { return id != other.id; }
    bool operator==(const RegionID& other) const { return id == other.id; }

    f32v2 getWorldPos() const { return f32v2(pos.x * WorldData::REGION_WIDTH_TILES, pos.y * WorldData::REGION_WIDTH_TILES); }
    RegionID getLeftID() const { return RegionID(ui32v2(pos.x - 1, pos.y)); }
    RegionID getTopID() const { return RegionID(ui32v2(pos.x, pos.y + 1)); }
    RegionID getRightID() const { return RegionID(ui32v2(pos.x + 1, pos.y)); }
    RegionID getBottomID() const { return RegionID(ui32v2(pos.x, pos.y - 1)); }

    ui32v2 pos;
    ui32 id;

private:
    inline void initIdFromPos() {
        id = pos.y * WorldData::WORLD_WIDTH_REGIONS + pos.x;
    }
};

// A region is a group of chunks, used for LOD in render, simulation, and serialization
class Region {
public:
    void init(const RegionID& id) {
        mRegionId = id;
    }

    f32v2 getWorldPos() const { return mRegionId.getWorldPos(); }

    // Bottom left root position
    Chunk* mChunks = nullptr;

    RegionID mRegionId;

    mutable RegionRenderData mRenderData;
};
