#include "stdafx.h"
#include "IWorld.h"

#include "world/Chunk.h"
#include "pathfinding/NavThread.h"

#include "combat/CombatContext.h"

#include "world/HeightmapTerrainQuadtree.h"
#include "world/IChunkGrid.h"
#include "world/IHeightmapGrid.h"
#include "generation/IWorldGenerator.h"
#include "generation/WorldGeneratorFactory.h"
#include "structure/Structure.h"

#include "tile/TileContainerRepository.h"

#include "time/TimeOfDayManager.h"

#include "ecs/IEntityComponentSystem.h"
#include "physics/PhysicsWorld.h"

#include "item/ItemStockpileRegistry.h"
#include "structure/StructureManager.h"

#include "options/DebugOptions.h"

#include "city/City.h"

#include "resources/ResourceManager.h"

IWorld::IWorld(ui32 widthTiles, IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid, WorldGeneratorType generatorType) : mWidthTiles(widthTiles), mChunkGrid(chunkGrid), mHeightmapGrid(heightmapGrid)
{
    assert(mChunkGrid);
    assert(mHeightmapGrid);

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
    mWorldGenerator = WorldGeneratorFactory::makeWorldGenerator(generatorType, *this);

    // Combat
    mCombatContext = std::make_unique<CombatContext>(*this);

}

IWorld::~IWorld()
{

}

void IWorld::tickShared(f32 elapsedSec) {
    PROFILE_FUNCTION();
    // Load center is player position
    // TODO: Handle dedicated server differently
    // TODO: Multiplayer considerations
    entt::entity localPlayer = mEcs->getLocalPlayer();
    if (localPlayer != entt::null) {
        PhysicsComponent& physCmp = mEcs->mRegistry.get<PhysicsComponent>(localPlayer);
        const f32v3 localPlayerPos = physCmp.getPosition();
        {
            std::lock_guard lock(mLoadCenterMutex);
            mLoadCenter = localPlayerPos;
        }
    }

    mChunkGrid->tick(mLoadCenter);

    mHeightmapGrid->tickShared();

    // Update ECS
    // TODO: Move out?
    mEcs->tick(elapsedSec);

    mTimeOfDayManager->updateTimeOfDay(0.0f /*TIME IS FROZEN*/);

    // Physworld will handle internal interpolation and timestep itself
    const int stepCount = mPhysWorld->stepSimulation(elapsedSec);

    // We dont update for every step, we dont need to
    if (stepCount) {
        mEcs->tickPhysics(elapsedSec);
    }

    mCities->update();
}

entt::entity IWorld::createEntity(const f32v3& pos, StrToken typeToken, bool shouldReplicate) {
    ASSERT_GAME_THREAD();
    return mEcs->createEntity(pos, typeToken, shouldReplicate);
}

bool IWorld::terrainTileHasHarvestable(const i32v2& worldPos, TileHarvestable resource, TileLayer* outLayer) {
    TileHandle handle = getTerrainTileHandleAtWorldPos(worldPos);
    if (handle.isValid()) {
        return handle.getTile().hasHarvestableResource(resource, outLayer);
    }
    return false;
}

//  TODO: No std function?
void IWorld::efficientEnumTileAABB(const i32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func)
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

TileHandle IWorld::getTileHandleAtWorldPos(const i32v3& worldPos) const {
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

TileHandle IWorld::getTileHandleAtWorldPos(const f32v3& worldPos) const {
    i32v3 wpi(worldPos.x, worldPos.y, floor(worldPos.z));
    return getTileHandleAtWorldPos(wpi);
}

TileHandle IWorld::getTerrainTileHandleAtWorldPos(const f32v2& worldPos) const {
    TileHandle handle;
    const Chunk* chunk = &mChunkGrid->getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return chunk->getTileHandleAt(chunk->getTileContainer()->getTileSpatialGrid().getTileIndexFromXYZOffset(x, y, 0));
    }
    return TileHandle();
}

TileHandle IWorld::getTerrainTileHandleAtWorldPos(const i32v2& worldPos) const {
    TileHandle handle;
    const Chunk* chunk = &mChunkGrid->getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return chunk->getTileHandleAt(chunk->getTileContainer()->getTileSpatialGrid().getTileIndexFromXYZOffset(x, y, 0));
    }
    return TileHandle();
}

std::vector<Structure*> IWorld::tryGetStructuresAtWorldPos(const i32v2& worldPos) const {
    return mStructureManager->tryGetStructuresAtWorldPos(worldPos);
}

f32v2 IWorld::getLoadCenter() const {
    if (IS_GAME_THREAD()) {
        return mLoadCenter;
    }
    std::lock_guard lock(mLoadCenterMutex);
    return mLoadCenter;
}

void IWorld::setLoadCenter(const f32v2& loadCenter) {
    assert(IS_GAME_THREAD());
    mLoadCenter = loadCenter;
}

void IWorld::onWorldBeginShared(const f32v2& loadCenter) {

    // Init chunks
    mChunkGrid->onWorldBegin(loadCenter);
    {
        std::lock_guard lock(mLoadCenterMutex);
        mLoadCenter = loadCenter;
    }
}
