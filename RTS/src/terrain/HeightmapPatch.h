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

    template<bool THREAD_SAFE = false>
    f32 getHeightAt(int pos) const {
        if constexpr (THREAD_SAFE) {
            std::shared_lock lock(mMutex);
            return HEIGHT_STEP * data[pos];
        }
        else {
            return HEIGHT_STEP * data[pos];
        }
    }
    void setHeightAt(int pos, f32 height) {
        data[pos] = compressHeight(glm::clamp(height, MIN_HEIGHT, MAX_HEIGHT));
    }
    // Use if guarenteed that height is withing MIN_HEIGHT and MAX_HEIGHT
    void setHeightAtNoClamp(int pos, f32 height) {
        data[pos] = compressHeight(height);
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
    mutable std::shared_mutex mMutex;
};

// TODO: Remove the middleman
class HeightmapPatch {
public:
    HeightmapPatch() = default;
    ~HeightmapPatch() = default;

    template<bool THREAD_SAFE = false>
    f32 getHeightAt(int pos) const {
        assert(mHeightData);
        return mHeightData->getHeightAt<THREAD_SAFE>(pos);
    }   

    HeightmapPatchData* mHeightData = nullptr;
};
static_assert(sizeof(HeightmapPatch) == 8, "Keep small");
