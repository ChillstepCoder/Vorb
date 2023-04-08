#include "stdafx.h"
#include "ConstructBuildingJob.h"

#include "city/BuildingBlueprint.h"
#include "ecs/business/BusinessComponent.h"
#include "ecs/component/OwnershipComponent.h"

#include "world/IWorld.h"
#include "world/IHeightmapGrid.h"

#include "city/contracts/ContractManager.h"

//#include "item/ItemReservation.h"
#include "item/ItemStockpile.h"

#include "ai/tasks/BuildBlueprintTask.h"
#include "ai/tasks/GatherTask.h"
#include "ai/tasks/PathToTargetTask.h"

// Look for items every 8 ticks
constexpr ui32 TICK_RATE_RESERVE_ITEMS = 8;


JobRequiredItems::JobRequiredItems() {

}

JobRequiredItems::~JobRequiredItems() {

}


ConstructBuildingJob::ConstructBuildingJob(BuildingBlueprint& blueprint, entt::entity businessEntity) : mBlueprint(blueprint), IBusinessJob(businessEntity) {
    assert(!mBlueprint.isGenerating);
    // Track required items internally
    mRequiredItems.resize(mBlueprint.requiredItemsToBuild.size());
    for (size_t i = 0; i < mRequiredItems.size(); ++i) {
        JobRequiredItems& required = mRequiredItems[i];
        ItemStackUnbounded& stack = mBlueprint.requiredItemsToBuild[i];
        required.id = stack.id;
        required.quantityRequired = stack.quantity;
    }
    assert(mBlueprint.totalTilesToBuild);

    // Clamp building height to 1 meter increments
    IHeightmapGrid& grid = sWorld->getHeightmapGrid();
    mBlueprint.mDesiredTerrainFlattenHeight = round(grid.computeMeanHeightAtAABB(i32AABB2(mBlueprint.mTileSpatialGrid.getAABB()), mBlueprint.tilesNeedingTerrainFlatten)) - 0.005f;

    // Initialize building data
    mBlueprint.building->getTileContainer()->allocateOwnedTiles();
}

ConstructBuildingJob::~ConstructBuildingJob() {

}

bool ConstructBuildingJob::tick(entt::registry& registry, entt::entity business) {
    assert(mBusinessEntity == business);

    if (isDone()) {
        return true;
    }

    //BusinessComponent& businessCmp = registry.get<BusinessComponent>(business);
    OwnershipComponent& ownershipCmp = registry.get<OwnershipComponent>(business);

    // Search for items if we need them
    //if (tickCounter % TICK_RATE_RESERVE_ITEMS == 0) {
    //    for (auto&& item : mRequiredItems) {
    //        if (item.quantityReserved < item.quantityRequired) {
    //            // TODO: Re-enable
    ////            tryReserveItems(item, ownershipCmp);
    //        }
    //    }
    //}

    ++tickCounter;
    return false;
}

float ConstructBuildingJob::getProgress() const {
    return (f32)mBlueprint.tilesBuilt / (f32)mBlueprint.totalTilesToBuild;
}

IAgentTaskPtr ConstructBuildingJob::tryMakeTaskForWorker(entt::registry& registry, entt::entity worker) {

    for (auto&& item : mRequiredItems) {
        // Gather
        if (item.quantityReserved < item.quantityRequired) {
            ui32 remainingQuantity = item.quantityRequired - item.quantityReserved;
            // TODO: check worker inventory space
            const ui16 quantity = (ui16)std::min(16u, remainingQuantity);

            // Mark as reserved
            item.quantityReserved += quantity;

            ItemShipmentContract* contract = Services::ContractManager::ref().createItemShipmentContract(registry, worker, mBusinessEntity, item.id, quantity);

            // Gather
            HarvestItemsTaskPtr gatherTask = std::make_unique<HarvestItemsTask>(item.id, quantity, nullptr);

            // Ship
            constexpr f32 SHIPMENT_COMPLETE_RADIUS = 20.0f;
            TileHandle targetHandle = mBlueprint.getTileHandle(0); // TODO: BETTER
            PathToTargetTaskPtr shipTask = std::make_unique<PathToTargetTask>(mBlueprint.getTileHandle(0), SHIPMENT_COMPLETE_RADIUS, nullptr);
            
            // Build
            BuildBlueprintTaskPtr buildTask = std::make_unique<BuildBlueprintTask>(mBlueprint, nullptr);

            // Link
            shipTask->setNextTask(std::move(buildTask));
            gatherTask->setNextTask(std::move(shipTask));

            return std::move(gatherTask);
        }
    }
    // OLD
    //if (mTotalResourcesReserved && mNumTilesReservedInTasks < (mBlueprint.totalTilesToBuild - mBlueprint.tilesBuilt)) {
    //    constexpr ui32 MAX_TILES_TO_BUILD_PER_JOB = 5;
    //    const ui32 maxTilesForJob = glm::min((mBlueprint.totalTilesToBuild - mBlueprint.tilesBuilt) - mNumTilesReservedInTasks, MAX_TILES_TO_BUILD_PER_JOB);

    //    std::vector<std::unique_ptr<ItemReservation>> sourceItems;
    //    std::vector<TileIndex> targetTiles;
    //    const ui32 bpSize = mBlueprint.aabb.dims.x * mBlueprint.aabb.dims.y;
    //    for (ui32 i = mFirstUnfinishedBpIndex; i < bpSize; ++i) {
    //        BlueprintTile& tile = mBlueprint.tiles[i];
    //        if (!tile.isReserved && !tile.isBuilt) {
    //            auto&& recipe = mBlueprint.tileRecipes[e_cast(tile.type)];
    //            assert(recipe);
    //            // Check if we have enough (TODO: can we make this not n^2?)
    //            bool canFulfill = true;
    //            for (const ItemStack& itemStack : *recipe) {
    //                for (const JobRequiredItems& requiredItems : mRequiredItems) {
    //                    if (requiredItems.id == itemStack.id) {
    //                        if (requiredItems.quantityReserved < itemStack.quantity) {
    //                            canFulfill = false;
    //                            break;
    //                        }
    //                    }
    //                }
    //                if (!canFulfill) break;
    //            }

    //            if (canFulfill) {
    //                ++mNumTilesReservedInTasks;
    //                targetTiles.push_back(i);
    //                tile.isReserved = true;
    //                // Second pass. reserve the items
    //                for (const ItemStack& itemStack : *recipe) {
    //                    for (JobRequiredItems& requiredItems : mRequiredItems) {
    //                        if (requiredItems.id == itemStack.id) {
    //                            ui32 totalNeeded = itemStack.quantity;
    //                            for (size_t i = 0; i < requiredItems.mReservations.size() && totalNeeded;) {
    //                                const ui32 reservationQuantity = requiredItems.mReservations[i]->getRemainingQuantity();
    //                                if (reservationQuantity <= totalNeeded) {
    //                                    // This stack is smaller than or equal to what we need, consume the entire reservation
    //                                    totalNeeded -= reservationQuantity;
    //                                    sourceItems.push_back(std::move(requiredItems.mReservations[i]));
    //                                    requiredItems.mReservations[i] = std::move(requiredItems.mReservations.back());
    //                                    requiredItems.mReservations.pop_back();
    //                                }
    //                                else {
    //                                    // This reservation is bigger than what we need, split it and break
    //                                    sourceItems.push_back(std::move(requiredItems.mReservations[i]->splitReservation(totalNeeded)));
    //                                    totalNeeded = 0;
    //                                    break;
    //                                }
    //                            }
    //                            assert(totalNeeded == 0);
    //                            // Adjust totals
    //                            requiredItems.quantityReserved -= itemStack.quantity;
    //                            requiredItems.quantityRequired -= itemStack.quantity;
    //                        }
    //                    }
    //                    // TODO: Sort reservations by item stockpile?
    //                }
    //                // Stop if we cant do any more
    //                if (targetTiles.size() >= maxTilesForJob) break;
    //            }
    //        }
    //        else if (i == mFirstUnfinishedBpIndex) {
    //            // Increment to reduce iteration later
    //            ++mFirstUnfinishedBpIndex;
    //        }
    //    }
    //    if (targetTiles.size()) {
    //        return std::make_unique<BuildTask>(mBlueprint, std::move(sourceItems), std::move(targetTiles));
    //    }
    //}
    //return nullptr;
}

void ConstructBuildingJob::tryReserveItems(JobRequiredItems& item, OwnershipComponent& ownerCmp) {
    //ItemStack itemsRequired;
    //itemsRequired.id = item.id;
    //itemsRequired.quantity = item.quantityRequired - item.quantityReserved;
    //for (auto&& stockpile : ownerCmp.mOwnedStockpiles) {
    //    // TODO: IncreaseReservation
    //    std::unique_ptr<ItemReservation> reservation = stockpile->tryReserveItemStack(itemsRequired, 1);
    //    if (reservation) {
    //        mTotalResourcesReserved += reservation->getRemainingQuantity();
    //        item.quantityReserved += reservation->getRemainingQuantity();
    //        itemsRequired.quantity -= reservation->getRemainingQuantity();
    //        item.mReservations.push_back(std::move(reservation));

    //        if (itemsRequired.quantity == 0) {
    //            break;
    //        }
    //    }
    //}
}
