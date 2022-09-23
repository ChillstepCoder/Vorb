#pragma once

#include "world/Chunk.h"

class WorldGrid;

class ChunkGrid
{
    friend class WorldGrid;
public:
    ChunkGrid(WorldGrid& worldGrid);

    void tick(const f32v2& loadCenter);


    const std::vector<Chunk*>& getActiveChunks() const { return mActiveChunks; }

private:
    void initChunk(Chunk& chunk);
    void generateChunkAsync(Chunk& chunk);

    bool tickChunk(Chunk& chunk);

    void onChunkDataReady(Chunk& chunk);
    void onChunkAllNeighborsDataReady(Chunk& chunk);
    void dataReadyTryNotifyNeighbor(Chunk& chunk, const ChunkID& id);
    void tryCreateNeighbors(Chunk& chunk);
    void tryCreateNeighbor(Chunk& chunk, const ChunkID& id);
    bool isChunkInLoadDistance(const ChunkID& chunkId, float addOffset = 0.0f);

    Chunk mChunks[WorldData::WORLD_SIZE_CHUNKS];
    std::vector<Chunk*> mActiveChunks;
    WorldGrid& mWorldGrid;
    f32v2 mLoadCenter;
};

