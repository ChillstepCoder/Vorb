#include "stdafx.h"
#include "World.h"

#include "city/City.h"
#include "combat/CombatContext.h"
#include "effect/cli/CliEffectContext.h"
#include "effect/host/HostEffectContext.h"
#include "ecs/cli/CliEntityComponentSystem.h"
#include "ecs/srv/SrvEntityComponentSystem.h"
#include "generation/ChunkGenerator.h"
#include "item/ItemStockpileRegistry.h"
#include "options/DebugOptions.h"
#include "pathfinding/NavThread.h"
#include "pathfinding/NavWorld.h"
#include "physics/PhysicsWorld.h"
#include "rendering/RenderContext.h"
#include "rendering/RenderThreadTasks.h"
#include "resources/ResourceManager.h"
#include "structure/Structure.h"
#include "structure/StructureManager.h"
#include "tile/TileContainerRepository.h"
#include "tile/TileHandle.h"
#include "time/TimeOfDayManager.h"
#include "world/Chunk.h"
#include "world/biome/BiomeGrid.h"
#include "world/cli/CliChunkGrid.h"
#include "world/cli/CliHeightmapGrid.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "world/IChunkGrid.h"
#include "world/IHeightmapGrid.h"
#include "world/srv/SrvChunkGrid.h"
#include "world/srv/HostHeightmapGrid.h"
#include "world/ecosystem/FishEcosystem.h"
#include "world/host/HostWorldData.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/road/RoadGrid.h"
#include "serialization/gamesave/WorldSaveContext.h"
#include "weather/WeatherManager.h"
#include "faction/HostFactionManager.h"

#include "visibility/VisibilityManager.h"
#include "visibility/VisibilityThread.h"

// TODO: Move this stuff out with a separate manager class
#include "rendering/renderstate/GameRenderStateManager.h"
#include "rendering/renderdata/WorldRenderDataManager.h"
#include "rendering/mesh/TerrainMeshManager.h"
#include "rendering/mesh/GrassMeshManager.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "world/HeightmapTerrainQuadtree.h"

static WorldID sWorldId = 0;
std::unique_ptr<World> sGameWorld;

World::World(WorldNetMode netMode, HostWorldData* hostWorldData) : mNetMode(netMode) {
    PreciseTimer timer;
    LOG_DEBUG("Allocating world {} net mode {}", (void*)this, e_cast(netMode));
    mId = ++sWorldId;
    {
        std::lock_guard lock(sWorldsMutex);
        sWorlds[mId] = this;
    }

    constexpr ui32 MIN_WORLD_WIDTH_TILES = TERRAIN_QUADTREE_WIDTH;
    const ui32 worldWidthTiles = hostWorldData->worldWidth;
    assert(worldWidthTiles < MAX_WORLD_WIDTH_TILES);

    // Clamp world width to multiple of MIN_WORLD_WIDTH_TILES
    mWidthTiles = (worldWidthTiles / MIN_WORLD_WIDTH_TILES) * MIN_WORLD_WIDTH_TILES;
    if (mWidthTiles < MIN_WORLD_WIDTH_TILES) {
        mWidthTiles = MIN_WORLD_WIDTH_TILES;
    }
    mWidthChunks = mWidthTiles / CHUNK_WIDTH;

    // Host vs Client objects
    switch (netMode) {
        case WorldNetMode::Editor: {
            assert(hostWorldData);
            mHeightmapGrid = hostWorldData->heightmapGrid;
            mBiomeGrid = hostWorldData->biomeGrid;
            mMarkupGrid = hostWorldData->markupGrid;
            mOwnershipGrid = hostWorldData->ownershipGrid;
            mRoadGrid = hostWorldData->roadGrid;
            mSimTileGrid = hostWorldData->tileGrid;
            mChunkGrid = std::make_unique<SrvChunkGrid>();
            mEcs = std::make_unique<CliEntityComponentSystem>(*this);
            mEffectContext = std::make_unique<CliEffectContext>(*this);
            break;
        }
        case WorldNetMode::Client: {
            assert(!hostWorldData);
            mHeightmapGrid = std::make_unique<CliHeightmapGrid>(worldWidthTiles);
            mBiomeGrid = hostWorldData->biomeGrid;
            mMarkupGrid = hostWorldData->markupGrid;
            mOwnershipGrid = hostWorldData->ownershipGrid;
            mRoadGrid = hostWorldData->roadGrid;
            mChunkGrid = std::make_unique<CliChunkGrid>();
            mEcs = std::make_unique<CliEntityComponentSystem>(*this);
            mEffectContext = std::make_unique<CliEffectContext>(*this);
            mFactionManager = std::make_unique<CliFactionManager>(*this);
            break;
        }
        case WorldNetMode::Host: {
            assert(hostWorldData);
            mHeightmapGrid = hostWorldData->heightmapGrid;
            mBiomeGrid = hostWorldData->biomeGrid;
            mMarkupGrid = hostWorldData->markupGrid;
            mOwnershipGrid = hostWorldData->ownershipGrid;
            mRoadGrid = hostWorldData->roadGrid;
            mSimTileGrid = hostWorldData->tileGrid;
            mChunkGrid = std::make_unique<SrvChunkGrid>();
            mEcs = std::make_unique<SrvEntityComponentSystem>(*this);
            mEffectContext = std::make_unique<HostEffectContext>(*this);
            mHostSimContext = std::make_unique<HostSimContext>(*this);
            mFactionManager = std::make_unique<HostFactionManager>(*this);
            mSeed = hostWorldData->worldSeed;
            break;
        }
        default:
            panic("Invalid world net mode {}", (int)netMode);
            break;
    }
    static_assert(e_count(WorldNetMode) == 3);
    
    // Tile Containers
    mTileContainerRepository = std::make_unique<TileContainerRepository>(*this);
    // Time of day
    mTimeOfDayManager = std::make_unique<TimeOfDayManager>();
    // Cities
    mCities = std::make_unique<CityGraph>(*this);
    // Structures
    mStructureManager = std::make_unique<StructureManager>(*this);
    // Physics
    mPhysWorld = std::make_unique<PhysicsWorld>(*this, Services::ResourceManager::ref().getCollisionShapeRepository());
    // Generation
    mChunkGenerator = std::make_unique<ChunkGenerator>(*this);
    // Combat
    mCombatContext = std::make_unique<CombatContext>(*this);
    // Items
    mItemStockpileRegistry = std::make_unique<ItemStockpileRegistry>(*this);
    // Fish
    mFishEcosystem = std::make_unique<FishEcosystem>(*this);
    // Visibility
    mVisibilityManager = std::make_unique<VisibilityManager>(*this);
    // Weather
    mWeatherManager = std::make_unique<WeatherManager>(*this);
    // Save
    mSaveContext = std::make_unique<WorldSaveContext>(*this);

    LOG_DEBUG("Systems allocated in {}", timer.stop()); timer.start();

    // Initialize world data
    mChunkGrid->setWorldAndAllocateChunks(*this);
    mHeightmapGrid->setWorld(*this);

    LOG_DEBUG("Chunks allocated in {}", timer.stop());
    LOG_DEBUG("Finished allocating world {} net mode {}", (void*)this, e_cast(netMode));

    static_assert(e_count(WorldNetMode) == 3);
}

World::~World() {
    LOG_DEBUG("Deallocating world 0x%08x", (void*)this);
}

void World::onWorldBeginGame(const f32v2& loadCenter) {

    // Nav
    if (mNetMode == WorldNetMode::Host) {
        mNavWorld = std::make_unique<NavWorld>(*this);
        Services::NavThread::ref().init(*mNavWorld);
    }

    assert(!mDidBegin);
    mDidBegin = true;
    // Init chunks
    mChunkGrid->onWorldBegin(loadCenter);
    {
        std::lock_guard lock(mLoadCenterMutex);
        mLoadCenter = loadCenter;
    }

    // Register for rendering
    GameRenderStateManager::getInstance().setActiveWorld(this);

    // Notify everyone
    dispatchOnWorldBeginGameThread(*this);

    // Initialize player last
    if (!isEditorWorld()) {
        mEcs->setLocalPlayer(mEcs->createEntity(getDefaultSpawn(), CStrToken("player"), true));
    }

    if (mHostSimContext) {
        mHostSimContext->onWorldBeginGame();
    }

}

void World::tick(f32 elapsedSec) {
    PROFILE_FUNCTION();

    // Load center is player position
    entt::entity localPlayer = mEcs->getLocalPlayer();
    if (localPlayer != entt::null) {
        PhysicsComponent& physCmp = mEcs->mRegistry.get<PhysicsComponent>(localPlayer);
        const f32v3 localPlayerPos = physCmp.getPosition();
        {
            std::lock_guard lock(mLoadCenterMutex);
            mLoadCenter = localPlayerPos;
        }
    }

    // Update pending assets
    AssetLoader::getInstance().update();

    // Chunks
    mChunkGrid->tick(mLoadCenter);

    // Terrain
    mHeightmapGrid->tickShared();

    // ECS
    mEcs->tick(elapsedSec);

    // Time
    mTimeOfDayManager->updateTimeOfDay(0.0f /*TIME IS FROZEN*/);

    // Physworld will handle internal interpolation and timestep itself
    const int stepCount = mPhysWorld->stepSimulation(elapsedSec);

    // If there were any steps, simulate manual physics once with no sub stepping
    if (stepCount) {
        mEcs->tickPhysics(elapsedSec);
    }

    // Cities
    mCities->update();

    // Nav (Optional)
    if (mNavWorld) {
        Services::NavThread::ref().mainThreadUpdate();
        mNavWorld->tickGameThread();
    }

    // Visibility
   /* if (VisibilityThread::hasInstance()) {
        VisibilityThread::getInstance().mainThreadUpdate();
    }*/

    // Fish
    mFishEcosystem->tickGameThread(elapsedSec);

    // Update all dynamic tiles
    // TODO: Handle a different way?
    auto&& tileContainers = mTileContainerRepository->getTileContainers();
    for (auto&& it : tileContainers) {
        it.second->updateActiveDynamicTiles();
    }

    // Rendering
    updateRenderState();
}

void World::shutdown() {
    if (mDidBegin) {
        dispatchOnWorldEndGameThread(*this);
    }

    {
        std::lock_guard lock(sWorldsMutex);
        sWorlds.erase(mId);
    }

    mBiomeGrid.reset();
    mEcs.reset();
    mEffectContext.reset();
    mHostSimContext.reset();
    mTimeOfDayManager.reset();
    mCities.reset();
    mStructureManager.reset();
    mPhysWorld.reset();
    mChunkGenerator.reset();
    mCombatContext.reset();
    mItemStockpileRegistry.reset();
    mFishEcosystem.reset();
    mVisibilityManager.reset();
    mWeatherManager.reset();
    mNavWorld.reset();
    mChunkGrid.reset();
    mHeightmapGrid.reset();
    mTileContainerRepository.reset();

    // Render thread needs to remove resources
    if (mDidBegin) {
        while (RenderThreadTasks::getInstance().getQueuedShutdownTasksApprox()) {
            Sleep(1);
        }
    }
}


f32v3 World::getDefaultSpawn() const {
    return f32v3(mWidthTiles * mDefaultPlayerSpawnUV.x, mWidthTiles * mDefaultPlayerSpawnUV.y, 20.0f);
}

f32v2 World::getLoadCenter() const {
    if (IS_GAME_THREAD()) {
        return mLoadCenter;
    }
    std::lock_guard lock(mLoadCenterMutex);
    return mLoadCenter;
}

void World::setLoadCenter(const f32v2& loadCenter) {
    assert(IS_GAME_THREAD());
    mLoadCenter = loadCenter;
}

void World::setWorldTimeMs(ui64 newTime) {
    // Fast forward only
    assert(newTime > mWorldTimeMs);
    mWorldTimeMs = newTime;
}

TileHandle World::getTileHandleAtWorldPos(const i32v3& worldPos) const {
    ASSERT_GAME_THREAD();
    i32v2 worldPos2D = worldPos;
    const Chunk* chunk = &mChunkGrid->getChunkAtPosition(worldPos2D);
    if (chunk->isDataReady()) {
        const ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        const ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        TileHandle baseHandle = chunk->getTileHandleAt(chunk->getTileContainer()->getTileSpatialGrid().getTileIndexFromXYZOffset(x, y, 0));
        assert(worldPos2D.x >= 0.0f && worldPos2D.y >= 0.0f);
        std::vector<Structure*> structures = tryGetStructuresAtWorldPos(worldPos2D);
        for (auto&& structure : structures) {
            TileHandle structureHandle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(worldPos);
            if (structureHandle.isValid() && structure->isTileOwned(structureHandle.tileIndex)) {
                return structureHandle;
            }
        }
        return baseHandle;
    }
    return TileHandle();
}

TileHandle World::getTileHandleAtWorldPos(const f32v3& worldPos) const {
    i32v3 wpi(worldPos.x, worldPos.y, floor(worldPos.z));
    return getTileHandleAtWorldPos(wpi);
}

TileHandle World::getTerrainTileHandleAtWorldPos(const f32v2& worldPos) const {
    TileHandle handle;
    const Chunk* chunk = &mChunkGrid->getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return chunk->getTileHandleAt(chunk->getTileContainer()->getTileSpatialGrid().getTileIndexFromXYZOffset(x, y, 0));
    }
    return TileHandle();
}

TileHandle World::getTerrainTileHandleAtWorldPos(const i32v2& worldPos) const {
    TileHandle handle;
    const Chunk* chunk = &mChunkGrid->getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return chunk->getTileHandleAt(chunk->getTileContainer()->getTileSpatialGrid().getTileIndexFromXYZOffset(x, y, 0));
    }
    return TileHandle();
}

TileHandle World::getTerrainTileHandleAtWorldPos(const ui32v2& worldPos) const {
    return getTerrainTileHandleAtWorldPos(f32v2(worldPos.x, worldPos.y));
}

bool World::terrainTileHasHarvestable(const i32v2& worldPos, TileHarvestable resource, TileLayer* outLayer) {
    TileHandle handle = getTerrainTileHandleAtWorldPos(worldPos);
    if (handle.isValid()) {
        return handle.getTile().hasHarvestableResource(resource, outLayer);
    }
    return false;
}

//  TODO: No std function?
void World::efficientEnumTileAABB(const i32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func)
{
    ASSERT_GAME_THREAD();
    // TODO: handle this without asserts
    // Start at bottom left
    i32v2 worldPos;
    ui32 spanX = CHUNK_WIDTH; // Logically these initial values wont actually be used, but need to please compiler
    ui32 spanY = CHUNK_WIDTH;
    for (worldPos.y = aabb.y; worldPos.y < aabb.y + aabb.depth;) {
        for (worldPos.x = aabb.x; worldPos.x < aabb.x + aabb.depth;) {
            TileHandle cornerHandle = getTerrainTileHandleAtWorldPos(worldPos);
            assert(cornerHandle.container);
            Chunk& chunk = mChunkGrid->getChunkAtPosition(cornerHandle.getWorldPos2D());
            ui32v3 offset = cornerHandle.getContainerOffset();
            const i32 distFromRightEdge = CHUNK_WIDTH - offset.x;
            const i32 distFromTopEdge = CHUNK_WIDTH - offset.y;
            spanX = std::min(distFromRightEdge, aabb.width);
            spanY = std::min(distFromTopEdge, aabb.depth); // TODO: Prob clever way to move this up a loop
            for (ui32 dy = 0; dy < spanY; ++dy) {
                for (ui32 dx = 0; dx < spanX; ++dx) {
                    func(chunk, chunk.getTileContainer()->getTileSpatialGrid().getTileIndexFromXYZOffset(offset.x + dx, offset.y + dy, 0));
                }
            }
            worldPos.x += spanX;
        }
        worldPos.y += spanY;
    }
}

std::vector<Structure*> World::tryGetStructuresAtWorldPos(const i32v2& worldPos) const {
    return mStructureManager->tryGetStructuresAtWorldPos(worldPos);
}

World* World::tryGetWorld(WorldID id) {
    std::shared_lock lock(sWorldsMutex);
    auto&& it = sWorlds.find(id);
    if (it != sWorlds.end()) {
        return it->second;
    }
    return nullptr;
}

void World::updateRenderState() {

    if (!GameRenderStateManager::getInstance().isActiveWorld(this)) {
        return;
    }

    WorldRenderState& renderState = GameRenderStateManager::getInstance().getRenderStateForUpdate();
    // Cache things we need to update so we can keep the update section small as possible

    // TODO: If we have a camera attach component, use that
    // ...
    // Otherwise fall back to the player entity
    renderState.mIsCameraOwned = false;
    f32v3 cameraEntityPos = f32v3(0.0f);
    entt::entity playerEntity = getECS().getLocalPlayer();
    if (playerEntity != INVALID_ENTITY) {
        cameraEntityPos = getECS().mRegistry.get<PhysicsComponent>(playerEntity).getInterpolatedPosition();
        renderState.mIsCameraOwned = true;
    }

    // Acquire render state
    renderState.mWorldId = mId;
    renderState.mWorldLoadCenter = getLoadCenter();
    renderState.mCameraOwningEntityPos = cameraEntityPos;

    updateEntitiesRenderState(renderState);
    updateDebugRenderState(renderState);

    // Release render state
    GameRenderStateManager::getInstance().finishUpdating();
}

void World::updateEntitiesRenderState(WorldRenderState& renderState) {

    IEntityComponentSystem& ecs = getECS();
    entt::registry& registry = ecs.mRegistry;

    { // Characters
        auto view = registry.view<PositionComponent, CharacterControlComponent, CharacterModelComponent>();

        renderState.mCharacters.clear();
        renderState.mCharacters.reserve(view.size_hint());

        for (auto entity : view) {
            PositionComponent& posCmp = view.get<PositionComponent>(entity);
            CharacterControlComponent& controlCmp = view.get<CharacterControlComponent>(entity);
            if (!controlCmp.mFlags.isBitSet(CharacterControlComponentFlags::HIDE_MODEL)) {
                renderState.mCharacters.emplace_back(CharacterRenderState{ entity, posCmp.mPosition, controlCmp.mControllerAngle, controlCmp.mLocomotionMode });
            }
        };
    }

    { // Dynamic models
        // TODO: Allow no orientation
        auto view = registry.view<PositionComponent, DynamicModelComponent, OrientationComponent>();

        renderState.mDynamicModels.clear();
        renderState.mDynamicModels.reserve(view.size_hint());

        for (auto entity : view) {
            PositionComponent& posCmp = view.get<PositionComponent>(entity);
            DynamicModelComponent& modelCmp = view.get<DynamicModelComponent>(entity);
            OrientationComponent& orientCmp = view.get<OrientationComponent>(entity);

            renderState.mDynamicModels.emplace_back(orientCmp.mOrientation, posCmp.mPosition, modelCmp.modelId);
        };
    }
}

void World::updateDebugRenderState(WorldRenderState& renderState) {
    PROFILE_FUNCTION();

    renderState.mDebugQuads.clear();
    // Terrain debug rendering
    if (sDebugOptions.mDebugTerrainLod) {
        assert(false); // Move to render thread
        WorldRenderDataManager& manager = RenderContext::getInstance().getRenderDataManagerForWorld(*this);
        for (auto&& terrainQuadtree : manager.getTerrainMeshManager().getTerrainQuadtrees()) {
            terrainQuadtree.getDebugQuads(renderState.mDebugQuads);
        }
    }

    // Chunk debug rendering
    if (sDebugOptions.mChunkBoundaries) {
        const IChunkGrid& chunkGrid = getChunkGrid();
        const auto& loadingChunks = chunkGrid.getLoadingChunks();
        const auto& activeChunks = chunkGrid.getActiveChunks();
        const auto& destroyingChunks = chunkGrid.getDestroyingChunks();
        renderState.mDebugChunks.resize(loadingChunks.size() + activeChunks.size() + destroyingChunks.size());

        int i = 0;

        // Add loading chunks
        for (auto&& cid : loadingChunks) {
            const Chunk& chunk = chunkGrid.getChunk(cid);
            BitFlags<DebugChunkFlags> flags;
            renderState.mDebugChunks[i++] = DebugChunkRenderState{ chunk.getChunkID(), chunk.getWorldPos(), chunk.getState(), DebugChunkListIndex::LOADING, (ui8)chunk.getRefCount(), flags };
        }

        // Add active chunks
        for (auto&& cid : activeChunks) {
            const Chunk& chunk = chunkGrid.getChunk(cid);
            BitFlags<DebugChunkFlags> flags;
            renderState.mDebugChunks[i++] = DebugChunkRenderState{ chunk.getChunkID(), chunk.getWorldPos(), chunk.getState(), DebugChunkListIndex::ACTIVE, (ui8)chunk.getRefCount(), flags };
        }

        // Add destroying chunks
        for (auto&& cid : destroyingChunks) {
            const Chunk& chunk = chunkGrid.getChunk(cid);
            BitFlags<DebugChunkFlags> flags;
            renderState.mDebugChunks[i++] = DebugChunkRenderState{ chunk.getChunkID(), chunk.getWorldPos(), chunk.getState(), DebugChunkListIndex::DESTROYING, (ui8)chunk.getRefCount(), flags };
        }
    }
    else {
        renderState.mDebugChunks.clear();
    }
}
