#pragma once

// TODO: This is a heavy include
#include "tile/TileHandle.h"

#include "network/WorldNetMode.h"

#include "tile/TileHarvestable.h"
#include "world/WorldEvents.h"

#include <shared_mutex>

class HostWorldData;
class Building;
class Camera3D;
class Chunk;
class IChunkGrid;
class IFullECS;
class IHeightmapGrid;
class ItemStockpileRegistry;
class IEffectContext;
class IFactionManager;
class PhysicsWorld;
class BuildingGrid;
class TimeOfDayManager;
class TileContainerRepository;
class TileContainerLoader;
class ChunkGenerator;
class BiomeGrid;
class TerrainSurfaceGrid;
class SimChunkGrid;
class OwnershipGrid;
class WorldMarkupGrid;
class HostSimContext;
class SimECS;
class CombatContext;
class NavWorld;
class FishEcosystem;
class WorldRenderState;
class VisibilityManager;
class WeatherManager;
class WorldSaveContext;

// Represents a total game context. Multiple can exist at once, for example editor world + host world. We could also
// potentially do seamless transitions between two host/client worlds with portals or other weirdness.
class World {
    friend class WorldDestroyer;
public:
    World(WorldNetMode netMode, HostWorldData* hostWorldData);
    ~World();

    VORB_NON_COPYABLE_BUT_MOVABLE(World);

    virtual void onWorldBeginGame(const f32v2& loadCenter);
    virtual void tick(f32 elapsedSec);

    void setDefaultWorldSpawn(const f32v2& spawnUV) { mDefaultPlayerSpawnUV = spawnUV; }

    void shutdown();
    static void shutdownAllWorlds();
    bool isShuttingDown() const { return mIsShuttingDown; }

    // World info
    virtual WorldNetMode getNetMode() const { return mNetMode; }
    bool isEditorWorld() const { return getNetMode() == WorldNetMode::Editor; }
    bool isHostWorld() const { return getNetMode() == WorldNetMode::Host; }
    f32v3 getDefaultSpawn() const;
    f32v2 getWorldCenter() const { return f32v2(mWidthTiles * 0.5f); }
    f32v2 getLoadCenter() const;
    void setLoadCenter(const f32v2& loadCenter);
    ui32 getWidthTiles() const { return mWidthTiles; }
    ui32 getWidthDTiles() const { return mWidthDTiles; }
    ui32 getWidthChunks() const { return mWidthChunks; }
    ui32 getTotalChunks() const { return mTotalChunks; }
    ui32 getWidthHeightmapPatches() const { return mWidthTiles / HEIGHTMAP_PATCH_WIDTH_TILES; }
    WorldID getId() const { return mId; }
    ui64 getWorldTimeMs() const { return mWorldTimeMs; }
    void setWorldTimeMs(ui64 newTime);
    ui32 getSeed() const { return mSeed; }
    inline ChunkID getChunkIDAtWorldPos(f32v2 worldPos) const { return (ChunkID)(((i32)worldPos.y / CHUNK_WIDTH) * mWidthChunks + ((i32)worldPos.x / CHUNK_WIDTH)); }
    inline TileCoord getChunkWorldPos(ChunkID id) const { return TileCoord((id % mWidthChunks) * CHUNK_WIDTH, (id / mWidthChunks) * CHUNK_WIDTH); }

    // System Accessors 
    IHeightmapGrid& getHeightmapGrid() const { return *mHeightmapGrid; }
    BiomeGrid& getBiomeGrid() const { return *mBiomeGrid; }
    TerrainSurfaceGrid& getTerrainSurfaceGrid() const { return *mTerrainSurfaceGrid; }
    SimChunkGrid& getSimChunkGrid() const { return *mSimChunkGrid; }
    WorldMarkupGrid& getMarkupGrid() const { return *mMarkupGrid; }
    OwnershipGrid& getOwnershipGrid() const { return *mOwnershipGrid; }
    IChunkGrid& getChunkGrid() const { return *mChunkGrid; }
    PhysicsWorld& getPhysicsWorld() const { return *mPhysWorld; }
    IFullECS& getECS() const { /*ASSERT_GAME_THREAD();*/ return *mEcs; }//  TODO: GameThreadAssert should be on
    BuildingGrid& getBuildingGrid() const { return *mStructureGrid; }
    TimeOfDayManager& getTimeOfDayManager() const { return *mTimeOfDayManager; }
    TileContainerRepository& getTileContainerRepository() const { return *mTileContainerRepository; }
    TileContainerLoader& getTileContainerLoader() const { return *mTileContainerLoader; }
    ChunkGenerator& getWorldGenerator() const { return *mChunkGenerator; }
    CombatContext& getCombatContext() const { return *mCombatContext; }
    ItemStockpileRegistry& getItemStockpileRegistry() const { return *mItemStockpileRegistry; }
    FishEcosystem& getFishEcosystem() const { return *mFishEcosystem; }
    IEffectContext& getEffectContext() const { return *mEffectContext; }
    VisibilityManager& getVisibilityManager() const { return *mVisibilityManager; }
    WeatherManager& getWeatherManager() const { return *mWeatherManager; }
    HostSimContext* tryGetHostSimContext() const { return mHostSimContext.get(); }
    SimECS* tryGetSimECS() const;
    IFactionManager& getFactionManager() const { return *mFactionManager; }
    WorldSaveContext& getSaveContext() const { return *mSaveContext; }

    // Optional system accessors 
    NavWorld* tryGetNavWorld() const { return mNavWorld.get(); }

    // Tile Accessors
    TileHandle getTileHandleAtWorldPos(const i32v3& worldPos) const;
    TileHandle getTileHandleAtWorldPos(const f32v3& worldPos) const;
    TileHandle getTerrainTileHandleAtWorldPos(const ui32v2& worldPos) const;
    TileHandle getTerrainTileHandleAtWorldPos(const f32v2& worldPos) const;
    TileHandle getTerrainTileHandleAtWorldPos(const i32v2& worldPos) const;
    f32 getTerrainHeightAtPoint(f32v2 worldPos) const;

    // Queries
    bool terrainTileHasHarvestable(const i32v2& worldPos, TileHarvestable resource, TileLayer* outLayer);
    void efficientEnumTileAABB(const i32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func);
    
    // Structures TODO: Needs to be StructureRef so it isnt destroyed
    Building* tryGetStructureAtWorldPos(TileCoord worldPos) const;

    STATIC_EVENT_LISTENER_FUNCS(StaticWorld, OnWorldBeginGameThread, WorldEventType::OnWorldBeginGameThread, World&);
    STATIC_EVENT_LISTENER_FUNCS(StaticWorld, OnWorldEndGameThread, WorldEventType::OnWorldEndGameThread, World&);
    STATIC_EVENT_LISTENER_FUNCS(StaticWorld, OnWorldEndRenderThread, WorldEventType::OnWorldEndRenderThread, World&);
    EVENT_LISTENER_FUNCS_ADAPTOR(World, OnEntityCreated, WorldEventType::OnEntityCreated, const WorldEntityEvent&);
    EVENT_LISTENER_FUNCS_ADAPTOR(World, OnEntityDestroyed, WorldEventType::OnEntityDestroyed, const WorldEntityEvent&);
    EVENT_LISTENER_FUNCS_ADAPTOR(World, OnItemProjectileLand, WorldEventType::OnItemProjectileLand, const WorldEntityEvent&);

    static World* tryGetWorld(WorldID id);
private:
    WorldID mId = 0;
    // TODO: WorldRenderStateManager?
    void updateRenderState();
    void updateEntitiesRenderState(WorldRenderState& renderState);
    void updateDebugRenderState(WorldRenderState& renderState);

    bool mDidBegin = false;
    bool mIsShuttingDown = false;

    // World info
    WorldNetMode mNetMode;
    mutable std::mutex mLoadCenterMutex;
    f32v2 mLoadCenter = f32v2(0);
    i32 mWidthTiles = 0;
    i32 mWidthDTiles = 0;
    i32 mWidthChunks = 0;
    i32 mTotalChunks = 0;
    f32v2 mDefaultPlayerSpawnUV = f32v2(0.5f);

    ui64 mWorldTimeMs = 0; // Time since the world began
    ui32 mSeed = 0;

    // Chunks
    std::unique_ptr<IChunkGrid> mChunkGrid;
    // Tile containers
    std::unique_ptr<TileContainerRepository> mTileContainerRepository;
    // Loader
    std::unique_ptr<TileContainerLoader> mTileContainerLoader;
    // Terrain
    std::shared_ptr<IHeightmapGrid> mHeightmapGrid;
    // Biome
    std::shared_ptr<BiomeGrid> mBiomeGrid;
    // Surfaces
    std::shared_ptr<TerrainSurfaceGrid> mTerrainSurfaceGrid;
    // SimTiles
    std::shared_ptr<SimChunkGrid> mSimChunkGrid;
    // Markup
    std::shared_ptr<WorldMarkupGrid> mMarkupGrid;
    // Ownership
    std::shared_ptr<OwnershipGrid> mOwnershipGrid;
    // Time of day
    std::unique_ptr<TimeOfDayManager> mTimeOfDayManager;
    // ECS
    std::unique_ptr<IFullECS> mEcs;
    // Physics
    std::unique_ptr<PhysicsWorld> mPhysWorld;
    // Structures
    std::unique_ptr<BuildingGrid> mStructureGrid;
    // Generation
    std::unique_ptr<ChunkGenerator> mChunkGenerator;
    // Combat
    std::unique_ptr<CombatContext> mCombatContext;
    // Stockpiles
    std::unique_ptr<ItemStockpileRegistry> mItemStockpileRegistry;
    // Ecosystems
    std::unique_ptr<FishEcosystem> mFishEcosystem;
    // Effects
    std::unique_ptr<IEffectContext> mEffectContext;
    // Visibility
    std::unique_ptr<VisibilityManager> mVisibilityManager;
    // Weather
    std::unique_ptr<WeatherManager> mWeatherManager;
    // Factions 
    std::unique_ptr<IFactionManager> mFactionManager;
    // Simulation (HOST ONLY)
    std::unique_ptr<HostSimContext> mHostSimContext;
    // Nav world (OPTIONAL)
    std::unique_ptr<NavWorld> mNavWorld;
    // Save
    std::unique_ptr<WorldSaveContext> mSaveContext;

    STATIC_EVENT_DISPATCHER_DEF(StaticWorld);
    EVENT_DISPATCHER_DEF(World);

    inline static std::shared_mutex sWorldsMutex;
    inline static UnorderedFlatMap<WorldID, World*> sWorlds;
};

extern std::unique_ptr<World> sGameWorld;