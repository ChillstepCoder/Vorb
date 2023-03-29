#include "stdafx.h"
#include "ChunkMesher.h"

#include "world/IHeightmapGrid.h"
#include "tile/TileContainer.h"

void ChunkMesher::initMeshAndPhysicsAsync(TileContainer& tileContainer) {
    assert(IS_GAME_THREAD());
    assert(tileContainer.getOwnerType() == TileContainerOwnerType::CHUNK);

    // TODO: minimum size instead of entire block
    f32* heightData = new f32[HEIGHTMAP_VERT_SIZE_PER_PATCH];
    const f32* srcData = sHeightmapGrid->getHeightDataAt(HeightmapPatchID::fromWorldI32v2(tileContainer.getWorldPos2D()))->data;
    memcpy(heightData, srcData, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH);

    initMeshAndPhysicsAsyncInternal(tileContainer, heightData, true, 512 /*reserveCount*/, nullptr);
}
