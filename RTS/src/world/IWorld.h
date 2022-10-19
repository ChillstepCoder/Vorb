#pragma once

#include "tile/TileHandle.h"
#include "world/Chunk.h"
#include "util/StrToken.h"

class IChunkGrid;
class IHeightmapGrid;
class IEntityComponentSystem;
class PhysicsWorld;
class StructureManager;
class ItemStockpileRegistry;
class CityGraph;
class HeightmapTerrainQuadtree;

// Shared world interface
class IWorld
{
    friend class WorldFactory;
protected:
    IWorld(IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid);
    virtual ~IWorld();
public:

    VORB_NON_COPYABLE_BUT_MOVABLE(IWorld);

    // Pure virtual interface
    virtual void onWorldBegin(const f32v2& loadCenter) = 0;

    // Shared interface
    void tickShared(f32 elapsedSec);
    void setTimeOfDay(f32 time);
    entt::entity createEntity(const f32v3& pos, StrToken typeToken, bool shouldReplicate);
    // Queries
    bool tileHasHarvestableResource(const i32v2& worldPos, TileResource resource, TileLayer* outLayer);
    void efficientEnumTileAABB(const i32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func);

    // Chunk Accessors
    Chunk& getChunkAtChunkCoords(const i32v2& worldPos);
    Chunk& getChunkAtPosition(const f32v2& worldPos);
    const Chunk& getChunkAtPosition(const f32v2& worldPos) const;
    Chunk& getChunkAtPosition(const i32v2& worldPos);
    const Chunk& getChunkAtPosition(const i32v2& worldPos) const;
    Chunk& getChunkAtPosition(const ui16v2& worldPos);
    const Chunk& getChunkAtPosition(const ui16v2& worldPos) const;
    Chunk& getChunk(ChunkID chunkId);
    const Chunk& getChunk(ChunkID chunkId) const;
    Chunk& getChunk(ui32 chunkId);
    const Chunk& getChunk(ui32 chunkId) const;
    size_t getNumActiveChunks() const;
    const std::vector<Chunk*>& getActiveChunks() const;

    // mutators
    virtual void dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius) = 0;

    // Tile Accessors
    TileHandle getTileHandleAtWorldPosThreadSafe(const i32v3& worldPos) const;
    TileHandle getTileHandleAtWorldPos(const i32v3& worldPos) const;
    TileHandle getTileHandleAtWorldPos(const f32v3& worldPos) const;
    TileHandle getTerrainTileHandleAtWorldPos(const ui32v2& worldPos) const { return getTerrainTileHandleAtWorldPos(f32v2(worldPos.x, worldPos.y)); }
    TileHandle getTerrainTileHandleAtWorldPos(const f32v2& worldPos) const;
    TileHandle getTerrainTileHandleAtWorldPos(const i32v2& worldPos) const;
    StructureArrayPtr tryGetStructuresAtWorldPos(const i32v2& worldPos) const;

    void enumActiveChunks(std::function<void(const Chunk&)> func) const;

    // Accessors 
    IHeightmapGrid& getHeightmapGrid() { return *mHeightmapGrid; }
    const IHeightmapGrid& getHeightmapGrid() const { return *mHeightmapGrid; }
    IChunkGrid& getChunkGrid() { return *mChunkGrid; }
    const IChunkGrid& getChunkGrid() const { return *mChunkGrid; }
    CityGraph& getCityGraph() { return *mCities; }
    const CityGraph& getCityGraph() const { return *mCities; }
    PhysicsWorld& getPhysicsWorld() { return *mPhysWorld; }
    const PhysicsWorld& getPhysicsWorld() const { return *mPhysWorld; }
    IEntityComponentSystem& getECS() { return *mEcs; }
    const IEntityComponentSystem& getECS() const { return *mEcs; }
    ItemStockpileRegistry& getItemStockpileRegistry() const { return *mItemStockpileRegistry; }
    StructureManager& getStructureManager() { return *mStructureManager; }
    const StructureManager& getStructureManager() const { return *mStructureManager; }

    // [-1.0, 1.0]
    float getSunHeight() const { return mSunHeight; }
    const f32v3& getSunPosition() const { return mSunPosition; }
    float getTimeOfDay() const { return mTimeOfDay; }
    const f32v3& getSunColor() const { return mSunColor; }
    const f32m4& getSkyRotMatrix() const { return mSkyRotMatrix; }
    const f32v2& getLoadCenter() const;

protected:
    void onWorldBeginShared(const f32v2& loadCenter);
    void refreshWorld();
    void sharedDirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius);

    void updateTimeOfDay();

    void updateCities();

    // Server + Client shared world data
    IChunkGrid* mChunkGrid = nullptr;
    IHeightmapGrid* mHeightmapGrid = nullptr;

    f32v2 mPrevLoadCenter = f32v2(0);
    f32v2 mLoadCenter = f32v2(0);

    // ECS
    std::unique_ptr<IEntityComponentSystem> mEcs;
    // Physics
    std::unique_ptr<PhysicsWorld> mPhysWorld;
    // Structures
    std::unique_ptr<StructureManager> mStructureManager;
    // Stockpiles
    std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;
    // Cities
    std::unique_ptr<CityGraph> mCities;

    // Sunlight and time of day
    float mSunHeight = 1.0f;
    f32v3 mSunPosition = f32v3(0.0f, 0.0f, 1.0f);
    float mTimeOfDay = 0.0f; // span of 24:00
    f32v3 mSunColor = f32v3(1.0f);
    f32m4 mSkyRotMatrix = f32m4(1.0f);
};

// TODO: Make const and use const_cast to set it? Singleton?
extern IWorld* sWorld;