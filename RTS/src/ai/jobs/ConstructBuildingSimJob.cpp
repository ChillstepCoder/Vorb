#include "stdafx.h"
#include "ConstructBuildingSimJob.h"

#include "ai/tasks/ConstructBuildingSimTask.h"
#include "world/World.h"
#include "world/chunk/SimChunkGrid.h"
#include "world/simulation/host/component/SimSettlementComponents.h"

#include "building/Building.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBuildingSimJob, 256);

ConstructBuildingContext::ConstructBuildingContext(Building& building) 
    : building(building), blueprint(*building.getBlueprint()) { }

std::optional<BuildContextTargetData> ConstructBuildingContext::tryAquireTargetForItem(ItemID itemId) {
    auto&& it = itemsToTileTargets.find(itemId);
    if (it == itemsToTileTargets.end()) {
        return std::nullopt;
    }
    if (it->second.empty()) {
        return std::nullopt;
    }
    BuildContextTargetData data = it->second.back();
    it->second.pop_back();
    return data;
}

void ConstructBuildingContext::returnTargetForItem(ItemID itemId, BuildContextTargetData target) {
    itemsToTileTargets[itemId].push_back(target);
}

std::optional<BuildContextTargetData> ConstructBuildingContext::tryAquireTargetToConstruct() {
    if (tilesToConstruct.empty()) {
        return std::nullopt;
    }
    BuildContextTargetData rv = tilesToConstruct.front();
    tilesToConstruct.pop();
    return rv;
}

bool ConstructBuildingContext::shouldFlattenTile(TileIndex i) const {
    if (i >= building.getFloorStride()) {
        return false;
    }
    return tilesNeedingFlatten.getBit(i);
}

void ConstructBuildingContext::markFlattened(TileIndex i) {
    tilesNeedingFlatten.clearBit(i);
}

ConstructBuildingSimJob::ConstructBuildingSimJob(Building& building, SimECS& simEcs, entt::entity simJobOwner)
    : ISimJob(simJobOwner), mContext(building), mSimEcs(simEcs) {
    ASSERT_SIM_THREAD();
    BuildingBlueprint& blueprint = mContext.blueprint;
    assert(blueprint.itemCompositionCount);
    assert(blueprint.parentPlotID != INVALID_SETTLEMENT_PLOT_ID);
    assert(blueprint.parentSettlement != entt::null);
    assert(blueprint.worldPosRootDTile.x > -1);

    initContext();
}

std::unique_ptr<ISimTask> ConstructBuildingSimJob::tryAquireNextSubtaskForSimCharacter(World& world, entt::registry& simRegistry, entt::entity simCharacter) {
    ASSERT_SIM_THREAD();
    BuildingBlueprint& blueprint = mContext.blueprint;
    if (blueprint.totalItemsUnpromised == 0) {
        // TODO: Need to handle when BP has all items but tiles still need to be constructed
        return nullptr;
    }

    entt::entity settlementEntity = blueprint.parentSettlement;
    assert(settlementEntity != entt::null);
    SettlementHarvestableTrackerComponent& harvestTracker = simRegistry.get<SettlementHarvestableTrackerComponent>(settlementEntity);

    const TileCoord settlementCenter = simRegistry.get<SettlementSimComponent>(settlementEntity).getCenterPos(world.getWidthChunks());

    SimChunkGrid& simGrid = world.getSimChunkGrid();

    // Determine what we should harvest
    TileHarvestable harvestableToAquire = TileHarvestable::None;
    SimChunkTileReservationHandle tileReservationHandle = nullptr;
    for (int i = 0; i < blueprint.itemCompositionCount; ++i) {
        FillableSimpleItemStack& itemStack = blueprint.itemComposition[i];
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
                            harvestableToAquire = itemStack.harvestableType;
                            break;
                        }
                    }
                    // Only retry once
                    if (!tileReservationHandle && !didRetry) {
                        constexpr i32 MAX_COUNT = 64;
                        harvestables = simGrid.getClosestUnreservedHarvestablesToPoint(settlementCenter, itemStack.harvestableType, harvestTracker.currentSearchRadiusTiles, MAX_COUNT);
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

    std::unique_ptr<ConstructBuildingSimTask> newTask =
        std::make_unique<ConstructBuildingSimTask>(
            world, *this, std::move(tileReservationHandle), harvestableToAquire
        );
    // If state is END then the task could not initialize, likely due to no valid items
    if (newTask->mState == ConstructBuildingSimTask::State::End) {
        return nullptr;
    }
    return newTask;
}

std::unique_ptr<ISimTask> ConstructBuildingSimJob::tryAquireNextSubaskForFullCharacter(World& world, entt::registry& fullRegistry, entt::entity fullCharacter)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ConstructBuildingSimJob::onAbortTask(ISimTask& task) {

}

void ConstructBuildingSimJob::onCompleteTask(ISimTask& task)
{

}

void ConstructBuildingSimJob::initContext() {
    BuildingBlueprint& blueprint = mContext.blueprint;
    // Reverse order so we can pop_back efficiently without greatly changing
    // order
    // One floor at a time
    const i32 floorStride = mContext.building.getFloorStride();
    std::vector<std::vector<i32>> floorTileTargets;
    std::vector<std::vector<i32>> floorStairTargets;
    std::vector<std::vector<i32>> floorWallTargets;
    floorTileTargets.resize(blueprint.floorCount);
    floorStairTargets.resize(blueprint.floorCount);
    floorWallTargets.resize(blueprint.floorCount);
    // Help prevent allocation
    for (i32 i = 0; i < blueprint.floorCount; ++i) {
        floorTileTargets[i].reserve(floorStride);
        floorStairTargets[i].reserve(floorStride / 16);
        floorWallTargets[i].reserve(floorStride / 8);
    }
    // Sort all targets into floors
    // Tiles
    for (i32 i = 0; i < blueprint.tileTargetCount; ++i) {
        const i32 floorIndex = blueprint.tileTargets[i].tileIndex / floorStride;
        assert(floorIndex < blueprint.floorCount);
        floorTileTargets[floorIndex].push_back(i);
    }
    // Stairs
    for (i32 i = 0; i < blueprint.stairTargetCount; ++i) {
        const i32 floorIndex = blueprint.stairTargets[i].piece.pos / floorStride;
        assert(floorIndex < blueprint.floorCount);
        floorStairTargets[floorIndex].push_back(i);
    }
    // Walls
    for (i32 i = 0; i < blueprint.wallTargetCount; ++i) {
        const i32 floorIndex = blueprint.wallTargets[i].tileIndex / floorStride;
        assert(floorIndex < blueprint.floorCount);
        floorWallTargets[floorIndex].push_back(i);
    }

    // Compile all into the context in reverse order so we can start from the back
    for (i32 f = blueprint.floorCount - 1; f >= 0; --f) {
        // Tiles
        for (i32 i = floorTileTargets[f].size() - 1; i >= 0; --i) {
            i32 targetIndex = floorTileTargets[f][i];
            FillableRecipe& recipe = blueprint.tileTargets[targetIndex].fillableRecipe;
            for (i32 j = 0; j < recipe.getNumItems(); ++j) {
                i32 remaining = recipe.getRemainingQuantityAtIndex(j);
                if (remaining > 0) {
                    mContext.itemsToTileTargets[recipe.getRequiredItems()[j]].emplace_back(
                        BuildContextTargetData{
                            .targetIndex = targetIndex,
                            .type = BuildContextTargetData::Type::Tile,
                        }
                    );
                }
            }
        }
        // Stairs
        for (i32 i = floorStairTargets[f].size() - 1; i >= 0; --i) {
            i32 targetIndex = floorStairTargets[f][i];
            FillableRecipe& recipe = blueprint.stairTargets[targetIndex].fillableRecipe;
            for (i32 j = 0; j < recipe.getNumItems(); ++j) {
                i32 remaining = recipe.getRemainingQuantityAtIndex(j);
                if (remaining > 0) {
                    mContext.itemsToTileTargets[recipe.getRequiredItems()[j]].emplace_back(
                        BuildContextTargetData{
                            .targetIndex = targetIndex,
                            .type = BuildContextTargetData::Type::Stairs,
                        }
                    );
                }
            }
        }
        // Walls
        for (i32 i = floorWallTargets[f].size() - 1; i >= 0; --i) {
            i32 targetIndex = floorWallTargets[f][i];
            FillableRecipe& recipe = blueprint.wallTargets[targetIndex].fillableRecipe;
            for (i32 j = 0; j < recipe.getNumItems(); ++j) {
                i32 remaining = recipe.getRemainingQuantityAtIndex(j);
                if (remaining > 0) {
                    mContext.itemsToTileTargets[recipe.getRequiredItems()[j]].emplace_back(
                        BuildContextTargetData{
                            .targetIndex = targetIndex,
                            .type = BuildContextTargetData::Type::Wall,
                        }
                    );
                }
            }
        }
    }
    // Shrink all to fit
    for (auto& [id, targets] : mContext.itemsToTileTargets) {
        targets.shrink_to_fit();
    }
    
    // Track what to flatten
    mContext.tilesNeedingFlatten = mContext.blueprint.computeSolidTilesFirstFloor();
}
