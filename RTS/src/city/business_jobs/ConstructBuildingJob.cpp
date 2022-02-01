#include "stdafx.h"
#include "ConstructBuildingJob.h"

#include "city/BuildingBlueprint.h"
#include "ecs/business/BusinessComponent.h"

#include "item/ItemStockpile.h"

ConstructBuildingJob::ConstructBuildingJob(BuildingBlueprint* blueprint) : mBlueprint(blueprint) {
    assert(!mBlueprint->isGenerating);
    const std::vector<BlueprintTile>& tiles = mBlueprint->tiles;
    // Track required items internally
    mRequiredItems.resize(mBlueprint->requiredItemsToBuild.size());
    for (size_t i = 0; i < mRequiredItems.size(); ++i) {
        JobRequiredItems& required = mRequiredItems[i];
        ItemStack& stack = mBlueprint->requiredItemsToBuild[i];
        required.id = stack.id;
        required.quantityRequired = stack.quantity;
    }

    mTileStates.resize(tiles.size());
    for (size_t i = 0; i < mTileStates.size(); ++i) {
        switch (tiles[i].type) {
            case BlueprintTileType::NONE:
                mTileStates[i] = ConstructTileState::DONE;
                break;
            case BlueprintTileType::WALL:
                mTileStates[i] = ConstructTileState::WAITING_CONSTRUCT_GROUND;
                ++mNumGroundTilesToConstruct;
                break;
            case BlueprintTileType::FLOOR_1:
                mTileStates[i] = ConstructTileState::WAITING_CONSTRUCT_GROUND;
                ++mNumGroundTilesToConstruct;
                break;
            case BlueprintTileType::DOOR:
                mTileStates[i] = ConstructTileState::WAITING_CONSTRUCT_TOP;
                ++mNumTopTilesToConstruct;
                break;
            case BlueprintTileType::TYPES:
            default:
                assert(false);
                break;
        }
    }

    mTotalTilesToConstruct = mNumGroundTilesToConstruct + mNumMidTilesToConstruct + mNumTopTilesToConstruct;
}
static_assert(enum_cast(BlueprintTileType::TYPES) == 4, "Update build logic");

ConstructBuildingJob::~ConstructBuildingJob() {

}

bool ConstructBuildingJob::tick(World& world, entt::registry& registry, entt::entity business) {

    if (isDone()) {
        return true;
    }

    // Without idle workers we cant do anything
    if (mIdleWorkers.size()) {
        BusinessComponent& businessCmp = registry.get<BusinessComponent>(business);

        // Search for items if we need them
        for (auto&& item : mRequiredItems) {
            if (item.quantityReserved < item.quantityRequired) {
                tryReserveItems(item, businessCmp);
            }
        }

        // Assign tasks to idle workers
        do {
            if (tryAssignTaskToWorker(mIdleWorkers.back())) {
                mIdleWorkers.pop_back();
            }
            else {
                // No task can be assigned so stop trying
                break;
            }
        } while (mIdleWorkers.size());
    }

    // Return true when we are done
    return false;
}

void ConstructBuildingJob::assignWorker(entt::entity worker) {
    mWorkers.push_back(worker);
    assert(mWorkers.size() <= mMaxWorkers);
}

float ConstructBuildingJob::getProgress() const {
    ui32 totalTilesConstructedThusFar = mTotalTilesToConstruct - (mNumGroundTilesToConstruct + mNumMidTilesToConstruct + mNumTopTilesToConstruct);
    if (totalTilesConstructedThusFar == 0) return 0.0f;
    return (f32)mTotalTilesToConstruct / (f32)totalTilesConstructedThusFar;
}

bool ConstructBuildingJob::tryAssignTaskToWorker(entt::entity worker) {


    return false;
}

void ConstructBuildingJob::tryReserveItems(JobRequiredItems& item, BusinessComponent& businessCmp) {
    ItemStack itemsRequired;
    itemsRequired.id = item.id;
    itemsRequired.quantity = item.quantityRequired - item.quantityReserved;
    for (auto&& stockpile : businessCmp.mOwnedStockpiles) {
        std::unique_ptr<ItemReservation> reservation = stockpile->tryReserveItemStack(itemsRequired, 1);
        if (reservation) {
            item.quantityReserved += reservation->getRemainingQuantity();
            itemsRequired.quantity -= reservation->getRemainingQuantity();
            mItemReservations.push_back(std::move(reservation));

            if (itemsRequired.quantity == 0) {
                break;
            }
        }
    }
}
