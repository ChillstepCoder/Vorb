#pragma once

#include "world/ChunkGrid.h"
#include "world/HeightmapGrid.h"


// Contains chunks and height data, handles generation, updating,
// and loading of world data
class WorldGrid : public HeightmapGrid {
    friend class World;
public:
    WorldGrid(World& world);

    void tick(const f32v2& loadCenter);

    Chunk& getChunk(ui32 i) { return mChunkGrid.mChunks[i]; }
    const Chunk& getChunk(ui32 i) const { return mChunkGrid.mChunks[i]; }
    Chunk& getChunk(ChunkID id) { return mChunkGrid.mChunks[id.id]; }
    const Chunk& getChunk(ChunkID id) const { return mChunkGrid.mChunks[id.id]; }
    
    static ui32 numChunks() { return WorldData::WORLD_SIZE_CHUNKS; }

    const std::vector<Chunk*>& getActiveChunks() const { return mChunkGrid.getActiveChunks(); }

private:
    
    ChunkGrid mChunkGrid;
    // Data
    f32v2 mLoadCenter = f32v2(0.0f);
    
};