#include "stdafx.h"
#include "ChunkMesher.h"

ChunkMesher sChunkMesher;

void ChunkMesher::initMeshAndPhysicsAsync(TileContainer& chunk, const f32* heightData) {
    initMeshAndPhysicsAsyncInternal(chunk, heightData, true, 512 /*reserveCount*/, nullptr);
}
