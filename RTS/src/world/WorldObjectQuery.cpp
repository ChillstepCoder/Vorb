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
    return true;
}

bool WorldObjectQuery::tryQuery(LiteTileHandle handle) {
    if (mData) {
        if (mData->mIsQuerying) {
            return false;
        }
    }
    else {
        mData = std::make_shared<WorldObjectQueryData>();
    }
    mData->mLiteHandle = handle;
    query();
    return true;
}

void WorldObjectQuery::query() {
    mData->mStockpileAtTile = nullptr;

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
    IChunkGrid& chunkGrid = sMainGameWorld->getChunkGrid();

    TileHandle handle;
    if (data.mLiteHandle.isValid()) {
        handle = data.mLiteHandle.toTileHandle(*sMainGameWorld);
        data.mTileRef.acquire(handle);
    }
    else {
        assert(IS_GAME_THREAD());
        f32v2 tilePos2D(data.mWorldPos.x, data.mWorldPos.y);
        handle = sMainGameWorld->getTerrainTileHandleAtWorldPos(tilePos2D);
        if (!handle.isValid()) {
            return;
        }

        Chunk* chunk = handle.container->getOwnerChunk();
        if (chunk->isDataReady()) {
            assert(tilePos2D.x >= 0.0f && tilePos2D.y >= 0.0f);
            std::vector<Structure*> structures = sMainGameWorld->tryGetStructuresAtWorldPos(i32v2(tilePos2D));
            for (size_t i = 0; i < structures.size(); ++i) {
                Structure* structure = structures[i];
                TileHandle nextHandle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(data.mWorldPos);
                if (nextHandle.isValid() && structure->isTileOwned(nextHandle.tileIndex)) {
                    data.mTileRef.acquire(nextHandle);
                    break;
                }
            }
        }
        // Fallback to terrain if no structure
        if (!data.mTileRef.container) {
            data.mTileRef.acquire(handle);
        }
    }

    // TODO: Tile flag city?
    // Stockpile
    assert(handle.isValid());
    if (handle.getTile().hasFlag(TileFlags::IS_STOCKPILE)) {
        const ChunkID id = handle.getChunkIDAtPos();
        const auto* stockPiles = sMainGameWorld->getItemStockpileRegistry().tryGetStockpilesAtTileContainer(chunkGrid.getChunk(id).getTileContainer()->getId());
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
