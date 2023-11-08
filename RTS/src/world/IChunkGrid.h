#pragma once

#include "world/Chunk.h"
#include "world/IHeightmapGrid.h"

#include <Vorb/Event.hpp>

class World;

enum class CHUNK_GRID_EVENT_TYPE {
    Create,
    Ready,
    Destroy
};
EVENT_DISPATCHER_TYPE(ChunkGrid, CHUNK_GRID_EVENT_TYPE, Chunk&);

class IWorldGrid;

class IChunkGrid
{
public:
    IChunkGrid();

    void onWorldBegin(const f32v2& loadCenter);
    void tick(const f32v2& loadCenter);

    Chunk& getChunk(ChunkID id) { assert(id < mTotalChunks); return mChunks[id]; }
    const Chunk& getChunk(ChunkID id) const { return mChunks[id]; }

    Chunk& getChunkAtPosition(const f32v2& worldPos);
    const Chunk& getChunkAtPosition(const f32v2& worldPos) const;
    void getClosestChunksAtPosition(const f32v2& worldPos, OUT const Chunk* chunks[4]) const;
    Chunk& getChunkAtPosition(const i32v2& worldPos);
    const Chunk& getChunkAtPosition(const i32v2& worldPos) const;
    Chunk& getChunkAtChunkOffset(const i32v2& chunkOffset);
    const Chunk& getChunkAtChunkOffset(const i32v2& chunkOffset) const;

    ChunkID getChunkIDFromWorldPos(const i32v2& worldPos) const;
    ChunkID getChunkIDFromWorldPos(const f32v2& worldPos) const;
    ChunkID getChunkIDFromChunkOffset(const i32v2& chunkOffset) const;
    i32v2 getWorldPosXYFromChunkID(ChunkID id) const;
    i32v2 getChunkOffsetFromChunkID(ChunkID id) const;

    ui32 getWidthChunks() const { return mWidthChunks; }
    ui32 getTotalChunks() const { return mTotalChunks; }
    const std::vector<LiteChunkID>& getLoadingChunks() const { return mLoadingChunks; }
    const std::vector<LiteChunkID>& getActiveChunks() const { return mActiveChunks; }
    const std::vector<LiteChunkID>& getDestroyingChunks() const { return mDestroyingChunks; }
    size_t getNumActiveChunks() const { return mActiveChunks.size(); }

    World& getWorld() const { return *mWorld; }

    // Events
    EVENT_LISTENER_FUNCS(ChunkGrid, Ready, CHUNK_GRID_EVENT_TYPE::Ready, Chunk&);
    EVENT_LISTENER_FUNCS(ChunkGrid, Destroy, CHUNK_GRID_EVENT_TYPE::Destroy, Chunk&);

    void setWorldAndAllocateChunks(World& world);
protected:
    virtual void updateLoadingChunks();

    bool isChunkXYInBounds(const i32v2& xy);

    // Events
    void onTerrainModified(const boost::container::flat_set<i32v2>& modifiedPositions);
    // Grid management
    void updateGridEdges(const f32v2& loadCenter);
    void makeChunkAlive(const ChunkID& chunkId);
    // List management
    void addChunkToActiveList(Chunk& chunk);
    void removeChunkFromActiveList(Chunk& chunk);
    void addChunkToLoadList(Chunk& chunk);
    void addChunkToDestroyList(Chunk& chunk);
    void removeChunkFromDestroyList(Chunk& chunk);
    // Loading
    void onAllNeighborsAlive(Chunk& chunk);
    void beginHeightLoadForChunk(Chunk& chunk);
    // Ready
    void onChunkReady(Chunk& chunk);
    
    // Chunk data
    ui32 mWidthChunks = 0;
    ui32 mTotalChunks = 0;
    std::unique_ptr<Chunk[]> mChunks;
    std::unique_ptr<ui8[]> mNeighborBits;
    
    // Chunk grid data
    BitArray mAliveChunkBits; // Includes any chunk which is in range, but an "alive" chunk is not active until it loads, which triggers once 8 neighbors are alive
    f32v2 mPrevLoadCenter = f32v2(0);
    bool mForceUpdateEdgeChunks = true;
    std::vector<ChunkID> mEdgeChunkPositions;

    // Chunk lists
    std::vector<ChunkID> mLoadingChunks;
    std::vector<ChunkID> mActiveChunks; // TODO: Can we get rid of this list completely by making chunk nodes an internal doubly linked list?
    std::vector<ChunkID> mDestroyingChunks;

    // World
    World* mWorld = nullptr;

    // Events
    IHeightmapGridListeners mHeightmapGridListeners;
    EVENT_DISPATCHER_DEF(ChunkGrid);
};