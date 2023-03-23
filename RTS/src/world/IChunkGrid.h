#pragma once

#include "world/Chunk.h"

#include <Vorb/Event.hpp>

enum class CHUNK_EVENT_TYPE {
    Create,
    Ready,
    Destroy
};
EVENT_DISPATCHER_TYPE(Chunk, CHUNK_EVENT_TYPE, Chunk&);

class IWorldGrid;

class IChunkGrid
{
    friend class IWorldGrid;
public:
    IChunkGrid();

    void onWorldBegin(const f32v2& loadCenter);
    void tick(const f32v2& loadCenter);

    Chunk& getChunk(LiteChunkID i) { return mChunks[i]; }
    const Chunk& getChunk(LiteChunkID i) const { return mChunks[i]; }
    Chunk& getChunk(ChunkID id) { return mChunks[id.id]; }
    const Chunk& getChunk(ChunkID id) const { return mChunks[id.id]; }

    static ui32 numChunks() { return WorldData::WORLD_SIZE_CHUNKS; }
    const std::vector<LiteChunkID>& getLoadingChunks() const { return mLoadingChunks; }
    const std::vector<LiteChunkID>& getActiveChunks() const { return mActiveChunks; }
    const std::vector<LiteChunkID>& getDestroyingChunks() const { return mDestroyingChunks; }

    void onTerrainModified(const boost::container::flat_set<i32v2>& modifiedPositions);

    // Events
    /*bool addCreateListener(ChunkListeners& remover, const ChunkEventDispatcher::Callback& callback) {
        return remover.appendListener(CHUNK_EVENT_TYPE::Create, callback);
    }*/
    STATIC_EVENT_LISTENER_FUNCS(Chunk, Ready, CHUNK_EVENT_TYPE::Ready, Chunk&);
    STATIC_EVENT_LISTENER_FUNCS(Chunk, Destroy, CHUNK_EVENT_TYPE::Destroy, Chunk&);

private:
    // Grid management
    void updateGridEdges(const f32v2& loadCenter);
    void makeChunkAlive(const ChunkID& chunkId);
    // List management
    void addChunkToActiveList(Chunk& chunk);
    void removeChunkFromActiveList(Chunk& chunk);
    void addChunkToLoadList(Chunk& chunk);
    void removeChunkFromLoadList(Chunk& chunk);
    void addChunkToDestroyList(Chunk& chunk);
    void removeChunkFromDestroyList(Chunk& chunk);
    // Loading
    void onAllNeighborsAlive(Chunk& chunk);
    void beginHeightLoadForChunk(Chunk& chunk);
    void beginTileLoadForChunk(Chunk& chunk);
    void generateChunkAsync(Chunk& chunk);
    // Ready
    void onChunkReady(Chunk& chunk);
    
    // Chunk data
    Chunk mChunks[WorldData::WORLD_SIZE_CHUNKS];
    
    // Chunk grid data
    BitArray mAliveChunkBits = BitArray(WorldData::WORLD_SIZE_CHUNKS); // Includes any chunk which is in range, but an "alive" chunk is not active until it loads, which triggers once 8 neighbors are alive
    ui8 mNeighborBits[WorldData::WORLD_SIZE_CHUNKS] = {};
    f32v2 mPrevLoadCenter = f32v2(0);
    bool mForceUpdateEdgeChunks = true;
    std::vector<LiteChunkID> mEdgeChunkPositions;

    // Chunk lists
    std::vector<LiteChunkID> mLoadingChunks;
    std::vector<LiteChunkID> mActiveChunks; // TODO: Can we get rid of this list completely by making chunk nodes an internal doubly linked list?
    std::vector<LiteChunkID> mDestroyingChunks;
    std::vector<TileContainer*> mTileContainersWaitingMeshAndPhysics;

    // Events
    STATIC_EVENT_DISPATCHER(Chunk);
};
extern IChunkGrid* sChunkGrid;