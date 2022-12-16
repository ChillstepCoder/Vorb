#include "stdafx.h"
#include "WorldObjectQuery.h"

#include "world/IWorld.h"
#include "city/City.h"
#include "city/CityQuartermaster.h"
#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"
#include "ecs/IEntityComponentSystem.h"
#include "ecs/component/CharacterDetailsComponent.h"

#include "gamethread/GameThreadTasks.h"

WorldObjectQuery::WorldObjectQuery()
{
}

WorldObjectQuery::~WorldObjectQuery()
{

}

bool WorldObjectQuery::tryQuery(const f32v3& worldPos)
{
    if (mData) {
        if (mData->mIsQuerying) {
            return false;
        }
    }
    else {
        mData = std::make_shared<WorldObjectQueryData>();
    }
    mData->mWorldPos = worldPos;
    query();
}

void WorldObjectQuery::query() {
    mData->mStockpileAtTile = nullptr;
    mData->mBuildingAtTile = nullptr;

    if (IS_GAME_THREAD()) {
        queryInternal(*mData);
    }
    else {
        mData->mIsReady = false;
        mData->mIsQuerying = true;
        // Copy our handle on the heap so it cannot be destroyed even if the original WorldObjectQuery is destroyed
        std::shared_ptr<WorldObjectQueryData>* threadHandle = new std::shared_ptr<WorldObjectQueryData>(mData);
        GameThreadTasks::getInstance().addGenericTask([](GameThread&, void* vHandle) {
            std::shared_ptr<WorldObjectQueryData>* threadHandle = static_cast<std::shared_ptr<WorldObjectQueryData>*>(vHandle);
            WorldObjectQueryData& data = **threadHandle;
            WorldObjectQuery::queryInternal(data);
            data.mIsQuerying = false;
            delete threadHandle;
        }, threadHandle);
    }
    
}

void WorldObjectQuery::queryInternal(WorldObjectQueryData& data)
{
    assert(IS_GAME_THREAD());
    f32v2 tilePos2D(data.mWorldPos.x, data.mWorldPos.y);
    TileHandle handle = sWorld->getTerrainTileHandleAtWorldPos(tilePos2D);
    if (!handle.isValid()) {
        return;
    }

    Chunk& chunk = sWorld->getChunkAtPosition(tilePos2D);
    if (chunk.isDataReady()) {
        data.mStructures = chunk.getStructuresAt(handle.tileIndex);
        for (int i = 0; i < data.mStructures.second; ++i) {
            Structure* structure = data.mStructures.first[i];
            TileHandle nextHandle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(data.mWorldPos);
            if (nextHandle.isValid() && structure->isTileOwned(nextHandle.tileIndex)) {
                data.mSelectedStructure = structure;
                data.mTileRef.acquire(nextHandle);
                break;
            }
        }
    }
    // Fallback to terrain if no structure
    if (!data.mTileRef.container) {
        data.mTileRef.acquire(handle);
    }

    // TODO: Tile flag city?
    // Stockpile
    if (handle.tile->hasFlag(TileFlags::TILE_FLAG_IS_STOCKPILE)) {
        const ChunkID id = handle.getChunkIDAtPos();
        const auto* stockPiles = sWorld->getItemStockpileRegistry().tryGetStockpilesAtChunkPosition(id);
        if (stockPiles) {
            for (auto& stockpile : *stockPiles) {
                if (pointIsWithinAABBInclusive(data.mWorldPos, stockpile->getAABB())) {
                    data.mStockpileAtTile = stockpile;
                    break;
                }
            }
        }
    }

    data.mIsReady = true;
    // Entities
    // TODO: more precise
    //const f32v2 queryPos(mTilePos.x + 0.5f, mTilePos.y + 0.5f);
    //mEntitiesAtTile = mWorld.queryActorsInRadius(queryPos, 0.5f, ACTORTYPE_ANY, 0, true);
}
