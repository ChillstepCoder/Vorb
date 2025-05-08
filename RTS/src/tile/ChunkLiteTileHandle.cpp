#include "stdafx.h"
#include "ChunkLiteTileHandle.h"

#include "world/World.h"
#include "world/chunk/SimChunkGrid.h"
#include "world/LocalChunkGrid.h"

i32v2 ChunkLiteTileHandle::getWorldPosition2D(World& world) const {
    assert(isValid());
    return world.getLocalChunkGrid().getChunk(chunkId).getWorldPos() + i32v2(index % CHUNK_WIDTH, index / CHUNK_WIDTH);
}

LocalChunk& ChunkLiteTileHandle::getChunk(World& world) const {
    assert(isValid());
    return world.getLocalChunkGrid().getChunk(chunkId);
}

SimChunk& ChunkLiteTileHandle::getSimChunk(World& world) const {
    assert(isValid());
    return world.getSimChunkGrid().getChunk(chunkId);
}
