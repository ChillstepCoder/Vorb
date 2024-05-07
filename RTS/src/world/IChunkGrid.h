#pragma once

#include "world/Chunk.h"
#include "world/IHeightmapGrid.h"
#include "ChunkGridEvent.h"

class World;


struct ChunkActivateContext {
    std::atomic_int refCount = 1;
};
//
//class ChunkActivationEvent : public ChunkGridEvent {
//    using ChunkGridEvent::ChunkGridEvent;
//    ChunkActivateContext* context;
//};

class IWorldGrid;

class IChunkGrid
{
public:
    IChunkGrid();
    virtual ~IChunkGrid();

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
    const std::vector<ChunkID>& getActivatingChunks() const { return mActivatingChunks; }
    const std::vector<ChunkID>& getActiveChunks() const { return mActiveChunks; }
    const std::vector<ChunkID>& getWantDeactivateChunks() const { return mWantDeactivateChunks; }
    size_t getNumActiveChunks() const { return mActiveChunks.size(); }

    World& getWorld() const { return *mWorld; }

    // Events
    EVENT_LISTENER_FUNCS(ChunkGrid, BeginActivate, CHUNK_GRID_EVENT_TYPE::BeginActivate, ChunkGridEvent&);
    EVENT_LISTENER_FUNCS(ChunkGrid, BeginLoad, CHUNK_GRID_EVENT_TYPE::BeginLoad, ChunkGridEvent&);
    EVENT_LISTENER_FUNCS(ChunkGrid, Activated, CHUNK_GRID_EVENT_TYPE::Activated, ChunkGridEvent&);
    EVENT_LISTENER_FUNCS(ChunkGrid, Deactivated, CHUNK_GRID_EVENT_TYPE::Deactivated, ChunkGridEvent&);

    void setWorldAndAllocateChunks(World& world);
protected:
    virtual void updateActivatingChunks();

    bool isChunkXYInBounds(const i32v2& xy);

    // Events
    void onTerrainModified(const boost::container::flat_set<i32v2>& modifiedPositions);
    // Grid management
    void updateGridEdges(const f32v2& loadCenter);
    bool tryMarkChunkAlive(const ChunkID& chunkId);
    // List management
    void addChunkToActiveList(Chunk& chunk);
    void removeChunkFromActiveList(Chunk& chunk);
    void addChunkToActivatingList(Chunk& chunk);
    void addChunkToWantDeactivateList(Chunk& chunk);
    void removeChunkFromWantDeactivateList(Chunk& chunk);
    // Loading
    void onAllNeighborsAlive(Chunk& chunk);
    // Ready
    void activateChunk(Chunk& chunk);
    
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
    std::vector<ChunkID> mActivatingChunks;
    std::vector<ChunkID> mActiveChunks; // TODO: Can we get rid of this list completely by making chunk nodes an internal doubly linked list?
    std::vector<ChunkID> mWantDeactivateChunks;

    // World
    World* mWorld = nullptr;
    // Events
    IHeightmapGridListeners mHeightmapGridListeners;
    EVENT_DISPATCHER_DEF(ChunkGrid);
};