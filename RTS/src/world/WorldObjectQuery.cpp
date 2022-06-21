#include "stdafx.h"
#include "WorldObjectQuery.h"

#include "World.h"
#include "city/City.h"
#include "city/CityQuartermaster.h"
#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"
#include "ecs/EntityComponentSystem.h"
#include "ecs/component/CharacterDetailsComponent.h"


WorldObjectQuery::WorldObjectQuery(World& world, const f32v2& tilePos) :
    mWorld(world),
    mTilePos(tilePos)
{
    refresh();
}

void WorldObjectQuery::refresh() {
    mStockpileAtTile = nullptr;
    mBuildingAtTile = nullptr;

    TileHandle handle = mWorld.getTileHandleAtWorldPos(mTilePos);
    if (!handle.isValid()) {
        return;
    }

    mTileRef.acquire(handle);

    // TODO: Tile flag city?
    // Stockpile
    if (handle.tile->hasFlagMainThread(TileFlags::TILE_FLAG_IS_STOCKPILE)) {
        const ChunkID id = handle.getChunkIDAtPos();
        const auto* stockPiles = mWorld.getItemStockpileRegistry().tryGetStockpilesAtChunkPosition(id);
        if (stockPiles) {
            for (auto& stockpile : *stockPiles) {
                if (pointIsWithinAABBInclusive(mTilePos, stockpile->getAABB())) {
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
