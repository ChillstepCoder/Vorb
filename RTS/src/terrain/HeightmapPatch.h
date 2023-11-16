#pragma once

#include "world/TerrainConstants.h"
#include "world/ChunkID.h"
#include "terrain/CompressedHeight.h"

#include <shared_mutex>

class btCollisionObject;

class HeightmapPatchData {
    friend class IHeightmapGrid;
public:
    HeightmapPatchData(HeightmapPatchID id) : id(id) {};

    f32 getHeightAt(int pos) {
        return HEIGHT_STEP * data[pos];
    }
    void setHeightAt(int pos, f32 height) {
        data[pos] = (CompressedHeight)glm::round(glm::clamp(height, MIN_HEIGHT, MAX_HEIGHT) / HEIGHT_STEP);
    }
    const CompressedHeight* getData() const {
        return data;
    }
private:
    CompressedHeight data[HEIGHTMAP_VERT_SIZE_PER_PATCH]; // Compressed height
public:
    BoundingSphere boundingSphere;
    f32AABB3 aabb;
    HeightmapPatchID id;
    btCollisionObject* mCollider = nullptr;
    mutable std::shared_mutex mMutex;
};

class HeightmapPatch {
public:
    HeightmapPatch() = default;
    ~HeightmapPatch() = default;

    HeightmapPatchData* mHeightData = nullptr;
};
static_assert(sizeof(HeightmapPatch) == 8, "Keep small");
