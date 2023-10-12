#pragma once

#include "tile/TileHandle.h"
#include "network/WorldNetMode.h"

#include "generation/WorldGeneratorType.h"

class Structure;
class Chunk;
class CityGraph;
class IChunkGrid;
class IEntityComponentSystem;
class IHeightmapGrid;
class ItemStockpileRegistry;
class PhysicsWorld;
class StructureManager;
class TimeOfDayManager;
class TileContainerRepository;
class IWorldGenerator;
class CombatContext;


// Shared world interface
class IWorld
{
    friend class WorldFactory;
    friend class CliWorldInterface;
protected:
    IWorld(ui32 widthTiles, IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid, WorldGeneratorType generatorType);
public:
    virtual ~IWorld();
    VORB_NON_COPYABLE_BUT_MOVABLE(IWorld);


    // Pure virtual interface
    virtual void init() = 0;
    virtual void onWorldBegin(const f32v2& loadCenter) = 0;
    virtual WorldNetMode getNetMode() const = 0;
    bool isEditorWorld() const { return getNetMode() == WorldNetMode::Editor; }
    virtual void tick(f32 elapsedSec) = 0;


    // Shared interface
    void tickShared(f32 elapsedSec);
    entt::entity createEntity(const f32v3& pos, StrToken typeToken, bool shouldReplicate);
    // Queries
    bool terrainTileHasHarvestable(const i32v2& worldPos, TileHarvestable resource, TileLayer* outLayer);
    void efficientEnumTileAABB(const i32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func);

    // Tile Accessors
    TileHandle getTileHandleAtWorldPos(const i32v3& worldPos) const;
    TileHandle getTileHandleAtWorldPos(const f32v3& worldPos) const;
    TileHandle getTerrainTileHandleAtWorldPos(const ui32v2& worldPos) const { return getTerrainTileHandleAtWorldPos(f32v2(worldPos.x, worldPos.y)); }
    TileHandle getTerrainTileHandleAtWorldPos(const f32v2& worldPos) const;
    TileHandle getTerrainTileHandleAtWorldPos(const i32v2& worldPos) const;
    // TODO: Non vector
    std::vector<Structure*> tryGetStructuresAtWorldPos(const i32v2& worldPos) const;

    // Accessors 
    IHeightmapGrid& getHeightmapGrid() const { return *mHeightmapGrid; }
    IChunkGrid& getChunkGrid() const { return *mChunkGrid; }
    CityGraph& getCityGraph() const { return *mCities; }
    PhysicsWorld& getPhysicsWorld() const { return *mPhysWorld; }
    IEntityComponentSystem& getECS() const { /*ASSERT_GAME_THREAD();*/ return *mEcs; }//  TODO: GameThreadAssert should be on
    StructureManager& getStructureManager() const { return *mStructureManager; }
    TimeOfDayManager& getTimeOfDayManager() const { return *mTimeOfDayManager; }
    TileContainerRepository& getTileContainerRepository() const { return *mTileContainerRepository; }
    IWorldGenerator& getWorldGenerator() const { return *mWorldGenerator; }
    CombatContext& getCombatContext() const { return *mCombatContext; }

    f32v3 getDefaultSpawn() const { return f32v3(mWidthTiles * 0.5f, mWidthTiles * 0.5f, 20.0f); }
    f32v2 getWorldCenter() const { return f32v2(mWidthTiles * 0.5f); }
    f32v2 getLoadCenter() const;
    ui32 getWidthTiles() const { return mWidthTiles; }
    ui32 getWidthChunks() const { return mWidthTiles / CHUNK_WIDTH; }
    ui32 getWidthHeightmapPatches() const { return mWidthTiles / HEIGHTMAP_WIDTH; }

    void setLoadCenter(const f32v2& loadCenter);

protected:
    void onWorldBeginShared(const f32v2& loadCenter);

    // Server + Client shared world data
    IChunkGrid* mChunkGrid = nullptr;
    IHeightmapGrid* mHeightmapGrid = nullptr;

    // TODO: Is this still needed? Can we make a ThreadSafeDataContainer?
    mutable std::mutex mLoadCenterMutex;
    f32v2 mLoadCenter = f32v2(0);
    ui32 mWidthTiles;

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
    // Cities
    std::unique_ptr<CityGraph> mCities;
    // Generation
    std::unique_ptr<IWorldGenerator> mWorldGenerator;
    // Combat
    std::unique_ptr<CombatContext> mCombatContext;

};
