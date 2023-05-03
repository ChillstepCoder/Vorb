#include "stdafx.h"
#include "ChunkMesher.h"

#include "world/IWorld.h"
#include "world/IHeightmapGrid.h"
#include "tile/TileContainer.h"

void ChunkMesher::initMeshAndPhysicsAsync(TileContainer& tileContainer) {
    ASSERT_GAME_THREAD();
    assert(tileContainer.getOwnerType() == TileContainerOwnerType::CHUNK);

    IWorld& world = tileContainer.getWorld();

    // TODO: minimum size instead of entire block
    f32* heightData = new f32[HEIGHTMAP_VERT_SIZE_PER_PATCH];
    const f32* srcData = world.getHeightmapGrid().getHeightDataAt(HeightmapPatchID::fromWorldI32v2(tileContainer.getTileSpatialGrid().getWorldPos2D()))->data;
    memcpy(heightData, srcData, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH);

    initMeshAndPhysicsAsyncInternal(tileContainer, heightData, true, 512 /*reserveCount*/, nullptr);
}
