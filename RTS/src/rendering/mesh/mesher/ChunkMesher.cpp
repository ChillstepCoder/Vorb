#include "stdafx.h"
#include "ChunkMesher.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "tile/TileContainer.h"

void ChunkMesher::initMeshAndPhysicsAsync(TileContainer& tileContainer) {
    ASSERT_GAME_THREAD();
    assert(tileContainer.getOwnerType() == TileContainerOwnerType::CHUNK);

    World& world = tileContainer.getWorld();

    // TODO: minimum size instead of entire block
    f32* heightData = new f32[HEIGHTMAP_VERT_SIZE_PER_PATCH];
    const f32* srcData = world.getHeightmapGrid().getHeightDataAtWorldPos(tileContainer.getTileSpatialGrid().getWorldPos2D())->data;
    memcpy(heightData, srcData, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH);

    initMeshAndPhysicsAsyncInternal(tileContainer, heightData, true, 512 /*reserveCount*/, nullptr);
}
