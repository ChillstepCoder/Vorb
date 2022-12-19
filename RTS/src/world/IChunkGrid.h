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

    void onWorldBegin(const f32v2& loadCenter);
    void tick(const f32v2& loadCenter);

    Chunk& getChunk(ui32 i) { return mChunks[i]; }
    const Chunk& getChunk(ui32 i) const { return mChunks[i]; }
    Chunk& getChunk(ChunkID id) { return mChunks[id.id]; }
    const Chunk& getChunk(ChunkID id) const { return mChunks[id.id]; }

    static ui32 numChunks() { return WorldData::WORLD_SIZE_CHUNKS; }
    const std::vector<Chunk*>& getLoadingChunks() const { return mLoadingChunks; }
    const std::vector<Chunk*>& getActiveChunks() const { return mActiveChunks; }
    const std::vector<Chunk*>& getDestroyingChunks() const { return mDestroyingChunks; }

    // Events
    /*bool addCreateListener(ChunkListeners& remover, const ChunkEventDispatcher::Callback& callback) {
        return remover.appendListener(CHUNK_EVENT_TYPE::Create, callback);
    }*/
    STATIC_EVENT_LISTENER_FUNCS(Chunk, Ready, CHUNK_EVENT_TYPE::Ready, const Chunk&);
    STATIC_EVENT_LISTENER_FUNCS(Chunk, Destroy, CHUNK_EVENT_TYPE::Destroy, const Chunk&);

private:
    void updateGridEdges(const f32v2& loadCenter);
    void markChunkForDestroy(Chunk& chunk);
    void beginHeightLoadForChunk(Chunk& chunk);
    void beginTileLoadForChunk(Chunk& chunk);
    void generateChunkAsync(Chunk& chunk);
    void onChunkReady(Chunk& chunk);
    
    // Chunk data
    Chunk mChunks[WorldData::WORLD_SIZE_CHUNKS];
    
    // Chunk grid data
    BitArray mAliveChunkBits = BitArray(WorldData::WORLD_SIZE_CHUNKS); // Includes both simulating chunks, and edge chunks which will not be simulating
    bool mForceUpdateEdgeChunks = true;
    std::vector<ChunkID> mEdgeChunkPositions;
    f32v2 mPrevLoadCenter = f32v2(0);

    // Chunk lists
    std::vector<Chunk*> mLoadingChunks;
    std::vector<Chunk*> mActiveChunks; // TODO: Can we get rid of this list completely by making chunk nodes an internal doubly linked list?
    std::vector<Chunk*> mDestroyingChunks;
    std::vector<TileContainer*> mTileContainersWaitingMeshAndPhysics;

    // Events
    STATIC_EVENT_DISPATCHER(Chunk);
};

extern IChunkGrid* sChunkGrid;