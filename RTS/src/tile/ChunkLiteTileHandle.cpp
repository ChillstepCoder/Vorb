#include "stdafx.h"
#include "ChunkLiteTileHandle.h"

#include "world/World.h"
#include "world/chunk/SimChunkGrid.h"
#include "world/IChunkGrid.h"

i32v2 ChunkLiteTileHandle::getWorldPosition2D(World& world) const {
    assert(isValid());
    return world.getChunkGrid().getChunk(chunkId).getWorldPos() + i32v2(index % CHUNK_WIDTH, index / CHUNK_WIDTH);
}

Chunk& ChunkLiteTileHandle::getChunk(World& world) const {
    assert(isValid());
    return world.getChunkGrid().getChunk(chunkId);
}

SimChunk& ChunkLiteTileHandle::getSimChunk(World& world) const {
    assert(isValid());
    return world.getSimChunkGrid().getChunk(chunkId);
}
