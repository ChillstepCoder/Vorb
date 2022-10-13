#include "stdafx.h"
#include "IWorld.h"

#include "pathfinding/NavThread.h"

#include "world/IChunkGrid.h"
#include "world/IHeightmapGrid.h"
#include "structure/Structure.h"

#include "ecs/IEntityComponentSystem.h"
#include "physics/PhysicsWorld.h"

#include "item/ItemStockpileRegistry.h"
#include "structure/StructureManager.h"

#include "options/DebugOptions.h"

#include "city/City.h"

#include <Vorb/math/VectorMath.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform.hpp>

IWorld* sWorld = nullptr;

IWorld::IWorld(IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid) : mChunkGrid(chunkGrid), mHeightmapGrid(heightmapGrid)
{
    assert(mChunkGrid);
    assert(mHeightmapGrid);

    assert(!sWorld);
    sWorld = this;

    // Cities
    mCities = std::make_unique<CityGraph>();

    // Structures
    mStructureManager = std::make_unique<StructureManager>();

    // Stockpiles
    mItemStockpileRegistry = std::make_unique<ItemStockpileRegistry>();

    // Physics
    mPhysWorld = std::make_unique<PhysicsWorld>();

}

IWorld::~IWorld()
{

}

void IWorld::tickShared(const f32v2& playerPos, f32 elapsedSec){

    mChunkGrid->tick(playerPos);

    // Update ECS
    // TODO: Move out?
    mEcs->tick();

    // Physworld will handle internal interpolation and timestep itself
    mPhysWorld->stepSimulation(elapsedSec);
}

void IWorld::setTimeOfDay(float time) {
    assert(time >= 0.0f && time <= HOURS_PER_DAY);

    // Offset debug time
    const f64 timeOffset = time - mTimeOfDay;
    sDebugOptions.mTimeOffset = timeOffset * SECONDS_PER_HOUR;

}

entt::entity IWorld::createEntity(const f32v3& pos, StrToken typeToken, bool shouldReplicate) {

    return mEcs->createEntity(pos, typeToken, shouldReplicate);
}

bool IWorld::tileHasHarvestableResource(const i32v2& worldPos, TileResource resource, TileLayer* outLayer) {
    TileHandle handle = getTerrainTileHandleAtWorldPos(worldPos);
    if (handle.isValid()) {
        return handle.tile->hasHarvestableResource(resource, outLayer);
    }
    return false;
}

//  TODO: No std function?
void IWorld::efficientEnumTileAABB(const i32AABB2& aabb, std::function<void(Chunk&, TileIndex)> func)
{
    assert(IS_GAME_THREAD());
    // TODO: handle this without asserts
    // Start at bottom left
    i32v2 worldPos;
    ui32 spanX = CHUNK_WIDTH; // Logically these initial values wont actually be used, but need to please compiler
    ui32 spanY = CHUNK_WIDTH;
    for (worldPos.y = aabb.y; worldPos.y < aabb.y + aabb.depth;) {
        for (worldPos.x = aabb.x; worldPos.x < aabb.x + aabb.depth;) {
            TileHandle cornerHandle = getTerrainTileHandleAtWorldPos(worldPos);
            assert(cornerHandle.container);
            Chunk& chunk = sWorld->getChunk(ChunkID::fromWorldI32v2(cornerHandle.getWorldPos2D()));
            ui32v3 offset = cornerHandle.getContainerOffset();
            const i32 distFromRightEdge = CHUNK_WIDTH - offset.x;
            const i32 distFromTopEdge = CHUNK_WIDTH - offset.y;
            spanX = std::min(distFromRightEdge, aabb.width);
            spanY = std::min(distFromTopEdge, aabb.depth); // TODO: Prob clever way to move this up a loop
            for (ui32 dy = 0; dy < spanY; ++dy) {
                for (ui32 dx = 0; dx < spanX; ++dx) {
                    func(chunk, chunk.getTileContainer()->getTileIndexFromXYZOffset(offset.x + dx, offset.y + dy, 0));
                }
            }
            worldPos.x += spanX;
        }
        worldPos.y += spanY;
    }
}

Chunk& IWorld::getChunkAtPosition(const f32v2& worldPos) {
    return getChunk(ChunkID(worldPos));
}

const Chunk& IWorld::getChunkAtPosition(const f32v2& worldPos) const {
    return getChunk(ChunkID(worldPos));
}

Chunk& IWorld::getChunkAtPosition(const i32v2& worldPos) {
    return getChunk(ChunkID::fromWorldI32v2(worldPos));
}

const Chunk& IWorld::getChunkAtPosition(const i32v2& worldPos) const {
    return getChunk(ChunkID::fromWorldI32v2(worldPos));
}

Chunk& IWorld::getChunkAtPosition(const ui16v2& worldPos) {
    return getChunk(ChunkID::fromWorldUI16v2(worldPos));
}

const Chunk& IWorld::getChunkAtPosition(const ui16v2& worldPos) const {
    return getChunk(ChunkID::fromWorldUI16v2(worldPos));
}

Chunk& IWorld::getChunk(ChunkID chunkId) {
    assert(chunkId.id < WorldData::WORLD_SIZE_CHUNKS);
    return mChunkGrid->getChunk(chunkId.id);
}

const Chunk& IWorld::getChunk(ChunkID chunkId) const {
    assert(chunkId.id < WorldData::WORLD_SIZE_CHUNKS);
    return mChunkGrid->getChunk(chunkId.id);
}

Chunk& IWorld::getChunk(ui32 chunkId) {
    assert(chunkId < WorldData::WORLD_SIZE_CHUNKS);
    return mChunkGrid->getChunk(chunkId);
}

const Chunk& IWorld::getChunk(ui32 chunkId) const {

    assert(chunkId < WorldData::WORLD_SIZE_CHUNKS);
    return mChunkGrid->getChunk(chunkId);
}

size_t IWorld::getNumActiveChunks() const {
    return mChunkGrid->getActiveChunks().size();
}

const std::vector<Chunk*>& IWorld::getActiveChunks() const {
    return mChunkGrid->getActiveChunks();
}

Chunk& IWorld::getChunkAtChunkCoords(const i32v2& worldPos) {
    return getChunk(ChunkID(worldPos));
}

// TODO: Remove this
#include "rendering/RenderContext.h"
#include "rendering/mesh/TerrainMeshManager.h"
void IWorld::dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius) {
    if (RenderContext::exists()) {
        RenderContext::getInstance().getTerrainMeshManager().dirtyTerrainFromBrush(pos, brushRadius);
    }

    for (Chunk* chunk : getActiveChunks()) {
        chunk->onTerrainDataChanged(pos, brushRadius);
    }
}

TileHandle IWorld::getTileHandleAtWorldPosThreadSafe(const i32v3& worldPos) const {
    assert(!IS_GAME_THREAD());
    const Chunk* chunk = &getChunkAtPosition(i32v2(worldPos));
    if (chunk->isDataReady()) {
        const ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        const ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        TileHandle baseHandle = chunk->getTileHandleAt(chunk->getTileContainer()->getTileIndexFromXYZOffset(x, y, 0));
        StructureArrayPtr structures = chunk->getStructuresAtThreadSafe(baseHandle.tileIndex);
        for (ui16 i = 0; i < structures.second; ++i) {
            Structure* structure = structures.first[i];
            const TileContainer* container = structure->getTileContainer();
            if (container) {
                TileHandle structureHandle = container->tryGetTileHandleAtWorldPos(worldPos);
                if (structureHandle.isValid() && container->isTileOwned(structureHandle.tileIndex)) {
                    return structureHandle;
                }
            }
        }
        return baseHandle;
    }
    return TileHandle();
}

TileHandle IWorld::getTileHandleAtWorldPos(const i32v3& worldPos) const {
    assert(IS_GAME_THREAD());
    const Chunk* chunk = &getChunkAtPosition(i32v2(worldPos));
    if (chunk->isDataReady()) {
        const ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        const ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        TileHandle baseHandle = chunk->getTileHandleAt(chunk->getTileContainer()->getTileIndexFromXYZOffset(x, y, 0));
        StructureArrayPtr structures = chunk->getStructuresAt(baseHandle.tileIndex);
        for (ui16 i = 0; i < structures.second; ++i) {
            Structure* structure = structures.first[i];
            const TileContainer* container = structure->getTileContainer();
            if (container) {
                TileHandle structureHandle = container->tryGetTileHandleAtWorldPos(worldPos);
                if (structureHandle.isValid() && container->isTileOwned(structureHandle.tileIndex)) {
                    return structureHandle;
                }
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
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return chunk->getTileHandleAt(chunk->getTileContainer()->getTileIndexFromXYZOffset(x, y, 0));
    }
    return TileHandle();
}

TileHandle IWorld::getTerrainTileHandleAtWorldPos(const i32v2& worldPos) const {
    TileHandle handle;
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return chunk->getTileHandleAt(chunk->getTileContainer()->getTileIndexFromXYZOffset(x, y, 0));
    }
    return TileHandle();
}

StructureArrayPtr IWorld::tryGetStructuresAtWorldPos(const i32v2& worldPos) const {
    const Chunk* chunk = &getChunkAtPosition(worldPos);
    if (chunk->isDataReady()) {
        ui32 x = (ui32)worldPos.x & (CHUNK_WIDTH - 1); // Fast modulus
        ui32 y = (ui32)worldPos.y & (CHUNK_WIDTH - 1); // Fast modulus
        return chunk->getStructuresAt(chunk->getTileContainer()->getTileIndexFromXYZOffset(x, y, 0));
    }
    return std::make_pair(nullptr, 0);
}

void IWorld::enumActiveChunks(std::function<void(const Chunk&)> func) const {
    for (auto&& chunk : getActiveChunks()) {
        func(*chunk);
    }
}

const f32v2& IWorld::getLoadCenter() const {
    return mChunkGrid->getLoadCenter();
}

void IWorld::onWorldBeginShared(const f32v2& loadCenter) {
    mChunkGrid->onWorldBegin(loadCenter);
}

void IWorld::updateTimeOfDay()
{
    const float SUNRISE_TIME = 6.0f; // 6am
    const float SUN_HEIGHT_OFFSET = 0.3f; // Smaller exponent means brighter days
    // TODO: Better time manager
    const f64 adjustedTime = /*sTotalTimeSeconds + */sDebugOptions.mTimeOffset;
    mTimeOfDay = (float)fmod(adjustedTime / (f64)SECONDS_PER_HOUR, (f64)HOURS_PER_DAY);

    const f32 sunDelta = (mTimeOfDay - SUNRISE_TIME) / 24.0f;
    const f32 sunRotate = sunDelta * M_PIF * 2.0f;
    mSunPosition = glm::rotateY(f32v3(-1.0f, 0.0f, 0.0f), sunRotate);
    mSunHeight = glm::min(mSunPosition.z + SUN_HEIGHT_OFFSET, 0.999f); // Store sun height before modification, cap at an epsilon to fix sampler issue
    mSunPosition.z += 0.2f; // Make it more up lol
    mSunPosition = glm::normalize(mSunPosition);

    mSkyRotMatrix = glm::rotate(sunRotate, f32v3(0.0f, 1.0f, 0.0f));

    // Colors
    f32v3 sunSet(1.0f, 0.5f, 0.0f);
    f32v3 sunPeak(1.0f, 1.0f, 1.0f);
    const float c = vmath::max(mSunHeight, 0.0f);
    mSunColor = f32v3(
        vmath::lerp(sunSet.r, sunPeak.r, c),
        vmath::lerp(sunSet.g, sunPeak.g, c),
        vmath::lerp(sunSet.b, sunPeak.b, c)
    );

    // TODO: Sync clients?
}

void IWorld::updateCities() {
    // TODO: Amortized
    mCities->update();
}
