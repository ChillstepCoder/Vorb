#include "stdafx.h"
#include "WorldObjectQuery.h"

#include "world/IWorld.h"
#include "world/IChunkGrid.h"
#include "city/City.h"
#include "city/CityQuartermaster.h"
#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"
#include "ecs/IEntityComponentSystem.h"
#include "ecs/component/CharacterDetailsComponent.h"

#include "gamethread/GameThreadTasks.h"

WorldObjectQuery::WorldObjectQuery(IWorld& world) : mWorld(world)
{
}

WorldObjectQuery::~WorldObjectQuery()
{
}

void WorldObjectQuery::query() {
    mStockpileAtTile = nullptr;

    if (IS_GAME_THREAD()) {
        queryInternal();
    }
    else {
        mIsReady = false;
        mIsQuerying = true;
        // Copy our handle on the heap so it cannot be destroyed even if the original WorldObjectQuery is destroyed
        WorldObjectQueryPtr* threadHandle = new WorldObjectQueryPtr(this);
        GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vHandle) {
            WorldObjectQueryPtr* threadHandle = static_cast<WorldObjectQueryPtr*>(vHandle);
            (*threadHandle)->queryInternal();
            (*threadHandle)->mIsQuerying = false;
            delete threadHandle;
        }, threadHandle);
    }
}

void WorldObjectQuery::queryInternal() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();

    TileHandle handle;
    if (mLiteHandle.isValid()) {
        handle = mLiteHandle.toTileHandle(mWorld);
        mTileRef.acquire(handle);
    }
    else {
        ASSERT_GAME_THREAD();
        f32v2 tilePos2D(mWorldPos.x, mWorldPos.y);
        handle = mWorld.getTerrainTileHandleAtWorldPos(tilePos2D);
        if (!handle.isValid()) {
            return;
        }

        Chunk* chunk = handle.container->getOwnerChunk();
        if (chunk->isDataReady()) {
            assert(tilePos2D.x >= 0.0f && tilePos2D.y >= 0.0f);
            std::vector<Structure*> structures = mWorld.tryGetStructuresAtWorldPos(i32v2(tilePos2D));
            for (size_t i = 0; i < structures.size(); ++i) {
                Structure* structure = structures[i];
                TileHandle nextHandle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(mWorldPos);
                if (nextHandle.isValid() && structure->isTileOwned(nextHandle.tileIndex)) {
                    mTileRef.acquire(nextHandle);
                    break;
                }
            }
        }
        // Fallback to terrain if no structure
        if (!mTileRef.container) {
            mTileRef.acquire(handle);
        }
    }

    // TODO: Tile flag city?
    // Stockpile
    assert(handle.isValid());
    if (handle.getTile().hasFlag(TileFlags::IS_STOCKPILE)) {
        const ChunkID id = handle.getChunkIDAtPos();
        const auto* stockPiles = mWorld.getItemStockpileRegistry().tryGetStockpilesAtTileContainer(chunkGrid.getChunk(id).getTileContainer()->getId());
        if (stockPiles) {
            for (auto& stockpile : *stockPiles) {
                if (pointIsWithinAABBInclusive(mWorldPos, stockpile->getAABB())) {
                    mStockpileAtTile = stockpile;
                    break;
                }
            }
        }
    }

    mIsReady = true;
    // Entities
    // TODO: more precise
    //const f32v2 queryPos(mTilePos.x + 0.5f, mTilePos.y + 0.5f);
    //mEntitiesAtTile = mWorld.queryActorsInRadius(queryPos, 0.5f, ACTORTYPE_ANY, 0, true);
}

WorldObjectQueryPtr WorldObjectQueryFactory::makeQuery(IWorld& world, const f32v3& worldPos) {
    WorldObjectQueryPtr newQuery = std::make_shared<WorldObjectQuery>(world);
    newQuery->mWorldPos = worldPos;
    newQuery->query();
    return newQuery;
}

WorldObjectQueryPtr WorldObjectQueryFactory::makeQuery(IWorld& world, LiteTileHandle handle) {
    WorldObjectQueryPtr newQuery = std::make_shared<WorldObjectQuery>(world);
    newQuery->mLiteHandle = handle;
    newQuery->query();
    return newQuery;
}
