#include "stdafx.h"
#include "ConstructBuildingSimJob.h"

#include "ai/tasks/ConstructBuildingSimTask.h"
#include "world/World.h"
#include "world/chunk/SimChunkGrid.h"
#include "world/simulation/host/component/SimSettlementComponents.h"

#include "world/simulation/host/SimECS.h"

#include "building/Building.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBuildingSimJob, 256);

ConstructBuildingContext::ConstructBuildingContext(Building& building, SimECS& simEcs)
    : simEcs(simEcs), building(building), blueprint(*building.getBlueprint()) { }

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
    assert(target.isValid());
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

void ConstructBuildingContext::trackItemIfNeeded(TileItemUID itemUID, ItemID itemId, TileCoord worldPos, ui16 quantity) {
    ASSERT_SIM_THREAD();
    assert(quantity);
    for (int i = 0; i < blueprint.itemCompositionCount; ++i) {
        FillableSimpleItemStack& stack = blueprint.itemComposition[i];
        if (stack.itemId == itemId) {
            const i32 maxPromiseSize = stack.getMaxPromiseSize();
            if (maxPromiseSize > 0) {
                const i32 reserveCount = std::min((i32)quantity, maxPromiseSize);
                SimChunkGrid& chunkGrid = simEcs.getWorld().getSimChunkGrid();
                SimChunkTileItemReservationPtr reservation = chunkGrid.tryReserveItemStack(worldPos, itemUID, itemId, reserveCount);
                if (reservation) {
                    stack.promisedQuantity += reserveCount;
                    blueprint.totalItemsUnpromised -= reserveCount;
                    ReservedItems& res = mReservedItems[itemId];
                    res.reservations.emplace_back(std::move(reservation));
                    res.positions.push_back(worldPos);
                }
            }
            return;
        }
    }
}

SimChunkTileItemReservationPtr ConstructBuildingContext::tryGetClosestItemToPickup(i16 maxCount, TileCoord pos, i32 maxDistance /*= 46340*/) {
    for (auto it = mReservedItems.begin(); it != mReservedItems.end(); ++it) {
        ReservedItems& res = it->second;
        if (res.positions.empty()) {
            return nullptr;
        }
        int closestIndex = -1;
        i32 closestDistSQ = std::numeric_limits<i32>::max();
        for (int i = 0; i < (int)res.positions.size(); ++i) {
            const TileCoord offset = pos - res.positions[i];
            const i32 distSQ = offset.x * offset.x + offset.y * offset.y;
            if (distSQ < closestDistSQ) {
                closestDistSQ = distSQ;
                closestIndex = i;
            }
        }
        if (closestIndex == -1) {
            return nullptr;
        }
        // Prevent overlow in SQ
        if (maxDistance > 46340) [[unlikely]] maxDistance = 46340;
        if (closestDistSQ > SQ(maxDistance)) {
            return nullptr;
        }

        SimChunkTileItemReservationPtr& reservation = res.reservations[closestIndex];
        if (maxCount >= reservation->getReservedCount()) {
            SimChunkTileItemReservationPtr rv = std::move(reservation);
            res.reservations[closestIndex] = std::move(res.reservations.back());
            res.positions[closestIndex] = res.positions.back();
            res.reservations.pop_back();
            res.positions.pop_back();

            // Erase
            if (res.reservations.empty()) {
                mReservedItems.erase(it);
            }

            return rv;
        }
        return reservation->trySplit(maxCount);
    }
    return nullptr;
}

ConstructBuildingSimJob::ConstructBuildingSimJob(World& world, Building& building, SimECS& simEcs, entt::entity simJobOwner)
    : ISimJob(world, simJobOwner), mContext(building, simEcs) {
    ASSERT_SIM_THREAD();
    BuildingBlueprint& blueprint = mContext.blueprint;
    assert(blueprint.itemCompositionCount);
    assert(blueprint.parentPlotID != INVALID_SETTLEMENT_PLOT_ID);
    assert(blueprint.parentSettlement != entt::null);
    assert(blueprint.worldPosRootDTile.x > -1);

    initContext();
}

std::unique_ptr<ISimTask> ConstructBuildingSimJob::tryAquireNextSubtaskForSimCharacter(entt::registry& simRegistry, entt::entity simCharacter) {
    ASSERT_SIM_THREAD();
    if (isFinished()) {
        return nullptr;
    }

    std::unique_ptr<ConstructBuildingSimTask> newTask =
        std::make_unique<ConstructBuildingSimTask>(
            mWorld, *this, simRegistry, simCharacter
        );
    // If state is END then the task could not initialize
    if (newTask->mState == ConstructBuildingSimTask::State::End) {
        return nullptr;
    }
    return newTask;
}

std::unique_ptr<ISimTask> ConstructBuildingSimJob::tryAquireNextSubaskForFullCharacter(entt::registry& fullRegistry, entt::entity fullCharacter)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ConstructBuildingSimJob::onAbortTask(ISimTask& task) {
    if (isFinished()) {
        finishJob();
    }
}

void ConstructBuildingSimJob::onCompleteTask(ISimTask& task) {
    if (isFinished()) {
        finishJob();
    }
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
    }
    // Shrink all to fit
    for (auto& [id, targets] : mContext.itemsToTileTargets) {
        targets.shrink_to_fit();
    }
    
    // Track what to flatten
    mContext.tilesNeedingFlatten = mContext.blueprint.computeSolidTilesFirstFloor();
}

bool ConstructBuildingSimJob::isFinished() {
    return mContext.blueprint.totalTargetsUnbuilt == 0;
}
