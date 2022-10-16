#pragma once

#include "world/Chunk.h"

class IWorldGrid;

class IChunkGrid
{
    friend class IWorldGrid;
public:
    IChunkGrid();

    void onWorldBegin(const f32v2& loadCenter) { refresh(loadCenter); }
    void tick();

    Chunk& getChunk(ui32 i) { return mChunks[i]; }
    const Chunk& getChunk(ui32 i) const { return mChunks[i]; }
    Chunk& getChunk(ChunkID id) { return mChunks[id.id]; }
    const Chunk& getChunk(ChunkID id) const { return mChunks[id.id]; }

    static ui32 numChunks() { return WorldData::WORLD_SIZE_CHUNKS; }
    const std::vector<Chunk*>& getActiveChunks() const { return mActiveChunks; }

    void refresh(const f32v2& loadCenter);

private:
    void initChunk(Chunk& chunk);
    void generateChunkAsync(Chunk& chunk);

    bool tickChunk(Chunk& chunk);

    void onChunkDataReady(Chunk& chunk);
    void onChunkAllNeighborsDataReady(Chunk& chunk);
    void dataReadyTryNotifyNeighbor(Chunk& chunk, const ChunkID& id);
    void tryCreateNeighbors(Chunk& chunk);
    void tryCreateNeighbor(Chunk& chunk, const ChunkID& id);

    Chunk mChunks[WorldData::WORLD_SIZE_CHUNKS];
    std::vector<Chunk*> mLoadingChunks;
    std::vector<Chunk*> mActiveChunks;
    std::vector<Chunk*> mDestroyingChunks;
};

extern IChunkGrid* sChunkGrid;