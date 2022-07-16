#include "stdafx.h"
#include "WorldObjectQuery.h"

#include "World.h"
#include "city/City.h"
#include "city/CityQuartermaster.h"
#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"
#include "ecs/EntityComponentSystem.h"
#include "ecs/component/CharacterDetailsComponent.h"


WorldObjectQuery::WorldObjectQuery(World& world, const f32v3& worldPos) :
    mWorld(world),
    mWorldPos(worldPos)
{
    refresh();
}

void WorldObjectQuery::refresh() {
    mStockpileAtTile = nullptr;
    mBuildingAtTile = nullptr;

    f32v2 tilePos2D(mWorldPos.x, mWorldPos.y);
    TileHandle handle = mWorld.getTileHandleAtWorldPos(tilePos2D);
    if (!handle.isValid()) {
        return;
    }

    mTileRef.acquire(handle);
    Chunk& chunk = mWorld.getChunkAtPosition(tilePos2D);
    if (chunk.isDataReady()) {
        StructureArrayPtr structures = chunk.getStructuresAt(handle.tileIndex);
        for (int i = 0; i < structures.second; ++i) {
            Structure* structure = structures.first[i];
            TileHandle handle = structure->getTileContainer()->tryGetTileHandleAtWorldPos(mWorldPos);
            if (handle.isValid() && structure->isTileOwned(handle.tileIndex)) {
                mStructureTileRef.acquire(handle);
                break;
            }
        }
    }

    // TODO: Tile flag city?
    // Stockpile
    if (handle.tile->hasFlagMainThread(TileFlags::TILE_FLAG_IS_STOCKPILE)) {
        const ChunkID id = handle.getChunkIDAtPos();
        const auto* stockPiles = mWorld.getItemStockpileRegistry().tryGetStockpilesAtChunkPosition(id);
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
