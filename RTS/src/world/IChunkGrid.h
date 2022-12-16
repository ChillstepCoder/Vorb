#pragma once

#include "world/Chunk.h"

#include <Vorb/Event.hpp>

enum class CHUNK_EVENT_TYPE {
    Create,
    Ready,
    Destroy
};


EVENT_DISPATCHER_TYPE(Chunk, CHUNK_EVENT_TYPE, const Chunk&);

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
    const std::vector<Chunk*>& getLoadingChunks() const { return mLoadingChunks; }
    const std::vector<Chunk*>& getActiveChunks() const { return mActiveChunks; }
    const std::vector<Chunk*>& getDestroyingChunks() const { return mDestroyingChunks; }

    void refresh(const f32v2& loadCenter);

    // Events
    /*bool addCreateListener(ChunkListeners& remover, const ChunkEventDispatcher::Callback& callback) {
        return remover.appendListener(CHUNK_EVENT_TYPE::Create, callback);
    }*/
    STATIC_EVENT_LISTENER_FUNCS(Chunk, Ready, CHUNK_EVENT_TYPE::Ready, const Chunk&);
    STATIC_EVENT_LISTENER_FUNCS(Chunk, Destroy, CHUNK_EVENT_TYPE::Destroy, const Chunk&);

private:
    void markChunkForDestroy(Chunk& chunk);
    void beginHeightLoadForChunk(Chunk& chunk);
    void beginTileLoadForChunk(Chunk& chunk);
    void generateChunkAsync(Chunk& chunk);
    void onChunkReady(Chunk& chunk);

    //void tickChunk(Chunk& chunk);

    // Virtual interface
   // virtual void onChunkFinished(Chunk& chunk) = 0;
    
    Chunk mChunks[WorldData::WORLD_SIZE_CHUNKS];
    BitArray mAliveChunkBits = BitArray(WorldData::WORLD_SIZE_CHUNKS);
    std::vector<Chunk*> mLoadingChunks;
    std::vector<Chunk*> mActiveChunks;
    std::vector<Chunk*> mDestroyingChunks;
    std::vector<TileContainer*> mTileContainersWaitingMeshAndPhysics;

    STATIC_EVENT_DISPATCHER(Chunk);
};

extern IChunkGrid* sChunkGrid;