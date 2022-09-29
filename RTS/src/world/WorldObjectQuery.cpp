#include "stdafx.h"
#include "WorldObjectQuery.h"

#include "world/IWorld.h"
#include "city/City.h"
#include "city/CityQuartermaster.h"
#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"
#include "ecs/EntityComponentSystem.h"
#include "ecs/component/CharacterDetailsComponent.h"


WorldObjectQuery::WorldObjectQuery(const f32v3& worldPos) :
    mWorldPos(worldPos)
{
    refresh();
}

void WorldObjectQuery::refresh() {
    mStockpileAtTile = nullptr;
    mBuildingAtTile = nullptr;

    f32v2 tilePos2D(mWorldPos.x, mWorldPos.y);
    TileHandle handle = sWorld->getTerrainTileHandleAtWorldPos(tilePos2D);
    if (!handle.isValid()) {
        return;
    }

    Chunk& chunk = sWorld->getChunkAtPosition(tilePos2D);
    if (chunk.isDataReady()) {
        StructureArrayPtr structures = chunk.getStructuresAt(handle.tileIndex);
        for (int i = 0; i < structures.second; ++i) {
            Structure* structure = structures.first[i];
            TileHandle nextHandle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(mWorldPos);
            if (nextHandle.isValid() && structure->isTileOwned(nextHandle.tileIndex)) {
                mSelectedStructure = structure;
                mTileRef.acquire(nextHandle);
                break;
            }
        }
    }
    // Fallback to terrain if no structure
    if (!mTileRef.container) {
        mTileRef.acquire(handle);
    }

    // TODO: Tile flag city?
    // Stockpile
    if (handle.tile->hasFlagMainThread(TileFlags::TILE_FLAG_IS_STOCKPILE)) {
        const ChunkID id = handle.getChunkIDAtPos();
        const auto* stockPiles = sWorld->getItemStockpileRegistry().tryGetStockpilesAtChunkPosition(id);
        if (stockPiles) {
            for (auto& stockpile : *stockPiles) {
                if (pointIsWithinAABBInclusive(mWorldPos, stockpile->getAABB())) {
                    mStockpileAtTile = stockpile;
                    break;
                }
            }
        }
    }

    // Entities
    // TODO: more precise
    //const f32v2 queryPos(mTilePos.x + 0.5f, mTilePos.y + 0.5f);
    //mEntitiesAtTile = mWorld.queryActorsInRadius(queryPos, 0.5f, ACTORTYPE_ANY, 0, true);
}
