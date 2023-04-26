#pragma once

#include "tile/TileHandle.h"
#include "world/Chunk.h"
#include "world/WorldType.h"
#include "util/StrToken.h"
#include "network/WorldNetMode.h"

class CityGraph;
class IChunkGrid;
class IEntityComponentSystem;
class IHeightmapGrid;
class ItemStockpileRegistry;
class PhysicsWorld;
class StructureManager;
class TimeOfDayManager;
class TileContainerRepository;


// Shared world interface
class IWorld
{
    friend class WorldFactory;
    friend class CliWorldInterface;
protected:
    IWorld(IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid);
    virtual ~IWorld();
public:
    VORB_NON_COPYABLE_BUT_MOVABLE(IWorld);

    // Pure virtual interface
    virtual void onWorldBegin(const f32v2& loadCenter) = 0;
    virtual WorldNetMode getNetMode() = 0;
    virtual WorldType getWorldType() = 0;

    // Shared interface
    void tickShared(f32 elapsedSec);
    entt::entity createEntity(const f32v3& pos, StrToken typeToken, bool shouldReplicate);
    // Queries
    bool terrainTileHasHarvestable(const i32v2& worldPos, TileHarvestable resource, TileLayer* outLayer);
    void efficientEnumTileAABB(const i32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func);

    // mutators
    virtual void dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) = 0;

    // Tile Accessors
    TileHandle getTileHandleAtWorldPos(const i32v3& worldPos) const;
    TileHandle getTileHandleAtWorldPos(const f32v3& worldPos) const;
    TileHandle getTerrainTileHandleAtWorldPos(const ui32v2& worldPos) const { return getTerrainTileHandleAtWorldPos(f32v2(worldPos.x, worldPos.y)); }
    TileHandle getTerrainTileHandleAtWorldPos(const f32v2& worldPos) const;
    TileHandle getTerrainTileHandleAtWorldPos(const i32v2& worldPos) const;
    // TODO: Non vector
    std::vector<Structure*> tryGetStructuresAtWorldPos(const i32v2& worldPos) const;

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
    TimeOfDayManager& getTimeOfDayManager() { return *mTimeOfDayManager; }
    const TimeOfDayManager& getTimeOfDayManager() const { return *mTimeOfDayManager; }
    TileContainerRepository& getTileContainerRepository() { return *mTileContainerRepository; }
    const TileContainerRepository& getTileContainerRepository() const { return *mTileContainerRepository; }


    const f32v2& getLoadCenter() const;

protected:
    void onWorldBeginShared(const f32v2& loadCenter);

    // Server + Client shared world data
    IChunkGrid* mChunkGrid = nullptr;
    IHeightmapGrid* mHeightmapGrid = nullptr;

    f32v2 mLoadCenter = f32v2(0);

    // Tile containers
    std::unique_ptr<TileContainerRepository> mTileContainerRepository;
    // Time of day
    std::unique_ptr<TimeOfDayManager> mTimeOfDayManager;
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

};

// TODO: Make const and use const_cast to set it? Singleton?
extern IWorld* sMainGameWorld;