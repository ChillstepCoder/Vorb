#include "stdafx.h"
#include "WorldObjectQuery.h"

#include "World.h"
#include "city/City.h"
#include "city/CityQuartermaster.h"
#include "item/ItemStockpile.h"
#include "ecs/EntityComponentSystem.h"
#include "ecs/component/CharacterDetailsComponent.h"


WorldObjectQuery::WorldObjectQuery(World& world, f32v2& tilePos) :
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
    if (handle.tile.hasFlag(TILE_FLAG_IS_STOCKPILE)) {
        // Render city stuff such as stockpiles
        const CityGraph& cities = mWorld.getCities();
        for (auto&& city : cities.mNodes) {
            // Stockpiles
            const CityQuartermaster& quarterMaster = city->getCityQuartermaster();
            for (auto& it : quarterMaster.getStockpiles()) {
                if (pointIsWithinAABBInclusive(mTilePos, it->getAABB())) {
                    mStockpileAtTile = it.get();
                }
            }
        }
    }

    // Entities
    // TODO: more precise
    const f32v2 queryPos(mTilePos.x + 0.5f, mTilePos.y + 0.5f);
    mEntitiesAtTile = mWorld.queryActorsInRadius(queryPos, 0.5f, ACTORTYPE_ANY, 0, true);
}
