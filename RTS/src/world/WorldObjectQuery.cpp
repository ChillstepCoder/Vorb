#include "stdafx.h"
#include "WorldObjectQuery.h"

#include "world/World.h"
#include "world/IChunkGrid.h"
#include "city/CityQuartermaster.h"
#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"
#include "ecs/IFullECS.h"
#include "ecs/component/CharacterDetailsComponent.h"

#include "gamethread/GameThreadTasks.h"

WorldObjectQuery::WorldObjectQuery(World& world) : mWorld(world)
{
}

WorldObjectQuery::~WorldObjectQuery()
{
}

void WorldObjectQuery::query(const WorldObjectQueryPtr& ptr) {
    mStockpileAtTile = nullptr;

    if (IS_GAME_THREAD()) {
        queryInternal();
    }
    else {
        mIsReady = false;
        mIsQuerying = true;
        // Copy our handle on the heap so it cannot be destroyed even if the original WorldObjectQuery is destroyed
        WorldObjectQueryPtr* threadHandle = new WorldObjectQueryPtr(ptr);
        GameThreadTasks::getInstance().addGenericTask([threadHandle]() {
            (*threadHandle)->queryInternal();
            (*threadHandle)->mIsQuerying = false;
            delete threadHandle;
        });
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
        if (chunk->isActivated()) {
            assert(tilePos2D.x >= 0.0f && tilePos2D.y >= 0.0f);
            if (Building* structure = mWorld.tryGetStructureAtWorldPos(TileCoord(tilePos2D))) {
                TileHandle nextHandle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(mWorldPos);
                if (nextHandle.isValid() && structure->isTileOwned(nextHandle.tileIndex)) {
                    mTileRef.acquire(nextHandle);
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
   /* if (handle.getTile().hasFlag(TileFlags::IS_STOCKPILE)) {
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
    }*/

    mIsReady = true;
    // Entities
    // TODO: more precise
    //const f32v2 queryPos(mTilePos.x + 0.5f, mTilePos.y + 0.5f);
    //mEntitiesAtTile = mWorld.queryActorsInRadius(queryPos, 0.5f, ACTORTYPE_ANY, 0, true);
}

WorldObjectQueryPtr WorldObjectQueryFactory::makeQuery(World& world, const f32v3& worldPos) {
    WorldObjectQueryPtr newQuery = std::make_shared<WorldObjectQuery>(world);
    newQuery->mWorldPos = worldPos;
    assert(worldPos.x >= 0.0f && worldPos.y >= 0.0f);
    newQuery->query(newQuery);
    return newQuery;
}

WorldObjectQueryPtr WorldObjectQueryFactory::makeQuery(World& world, LiteTileHandle handle) {
    WorldObjectQueryPtr newQuery = std::make_shared<WorldObjectQuery>(world);
    newQuery->mLiteHandle = handle;
    newQuery->query(newQuery);
    return newQuery;
}
