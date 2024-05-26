#include "stdafx.h"
#include "ConstructBlueprintSimJob.h"

#include "ai/tasks/ConstructBlueprintSimTask.h"
#include "world/World.h"
#include "world/chunk/SimChunkGrid.h"
#include "world/simulation/host/component/SimSettlementComponents.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBlueprintSimJob, 256);

ConstructBlueprintSimJob::ConstructBlueprintSimJob(BuildingBlueprint& blueprint, SimECS& simEcs, entt::entity simJobOwner)
    : ISimJob(simJobOwner), mBlueprint(blueprint), mSimEcs(simEcs) {
    ASSERT_SIM_THREAD();
    assert(blueprint.itemCompositionCount);
    assert(blueprint.parentPlotID != INVALID_SETTLEMENT_PLOT_ID);
    assert(blueprint.parentSettlement != entt::null);
    assert(blueprint.worldPosRootDTile.x > -1);
}

std::unique_ptr<ISimTask> ConstructBlueprintSimJob::tryAquireNextSubtaskForSimCharacter(World& world, entt::registry& simRegistry, entt::entity simCharacter) {
    ASSERT_SIM_THREAD();
    if (mBlueprint.totalItemsUnfulfilled == 0) {
        // TODO: Need to handle when BP has all items but tiles still need to be constructed
        return nullptr;
    }

    std::span<SimChunkTileReservationHandle> tilesToHarvest;

    entt::entity settlementEntity = mBlueprint.parentSettlement;
    assert(settlementEntity != entt::null);
    SettlementHarvestableTrackerComponent& harvestTracker = simRegistry.get<SettlementHarvestableTrackerComponent>(settlementEntity);

    const TileCoord settlementCenter = simRegistry.get<SettlementSimComponent>(settlementEntity).getCenterPos(world.getWidthChunks());

    SimChunkGrid& simGrid = world.getSimChunkGrid();

    // Determine what we should harvest
    TileHarvestable harvestableToAquire = TileHarvestable::None;
    SimChunkTileReservationHandle tileReservationHandle = nullptr;
    for (int i = 0; i < mBlueprint.itemCompositionCount; ++i) {
        FillableSimpleItemStack& itemStack = mBlueprint.itemComposition[i];
        const i32 difference = itemStack.desiredQuantity - itemStack.filledQuantity;
        if (difference > 0) {
            if (itemStack.harvestableType != TileHarvestable::None) {
                bool didRetry = false;
                do {
                    SortedIntCoordDistanceSqMap& harvestables = harvestTracker.getLocationsForHarvestable(itemStack.harvestableType);
                    auto&& it = harvestables.begin();
                    while (it != harvestables.end()) {
                        TileCoord pos(it->second);
                        tileReservationHandle = simGrid.tryReserveHarvestableAtTilePos(pos, itemStack.harvestableType);
                        it = harvestables.erase(it);
                        if (tileReservationHandle) {
                            break;
                        }
                    }
                    // Only retry once
                    if (!tileReservationHandle && !didRetry) {
                        harvestables = simGrid.getClosestUnreservedHarvestablesToPoint(settlementCenter, itemStack.harvestableType, harvestTracker.currentSearchRadiusTiles, 128);
                        didRetry = true;
                    }
                    else {
                        break;
                    }
                } while (true);
                break;
            }
            else {
                assert(false); // Need to handle non harvestables
            }
            // Check if we managed to reserve a tile for harvest
            if (tileReservationHandle) {
                break;
            }
        }
    }

    if (!tileReservationHandle) {
        return nullptr;
    }

    std::unique_ptr<ConstructBlueprintSimTask> newTask = std::make_unique<ConstructBlueprintSimTask>(mBlueprint, *this, tilesToHarvest, tileReservationHandle);
    // If state is END then the task could not initialize, likely due to no valid items
    if (newTask->mState == ConstructBlueprintSimTask::State::End) {
        return nullptr;
    }
    return newTask;
}

std::unique_ptr<ISimTask> ConstructBlueprintSimJob::tryAquireNextSubaskForFullCharacter(World& world, entt::registry& fullRegistry, entt::entity fullCharacter)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ConstructBlueprintSimJob::onAbortTask(ISimTask& task) {

}

void ConstructBlueprintSimJob::onCompleteTask(ISimTask& task)
{

}
