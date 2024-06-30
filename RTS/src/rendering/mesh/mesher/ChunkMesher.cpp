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
    CompressedHeight* heightData = new CompressedHeight[HEIGHTMAP_VERT_SIZE_PER_PATCH];
    const CompressedHeight* srcData = world.getHeightmapGrid().getHeightDataAtWorldPos(tileContainer.getWorldPos())->getData();
    memcpy(heightData, srcData, sizeof(CompressedHeight) * HEIGHTMAP_VERT_SIZE_PER_PATCH);

    buildMeshAndPhysicsAsyncInternal(tileContainer, heightData, true, 512 /*reserveCount*/, nullptr);
}
