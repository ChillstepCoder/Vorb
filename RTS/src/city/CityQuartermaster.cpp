#include "stdafx.h"
#include "CityQuartermaster.h"
#include "city/City.h"
#include "World.h"

#include "item/ItemStockpile.h"
#include "item/ItemStockpileRegistry.h"

#include "city/BuildingBlueprint.h"

#include "ResourceManager.h"
#include "BuildingDescriptionRepository.h"

CityQuartermaster::CityQuartermaster(City& city) : mCity(city) {

}

CityQuartermaster::~CityQuartermaster() {

}

void CityQuartermaster::createStockpilesForBlueprint(BuildingBlueprint& bp) {

    bool ownershipMask[CHUNK_SIZE];

    const BuildingDescriptionRepository& buildingRepo = Services::ResourceManager::ref().getBuildingRepository();
    for (auto&& room : bp.rooms) {
        const RoomDef& def = buildingRepo.getRoomDefFromID(room.roomDefId);
        if (def.roomType == RoomType::STOCKPILE) {
            int index = 0;
            assert(room.aabb.dims.x < CHUNK_WIDTH && room.aabb.dims.y < CHUNK_WIDTH);
            // Create the ownership mask
            for (ui32 y = 0; y < room.aabb.dims.y; ++y) {
                const ui32 ty = room.aabb.pos.y + y - bp.aabb.pos.y;
                for (ui32 x = 0; x < room.aabb.dims.x; ++x) {
                    const ui32 tx = room.aabb.pos.x + x - bp.aabb.pos.x;
                    ownershipMask[index++] = (bp.ownerArray[ty * bp.aabb.dims.x + tx] == room.id);
                }
            }
          
            tryCreateCityStockpileAt(room.aabb, ownershipMask, bp.mOwnerEntity);
        }
    }
}

bool CityQuartermaster::tryCreateCityStockpileAt(const ui32AABB2& aabb, entt::entity ownerEntity) {

    ItemStockpile* newStockpile = mCity.mWorld.getItemStockpileRegistry().tryCreateStockpileAt(aabb, nullptr, ownerEntity);
    
    // Create new stockpile and leave unassigned (city ownership)
    if (newStockpile) {
        mAllStockpiles.emplace_back(newStockpile);
        newStockpile->onDestroy.add(makeDelegate(this, &CityQuartermaster::onStockpileDestroy));
        return true;
    }
    return false;
}

bool CityQuartermaster::tryCreateCityStockpileAt(const ui32AABB2& aabb, bool* ownershipMask, entt::entity ownerEntity) {

    ItemStockpile* newStockpile = mCity.mWorld.getItemStockpileRegistry().tryCreateStockpileAt(aabb, ownershipMask, ownerEntity);

    // Create new stockpile and leave unassigned (city ownership)
    if (newStockpile) {
        mAllStockpiles.emplace_back(newStockpile);
        newStockpile->onDestroy.add(makeDelegate(this, &CityQuartermaster::onStockpileDestroy));
        return true;
    }
    return false;
}

ItemStockpile* CityQuartermaster::tryGetClosestStockpileToPoint(const ui32v2 position) {
    // TODO: Make sure that we can filter out full stockpiles
    ItemStockpile* best = nullptr;
    f32 bestDistance2 = FLT_MAX;
    // TODO: Heuristic
    // Morton order?
    for (auto& stockpile : mAllStockpiles) {
        ui32v2 offset = stockpile->getAABB().pos - position;
        const f32 distance2 = glm::length2(f32v2(offset));
        if (distance2 < bestDistance2) {
            best = stockpile;
            bestDistance2 = distance2;
        }
    }

    return best;
}

void CityQuartermaster::onStockpileDestroy(Sender s, ItemStockpile* stockPile) {
    for (size_t i = 0; i < mAllStockpiles.size(); ++i) {
        if (mAllStockpiles[i] == stockPile) {
            mAllStockpiles[i] = mAllStockpiles.back();
            mAllStockpiles.pop_back();
        }
    }
}

