#pragma once

#include "world/TerrainConstants.h"
#include "world/GridID.h"
#include "terrain/CompressedHeight.h"

#include <bitsery/traits/array.h>

class btCollisionObject;

class HeightmapPatch {
    friend class IHeightmapGrid;
    friend class GameSaveManager;
    friend class WorldSaveContext;
public:
    void init(HeightmapPatchID id) { this->id = id; }

    template<bool THREAD_SAFE = false>
    f32 getHeightAt(int vertPos) const {
        if constexpr (THREAD_SAFE) {
            std::shared_lock lock(mMutex);
            return HEIGHT_STEP * data[vertPos];
        }
        else {
            return HEIGHT_STEP * data[vertPos];
        }
    }

    template<bool THREAD_SAFE = false>
    CompressedHeight getCompressedHeightAt(int vertPos) const {
        if constexpr (THREAD_SAFE) {
            std::shared_lock lock(mMutex);
            return data[vertPos];
        }
        else {
            return data[vertPos];
        }
    }

    void setHeightAt(int vertPos, f32 height) {
        data[vertPos] = compressHeight(glm::clamp(height, MIN_HEIGHT, MAX_HEIGHT));
    }
    // Use if guaranteed that height is withing MIN_HEIGHT and MAX_HEIGHT
    void setHeightAtNoClamp(int vertPos, f32 height) {
        data[vertPos] = compressHeight(height);
    }
    const CompressedHeight* getData() const {
        return data.data();
    }

private:
    std::array<CompressedHeight, HEIGHTMAP_VERT_SIZE_PER_PATCH> data; // Compressed height
public:
    BoundingSphere boundingSphere;
    f32AABB3 aabb;
    HeightmapPatchID id;
    PhysBodyID physBodyID = INVALID_PHYS_BODY_ID;
private:
    mutable std::shared_mutex mMutex;
    mutable std::atomic_flag isSaveUpToDate = ATOMIC_FLAG_INIT;
    i32 mNumActiveChunksThisPatch = 0;

    BINARY_SERIALIZE() {
        s.container2b(data);
    }
};