#pragma once

#include "world/TerrainConstants.h"
#include "world/ChunkID.h"

#include <mutex>

class btRigidBody;

enum HeightmapPatchFlags : ui32 {
    HEIGHTMAP_PATCH_FLAG_GENERATING = 1 << 0,
    HEIGHTMAP_PATCH_FLAG_DONE = 1 << 1
};

struct HeightmapPatchData {
    HeightmapPatchData(HeightmapPatchID id) : id(id) {};

    f32 data[HEIGHTMAP_VERT_SIZE_PER_PATCH];
    BoundingSphere boundingSphere;
    f32AABB3 aabb;
    HeightmapPatchID id;
    btRigidBody* mCollider = nullptr;
};

class HeightmapPatch {
public:
    HeightmapPatch() = default;
    ~HeightmapPatch() = default;

    bool isDone() const { return mFlags & HEIGHTMAP_PATCH_FLAG_DONE; }
    bool isGenerating() const { return mFlags & HEIGHTMAP_PATCH_FLAG_GENERATING; }

    ui32 mFlags = 0u;
    ui32 mRefCount = 0u;
    HeightmapPatchData* mHeightData = nullptr;
};
static_assert(sizeof(HeightmapPatch) == 16, "Keep small");
