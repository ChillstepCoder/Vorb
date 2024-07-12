#include "stdafx.h"
#include "ConstructBuildingSimTask.h"

#include "building/BuildingBlueprint.h"
#include "building/Building.h"
#include "world/World.h"

#include "ecs/component/FullEntityBindingComponent.h"
#include "ecs/component/PositionComponent.h"

#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/simulation/host/component/SimSettlementComponents.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"
#include "world/IHeightmapGrid.h"

#include "world/chunk/SimChunkGrid.h"

#include "gamethread/GameThreadTasks.h"

#include "item/ItemDef.h"
#include "resources/TileRepository.h"

#include "ai/EntityActions.h"
#include "ai/jobs/ConstructBuildingSimJob.h"

// TODO: Query real radius!
constexpr f32 TMP_TARGET_RADIUS = 3.0f;

POOLED_ALLOC_DEF_THREADSAFE(ConstructBuildingSimTask, 256);

// TODO: Do all TODO in this file

constexpr i32 TMP_CARRY_COUNT = 6;

constexpr f32 MIN_BLUEPRINT_INTERACT_RADIUS = 0.5f;


#include "debugging/DebugRenderer.h"
void debugFullEntityPosition(World& world, entt::registry& registry, entt::entity agent, color4 color, int lifetime) {
    f32v2 pos2D = registry.get<PositionComponent>(agent).mPosition;
    f32v3 pos3D(pos2D.x, pos2D.y, world.getTerrainHeightAtPoint(pos2D));
    AM::DebugRenderer::drawWireQuadThreadSafe(pos3D, f32v2(1.0f), color, lifetime);
}

void debugDrawFullEntityPathTarget(World& world, entt::registry& registry, entt::entity agent, f32v2 pathTarget, color4 color, int lifetime) {
    f32v2 pos2D = registry.get<PositionComponent>(agent).mPosition;
    f32v3 pos3D(pos2D.x, pos2D.y, world.getTerrainHeightAtPoint(pos2D));
    f32v2 pathTarget2D = pathTarget + f32v2(0.5f);
    f32v3 pathTarget3D(pathTarget2D.x, pathTarget2D.y, world.getTerrainHeightAtPoint(pathTarget2D));
    AM::DebugRenderer::drawLineBetweenPointsThreadSafe(pos3D, pathTarget3D, color, lifetime);
}

ConstructBuildingSimTask::ConstructBuildingSimTask(
    World& world, ConstructBuildingSimJob& parentJob, entt::registry& registry, entt::entity agent, bool isSim
)
    : mContext(parentJob.mContext), mParentJob(parentJob) {
    if (isSim) {
        if (!simTrySelectItemSource(world, registry, agent)) {
            mState = State::SelectToConstruct;
        }
    }
    else {
        fullTrySelectItemSource(world, registry, agent);
    }
}

ConstructBuildingSimTask::~ConstructBuildingSimTask()
{
    if (mBlueprintItemPromise) {
        mBlueprintItemPromise->cancel();
    }
}

SimTaskTickResult ConstructBuildingSimTask::tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
    ASSERT_SIM_THREAD();
    assert(mCurrentResult == SimTaskTickResult::InProgress);

    switch (mState) {
        case State::Init:
            // Pending future result that full sim requested
            if (mSimEntityOperationFuture.valid()) {
                std::future_status status = mSimEntityOperationFuture.wait_for(std::chrono::seconds(0));
                assert(status != std::future_status::deferred);
                if (status == std::future_status::ready) {
                    // Just ignore the result as we only process futures in full
                    freeHandles();
                    if (!simTrySelectItemSource(world, simRegistry, simAgent)) {
                        mState = State::SelectToConstruct;
                    }
                }
                else {
                    // Waiting
                    return SimTaskTickResult::InProgress;
                }
            }
            break;
        case State::MoveToItemStack:
            updateMoveToItemStackSim(world, simRegistry, simAgent, elapsedSec);
            break;
        case State::MoveToHarvestable:
            updateMoveToHarvestableSim(world, simRegistry, simAgent, elapsedSec);
            break;
        case State::Harvest:
            updateHarvestSim(world, simRegistry, simAgent, elapsedSec);
            break;
        case State::MoveToBlueprint:
            updateMoveToBlueprintSim(world, simRegistry, simAgent, elapsedSec);
            break;
        case State::PlaceItems:
            updatePlaceItemsSim(world, simRegistry, simAgent, elapsedSec);
            break;
        case State::SelectToConstruct:
            updateConstructSim(world, simRegistry, simAgent, elapsedSec);
            break;
    }
    static_assert(e_count(State) == 8, "Update switch statement");
    return mCurrentResult;
}

SimTaskTickResult ConstructBuildingSimTask::tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) {
    ASSERT_GAME_THREAD();
    // TODO: REMOVE 
    //return SimTaskTickResult::InProgress;

    assert(mCurrentResult == SimTaskTickResult::InProgress);
    if (mSimEntityOperationFuture.valid()) {
        if (!updateFuture(world, fullRegistry, fullAgent)) {
            return SimTaskTickResult::InProgress;
        }
    }

    switch (mState) {
        case State::Init:
            // Should be unreachable as we hit pending future result above
            assert(false);
            break;
        case State::MoveToItemStack:
            updateMoveToItemStackFull(world, fullRegistry, fullAgent, elapsedSec);
            break;
        case State::MoveToHarvestable:
            updateMoveToHarvestableFull(world, fullRegistry, fullAgent, elapsedSec);
            break;
        case State::Harvest:
            updateHarvestFull(world, fullRegistry, fullAgent, elapsedSec);
            break;
        case State::MoveToBlueprint: {
            updateMoveToBlueprintFull(world, fullRegistry, fullAgent, elapsedSec);
            break;
        }
        case State::PlaceItems:
            updatePlaceItemsFull(world, fullRegistry, fullAgent, elapsedSec);
            break;
        case State::SelectToConstruct:
            updateConstructFull(world, fullRegistry, fullAgent, elapsedSec);
            break;
    }
    static_assert(e_count(State) == 8, "Update switch statement");
    return mCurrentResult;
}

void ConstructBuildingSimTask::onTransitionToFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) {
    switch (mState) {
        case State::Init:
        case State::MoveToItemStack:
        case State::MoveToHarvestable:
        case State::Harvest:
        case State::MoveToBlueprint:
        case State::PlaceItems:
        case State::SelectToConstruct:
        case State::End:
            break;
    }
}

void ConstructBuildingSimTask::onTransitionToSim(World& world, entt::registry& simRegistry, entt::entity simAgent) {

    // Make sure we ignore any pending operation
    mSimEntityOperationFuture = {};
    mSimEntityOperationCompleteFunc = nullptr;

    mMoveSubtask.onTransitionToSim(simRegistry, simAgent);

    switch (mState) {
        case State::Init:
        case State::MoveToItemStack:
        case State::MoveToHarvestable:
        case State::Harvest:
        case State::MoveToBlueprint:
        case State::PlaceItems:
        case State::SelectToConstruct:
        case State::End:
            break;
    }
}

std::string ConstructBuildingSimTask::getDebugString() const {
    std::string output = std::string("  Task: ") + std::string(getTaskName());
    output += std::format("\n    State: {}\n", e_cast(mState));
    return output;
}

void ConstructBuildingSimTask::initItemPromise(FillableSimpleItemStack& blueprintStack, i32 count, bool shouldUpdateBPCount) {
    assert(!mBlueprintItemPromise);

    assert(blueprintStack.getMaxPromiseSize() >= 0);
    SimpleItemStack itemToFill;
    itemToFill.itemId = blueprintStack.itemId;
    itemToFill.count = count;

    assert(itemToFill.itemId != INVALID_ITEM_ID);
    ItemReservationPair reservationPair = SimpleItemReservation::createReservation(std::span<SimpleItemStack>(&itemToFill, 1));

    reservationPair.source->bindEndFunction([this](ItemReservationEndReason reason, SimpleItemReservationData& data) {
        std::span<const ItemID> desiredItems = data.getDesiredItems();
        std::span<const i32> remainingQuantities = data.getRemainingQuantities();

        // If we have any items that were not fully filled, decrement the count from
        // the tracked promised count so other workers can then try to promise it
        for (int i = 0; i < desiredItems.size(); ++i) {
            if (remainingQuantities[i] > 0) {
                for (int j = 0; j < mContext.blueprint.itemCompositionCount; ++j) {
                    FillableSimpleItemStack& itemStack = mContext.blueprint.itemComposition[j];
                    if (itemStack.itemId == desiredItems[i]) {
                        std::lock_guard lock(mContext.blueprint.mutex);
                        itemStack.promisedQuantity -= remainingQuantities[i];
                        mContext.blueprint.totalItemsUnpromised += remainingQuantities[i];
                        break;
                    }
                }
            }
        }
        mState = State::End;
    });

    // Notify blueprint when we increase our promised amount
    reservationPair.target->bindUpdateFunction([this](ItemReservationUpdateType type, ItemID id, i32 quantity) {
        if (type == ItemReservationUpdateType::PromiseIncrease) {
            // Item composition count and itemID cannot change so do not need lock
            for (i32 i = 0; i < mContext.blueprint.itemCompositionCount; ++i) {
                FillableSimpleItemStack& itemStack = mContext.blueprint.itemComposition[i];
                if (itemStack.itemId == id) {
                    std::lock_guard lock(mContext.blueprint.mutex);
                    assert(itemStack.getMaxPromiseSize() >= 0);
                    itemStack.promisedQuantity += quantity;
                    mContext.blueprint.totalItemsUnpromised -= quantity;
                    assert(itemStack.getMaxPromiseSize() >= 0);
                    break;
                }
            }
        }
        else if (type == ItemReservationUpdateType::FulfillCount) {
            // Item composition count and itemID cannot change so do not need lock
            for (i32 i = 0; i < mContext.blueprint.itemCompositionCount; ++i) {
                FillableSimpleItemStack& itemStack = mContext.blueprint.itemComposition[i];
                if (itemStack.itemId == id) {
                    std::lock_guard lock(mContext.blueprint.mutex);
                    assert(itemStack.getMaxPromiseSize() >= 0);
                    itemStack.filledQuantity += quantity;
                    assert(itemStack.promisedQuantity >= quantity);
                    itemStack.promisedQuantity -= quantity;
                    assert(mContext.blueprint.totalItemsUnfulfilled >= quantity);
                    mContext.blueprint.totalItemsUnfulfilled -= quantity;
                    break;
                }
            }
        }
    });

    // Bind handles
    mBlueprintItemPromise = std::move(reservationPair.source);
    mBlueprintItemTargetHandle = std::move(reservationPair.target);
    // Store a reference to the blueprint handle so we can remove it

    // Init promised quantity if it hasn't already been done
    if (shouldUpdateBPCount) {
        std::lock_guard lock(mContext.blueprint.mutex);
        blueprintStack.promisedQuantity += itemToFill.count;
        mContext.blueprint.totalItemsUnpromised -= itemToFill.count;
    }
    assert(mContext.blueprint.totalItemsUnpromised >= 0);
    assert(blueprintStack.getMaxPromiseSize() >= 0);
}

bool ConstructBuildingSimTask::simTrySelectItemSource(World& world, entt::registry& simRegistry, entt::entity simAgent) {
    BuildingBlueprint& blueprint = mContext.blueprint;
    entt::entity settlementEntity = blueprint.parentSettlement;
    assert(settlementEntity != entt::null);
    SettlementHarvestableTrackerComponent& harvestTracker = simRegistry.get<SettlementHarvestableTrackerComponent>(settlementEntity);

    const TileCoord settlementCenter = simRegistry.get<SettlementSimComponent>(settlementEntity).getCenterPos(world.getWidthChunks());

    SimChunkGrid& simGrid = world.getSimChunkGrid();

    // Check item pickup first
    const TileCoord position(simRegistry.get<SimPositionComponent>(simAgent).getPosition());
    if (mTileItemReservation = mContext.tryGetClosestItemToPickup(TMP_CARRY_COUNT, position)) {
        constexpr f32 SUCCESS_RADIUS = 2.0f;
        ChunkLiteTileHandle tileHandle(mTileItemReservation->getChunkID(), mTileItemReservation->getTileIndex());
        mMoveSubtask.initSim(simRegistry, simAgent, tileHandle.getWorldPosition2D(world), SUCCESS_RADIUS, TMP_TARGET_RADIUS);
        mState = State::MoveToItemStack;

        bool found = false;
        for (int i = 0; i < blueprint.itemCompositionCount; ++i) {
            if (blueprint.itemComposition[i].itemId == mTileItemReservation->getItemID()) {
                // The context already tracked and updated the count, no need to do it twice
                initItemPromise(blueprint.itemComposition[i], mTileItemReservation->getReservedCount(), false /*shouldUpdateBPCount*/);
                found = true;
                break;
            }
        }
        assert(found);

        return true;
    }

    // Determine what we should harvest
    // Send characters to harvestables if we need more items
    if (blueprint.totalItemsUnpromised != 0) {
        i32 itemCompIndex = 0;
        i32 neededCount = 0;
        for (; itemCompIndex < blueprint.itemCompositionCount; ++itemCompIndex) {
            FillableSimpleItemStack& blueprintStack = blueprint.itemComposition[itemCompIndex];
            neededCount = blueprintStack.getMaxPromiseSize();
            if (neededCount > 0) {
               
                // Check harvestable
                if (blueprintStack.harvestableType != TileHarvestable::None) {
                    mTileHarvestReservation = harvestTracker.tryReserveNearestHarvestable(
                        blueprintStack.harvestableType, settlementCenter, simGrid
                    );
                }
                else {
                    assert(false); // Need to handle non harvestables
                }
                // Check if we managed to reserve a tile for harvest
                if (mTileHarvestReservation) {
                    mHarvestableToAquire = blueprintStack.harvestableType;
                    break;
                }
            }
        }

        // Make item reservation now that we have a worthy tile to harvest
        if (mTileHarvestReservation) {
            FillableSimpleItemStack& blueprintStack = blueprint.itemComposition[itemCompIndex];

            initItemPromise(blueprintStack, glm::min(neededCount, TMP_CARRY_COUNT), true /*shouldUpdateBPCount*/);

            // TODO: Dynamic success radius based on the tile size?
            constexpr f32 SUCCESS_RADIUS = 2.0f;
            mMoveSubtask.initSim(simRegistry, simAgent, mTileHarvestReservation->getLiteTileHandle().getWorldPosition2D(world), SUCCESS_RADIUS, TMP_TARGET_RADIUS);
            mState = State::MoveToHarvestable;
            return true;
        }
    }
    return false;
}

void ConstructBuildingSimTask::fullTrySelectItemSource(World& world, entt::registry& fullRegistry, entt::entity fullAgent) {
    FullEntityBindingComponent& bindingCmp = fullRegistry.get<FullEntityBindingComponent>(fullAgent);
    
    assert(!mSimEntityOperationFuture.valid());
    TileCoord position(i32v2(fullRegistry.get<PositionComponent>(fullAgent).mPosition));

    assert(!mBlueprintItemPromise);
    // On Complete
    mSimEntityOperationCompleteFunc = [this](World& world, entt::registry& fullRegistry, entt::entity fullAgent, bool success) {
        if (success) {
            if (mTileHarvestReservation) {
                assert(!mTileItemReservation);
                // TODO: Dynamic success radius based on the tile size?
                f32v2 worldPos2D = mTileHarvestReservation->getLiteTileHandle().getWorldPosition2D(world);
                constexpr f32 SUCCESS_RADIUS = 2.0f;
                mMoveSubtask.initFull(f32v3(worldPos2D.x, worldPos2D.y, world.getTerrainHeightAtPoint(worldPos2D)), SUCCESS_RADIUS, TMP_TARGET_RADIUS);
                mState = State::MoveToHarvestable;
            }
            else {
                assert(mTileItemReservation);
                ChunkLiteTileHandle tileHandle(mTileItemReservation->getChunkID(), mTileItemReservation->getTileIndex());
                f32v2 worldPos2D = tileHandle.getWorldPosition2D(world);
                constexpr f32 SUCCESS_RADIUS = 2.0f;
                mMoveSubtask.initFull(f32v3(worldPos2D.x, worldPos2D.y, world.getTerrainHeightAtPoint(worldPos2D)), SUCCESS_RADIUS, TMP_TARGET_RADIUS);
                mState = State::MoveToItemStack;
            }
        }
        else {
            mState = State::SelectToConstruct;
        }
    };

    // Begin operation
    mSimEntityOperationFuture = world.tryGetHostSimContext()->getECS().gameThreadRequestSimEntityOperation(bindingCmp, [this, &world, position](entt::registry& simRegistry, entt::entity simAgent) {
        BuildingBlueprint& blueprint = mContext.blueprint;
        entt::entity settlementEntity = blueprint.parentSettlement;
        assert(settlementEntity != entt::null);
        SettlementHarvestableTrackerComponent& harvestTracker = simRegistry.get<SettlementHarvestableTrackerComponent>(settlementEntity);

        const TileCoord settlementCenter = simRegistry.get<SettlementSimComponent>(settlementEntity).getCenterPos(world.getWidthChunks());

        SimChunkGrid& simGrid = world.getSimChunkGrid();

        // Check item pickup first
        if (mTileItemReservation = mContext.tryGetClosestItemToPickup(TMP_CARRY_COUNT, position)) {
            constexpr f32 SUCCESS_RADIUS = 2.0f;
            bool found = false;
            for (int i = 0; i < blueprint.itemCompositionCount; ++i) {
                if (blueprint.itemComposition[i].itemId == mTileItemReservation->getItemID()) {
                    // The context already tracked and updated the count, no need to do it twice
                    initItemPromise(blueprint.itemComposition[i], mTileItemReservation->getReservedCount(), false /*shouldUpdateBPCount*/);
                    found = true;
                    break;
                }
            }
            assert(found);

            return true;
        }

        // Determine what we should harvest
        // Send characters to harvestables if we need more items
        if (blueprint.totalItemsUnpromised != 0) {
            i32 itemCompIndex = 0;
            i32 neededCount = 0;
            for (; itemCompIndex < blueprint.itemCompositionCount; ++itemCompIndex) {
                FillableSimpleItemStack& blueprintStack = blueprint.itemComposition[itemCompIndex];
                neededCount = blueprintStack.getMaxPromiseSize();
                if (neededCount > 0) {

                    // Check harvestable
                    if (blueprintStack.harvestableType != TileHarvestable::None) {
                        mTileHarvestReservation = harvestTracker.tryReserveNearestHarvestable(
                            blueprintStack.harvestableType, settlementCenter, simGrid
                        );
                    }
                    else {
                        assert(false); // Need to handle non harvestables
                    }
                    // Check if we managed to reserve a tile for harvest
                    if (mTileHarvestReservation) {
                        mHarvestableToAquire = blueprintStack.harvestableType;
                        break;
                    }
                }
            }

            // Make item reservation now that we have a worthy tile to harvest
            if (mTileHarvestReservation) {
                FillableSimpleItemStack& blueprintStack = blueprint.itemComposition[itemCompIndex];
                initItemPromise(blueprintStack, glm::min(neededCount, TMP_CARRY_COUNT), true /*shouldUpdateBPCount*/);
                return true;
            }
        }
        mState = State::SelectToConstruct;
        return false;
    });
}

void ConstructBuildingSimTask::updateMoveToItemStackSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
    if (mMoveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec) == SimTaskTickResult::Success) {

        assert(mTileItemReservation);
        const i32 pickedCount = mTileItemReservation->tryPickupSimThread(TMP_CARRY_COUNT);

        if (pickedCount > 0) {

            if (simRegistry.all_of<DualResourceBundleComponent>(simAgent)) {
                // Drop old bundle
                EntityActions::dropBundleSim(world, simRegistry, simAgent);
            }

            DualResourceBundleComponent& bundle = simRegistry.emplace<DualResourceBundleComponent>(simAgent);
            bundle.itemStack.itemId = mTileItemReservation->getItemID();
            bundle.itemStack.count = pickedCount;
            bundle.itemStack.harvestableType = mHarvestableToAquire;

            mState = State::MoveToBlueprint;
            const f32v2 targetPos = mContext.getClosestValidInteractPosition(simRegistry.get<SimPositionComponent>(simAgent).getPosition());
            mMoveSubtask.initSim(simRegistry, simAgent, targetPos, MIN_BLUEPRINT_INTERACT_RADIUS, TMP_TARGET_RADIUS);
            // Free reservation of the tile item
            mTileItemReservation.reset();
        }
        else if (!simTrySelectItemSource(world, simRegistry, simAgent)) {
            // Couldnt find the item, so lets see if we can construct
            mState = State::SelectToConstruct;
        }

    }
}

void ConstructBuildingSimTask::updateMoveToItemStackFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) {
    SimTaskTickResult moveResult = mMoveSubtask.tickFull(world, fullRegistry, fullAgent, elapsedSec);
    switch (moveResult) {
        case SimTaskTickResult::InProgress:
            break;
        case SimTaskTickResult::Success: {

            const i32 pickedCount = mTileItemReservation->tryPickupGameThread(fullAgent, TMP_CARRY_COUNT, world.getECS());
            if (pickedCount > 0) {

                if (fullRegistry.all_of<DualResourceBundleComponent>(fullAgent)) {
                    // Drop old bundle
                    EntityActions::dropBundleFull(world, fullRegistry, fullAgent);
                }

                DualResourceBundleComponent& bundle = fullRegistry.emplace<DualResourceBundleComponent>(fullAgent);
                bundle.itemStack.itemId = mTileItemReservation->getItemID();
                bundle.itemStack.count = pickedCount;
                bundle.itemStack.harvestableType = mHarvestableToAquire;

                mState = State::MoveToBlueprint;
                const f32v2 targetPos = mContext.getClosestValidInteractPosition(fullRegistry.get<PositionComponent>(fullAgent).mPosition);
                f32v3 pos3D(targetPos.x, targetPos.y, world.getTerrainHeightAtPoint(targetPos));
                mMoveSubtask.initFull(pos3D, MIN_BLUEPRINT_INTERACT_RADIUS, TMP_TARGET_RADIUS);
                //debugDrawFullEntityPathTarget(world, fullRegistry, fullAgent, targetPos, color::Yellow, 600);

                mTileItemReservation.reset();
            }
            else {
                fullTrySelectItemSource(world, fullRegistry, fullAgent);
            }

            break;
        }
        case SimTaskTickResult::Fail: {
            if (onMinorFailCheckCanRecoverFull(world, fullRegistry, fullAgent)) {
                fullTrySelectItemSource(world, fullRegistry, fullAgent);
            }
            break;
        }
        default:
            break;
    }
}

void ConstructBuildingSimTask::updateMoveToHarvestableSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
    if (mMoveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec) == SimTaskTickResult::Success) {

        // Check if harvestable still exists
        SimTileData tileData = mTileHarvestReservation->getCurrentTileDataCopy();
        if (tileData.tileId == TILE_ID_NONE ||
            TileRepository::get().getLoadedOrUnloadedAsset(tileData.tileId).harvestable != mHarvestableToAquire) {
            // TODO: Instead fall back to finding new tile
            LOG_WARN("Need to find new tile in harvest state for construct blueprint due to lost harvestable");
            cleanupSim(world, simRegistry, simAgent, SimTaskTickResult::Fail);
            return;
        }

        mState = State::Harvest;
        // TODO: Estimate duration better
        mTimer.begin(10.0f);
    }
}

void ConstructBuildingSimTask::updateMoveToHarvestableFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) {
    SimTaskTickResult moveResult = mMoveSubtask.tickFull(world, fullRegistry, fullAgent, elapsedSec);
    switch (moveResult) {
        case SimTaskTickResult::InProgress:
            break;
        case SimTaskTickResult::Success: {
            // Check if harvestable still exists
            SimTileData tileData = mTileHarvestReservation->getCurrentTileDataCopy();
            if (tileData.tileId == TILE_ID_NONE ||
                TileRepository::get().getLoadedOrUnloadedAsset(tileData.tileId).harvestable != mHarvestableToAquire) {
                // TODO: Instead fall back to finding new tile
                LOG_WARN("Need to find new tile in harvest state for construct blueprint due to lost harvestable");
                cleanupFull(world, fullRegistry, fullAgent, SimTaskTickResult::Fail);
                return;
            }

            mState = State::Harvest;
            // TODO: Real damage
            mTimer.begin(10.0f);
            break;
        }
        case SimTaskTickResult::Fail: {
            if (onMinorFailCheckCanRecoverFull(world, fullRegistry, fullAgent)) {
                fullTrySelectItemSource(world, fullRegistry, fullAgent);
            }
            break;
        }
        default:
            assert(false);
            break;
    }
}

void ConstructBuildingSimTask::updateHarvestSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
    if (mTimer.tick(elapsedSec)) {

        const TileID clearedTileID = mTileHarvestReservation->tryClearHarvestable(mHarvestableToAquire);
        if (clearedTileID == TILE_ID_NONE) {
            // TODO: Instead fall back to finding new tile
            LOG_WARN("Need to find new tile in harvest state for construct blueprint");
            cleanupSim(world, simRegistry, simAgent, SimTaskTickResult::Fail);
            return;
        }

        // TODO: Inventory Operations helper?
        constexpr i32 MAX_ROLL_RESULTS = 16;
        ItemRollTable::Result rollResults[MAX_ROLL_RESULTS];
        const i32 resultCount = TileRepository::get().getLoadedOrUnloadedAsset(clearedTileID).itemDrops.roll(std::span(rollResults, MAX_ROLL_RESULTS));
        if (!resultCount) {
            LOG_WARN("Failed item drop result on construct blueprint task");
            cleanupSim(world, simRegistry, simAgent, SimTaskTickResult::Fail);
            return;
        }

        // Figure out which of the items we desire
        i32 bundleSelect = -1;
        for (i32 i = 0; i < resultCount; ++i) {
            const ItemAssetRef itemAsset = rollResults[i].value;
            if (mBlueprintItemPromise->desiresItem(itemAsset.getAssetID())) {
                bundleSelect = i;
                break;
            }
        }

        // Equip desired item, drop the rest
        // TODO: Keep other items for ourselves?
        for (i32 i = 0; i < resultCount; ++i) {
            const ItemAssetRef itemAsset = rollResults[i].value;
            i32 quantity = rollResults[i].quantity;
            if (i == bundleSelect) {
                DualResourceBundleComponent& bundle = simRegistry.get_or_emplace<DualResourceBundleComponent>(simAgent);
                // TODO use StatsComponent?
                if (quantity > TMP_CARRY_COUNT) {
                    // Drop extra on floor
                    i32 quantityToDrop = quantity - TMP_CARRY_COUNT;
                    TileCoord worldPos(i32v2(simRegistry.get<SimPositionComponent>(simAgent).getPosition()));
                    ItemStack dropItem(itemAsset.getAssetID(), quantityToDrop);
                    TileItemUID newItemUid = world.getSimChunkGrid().tryDropItemStackOnGroundSimThread(dropItem, worldPos);
                    assert(newItemUid != INVALID_TILE_ITEM_UID);
                    mContext.trackItemIfNeeded(newItemUid, itemAsset.getAssetID(), worldPos, quantityToDrop);
                    quantity -= quantityToDrop;
                }

                if (bundle.itemStack.itemId != INVALID_ITEM_ID) {
                    LOG_WARN("Dropping old bundle");
                    EntityActions::dropBundleSim(world, simRegistry, simAgent);
                }
                bundle.itemStack.itemId = itemAsset.getAssetID();
                bundle.itemStack.count = quantity;
                bundle.itemStack.harvestableType = mHarvestableToAquire; // TODO: ensure this is correct?
            }
            else {
                // Drop unneeded stack on floor
                TileCoord worldPos(i32v2(simRegistry.get<SimPositionComponent>(simAgent).getPosition()));
                ItemStack dropItem(itemAsset.getAssetID(), quantity);
                TileItemUID newItemUid = world.getSimChunkGrid().tryDropItemStackOnGroundSimThread(dropItem, worldPos);
                assert(newItemUid != INVALID_TILE_ITEM_UID);
                mContext.trackItemIfNeeded(newItemUid, itemAsset.getAssetID(), worldPos, quantity);
            }
        }

        mState = State::MoveToBlueprint;
        const f32v2 targetPos = mContext.getClosestValidInteractPosition(simRegistry.get<SimPositionComponent>(simAgent).getPosition());
        mMoveSubtask.initSim(simRegistry, simAgent, targetPos, MIN_BLUEPRINT_INTERACT_RADIUS, TMP_TARGET_RADIUS);
    }
}

void ConstructBuildingSimTask::updateHarvestFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) {
    if (mTimer.tick(elapsedSec)) {
        const TileID clearedTileID = mTileHarvestReservation->tryClearHarvestable(mHarvestableToAquire);
        if (clearedTileID == TILE_ID_NONE) {
            // TODO: Instead fall back to finding new tile
            LOG_WARN("Need to find new tile in full harvest state for construct blueprint");
            cleanupFull(world, fullRegistry, fullAgent, SimTaskTickResult::Fail);
            return;
        }

        // TODO: Inventory Operations helper?
        constexpr i32 MAX_ROLL_RESULTS = 16;
        ItemRollTable::Result rollResults[MAX_ROLL_RESULTS];
        const i32 resultCount = TileRepository::get().getLoadedOrUnloadedAsset(clearedTileID).itemDrops.roll(std::span(rollResults, MAX_ROLL_RESULTS));
        if (!resultCount) {
            LOG_WARN("Failed item drop result on construct blueprint task");
            cleanupFull(world, fullRegistry, fullAgent, SimTaskTickResult::Fail);
            return;
        }

        // Figure out which of the items we desire
        i32 bundleSelect = -1;
        for (i32 i = 0; i < resultCount; ++i) {
            const ItemAssetRef itemAsset = rollResults[i].value;
            if (mBlueprintItemPromise->desiresItem(itemAsset.getAssetID())) {
                bundleSelect = i;
                break;
            }
        }

        // Equip desired item, drop the rest
        // TODO: Keep other items for ourselves?
        for (i32 i = 0; i < resultCount; ++i) {
            const ItemAssetRef itemAsset = rollResults[i].value;
            i32 quantity = rollResults[i].quantity;
            if (i == bundleSelect) {
                DualResourceBundleComponent& bundle = fullRegistry.get_or_emplace<DualResourceBundleComponent>(fullAgent);
                // TODO use StatsComponent?
                if (quantity > TMP_CARRY_COUNT) {
                    // Drop extra on floor
                    i32 quantityToDrop = quantity - TMP_CARRY_COUNT;
                    const f32v3 characterPos = fullRegistry.get<PositionComponent>(fullAgent).mPosition;
                    const TileCoord tileCoord(characterPos);
                    ItemStack dropItem(itemAsset.getAssetID(), quantityToDrop);

                    TileItemUID newItemUid = world.getSimChunkGrid().tryDropItemStackOnGroundGameThread(dropItem, characterPos);
                    assert(newItemUid != INVALID_TILE_ITEM_UID);
                    mContext.trackItemIfNeeded(newItemUid, itemAsset.getAssetID(), tileCoord, quantityToDrop);
                    quantity -= quantityToDrop;
                }

                if (bundle.itemStack.itemId != INVALID_ITEM_ID) {
                    LOG_WARN("Dropping old bundle");
                    EntityActions::dropBundleFull(world, fullRegistry, fullAgent);
                }
                bundle.itemStack.itemId = itemAsset.getAssetID();
                bundle.itemStack.count = quantity;
                bundle.itemStack.harvestableType = mHarvestableToAquire; // TODO: ensure this is correct?
            }
            else {
                // Drop unneeded stack on floor
                const f32v3 characterPos = fullRegistry.get<PositionComponent>(fullAgent).mPosition;
                const TileCoord tileCoord(characterPos);
                ItemStack dropItem(itemAsset.getAssetID(), quantity);
                TileItemUID newItemUid = world.getSimChunkGrid().tryDropItemStackOnGroundGameThread(dropItem, characterPos);
                assert(newItemUid != INVALID_TILE_ITEM_UID);
                mContext.trackItemIfNeeded(newItemUid, itemAsset.getAssetID(), tileCoord, quantity);
            }
        }

        mState = State::MoveToBlueprint;
        const f32v2 targetPos = mContext.getClosestValidInteractPosition(fullRegistry.get<PositionComponent>(fullAgent).mPosition);
        f32v3 pos3D(targetPos.x, targetPos.y, world.getTerrainHeightAtPoint(targetPos));
        mMoveSubtask.initFull(pos3D, 8.0f, TMP_TARGET_RADIUS);
    }
}

void ConstructBuildingSimTask::updateMoveToBlueprintSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
    if (mMoveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec) == SimTaskTickResult::Success) {

        if (DualResourceBundleComponent* bundle = simRegistry.try_get<DualResourceBundleComponent>(simAgent)) {
            SimpleItemStack& bundleItem = bundle->itemStack;

            // We have extra items, let the BP know we intend to use them!
            i32 existingPromiseSize = mBlueprintItemPromise->getRemainingQuantity(bundleItem.itemId);
            const i32 countDiff = bundleItem.count - existingPromiseSize;
            if (countDiff > 0) {
                const i32 promiseIncrease = glm::min(countDiff, mContext.blueprint.getMaxPromiseSize(bundleItem.itemId));
                if (!mBlueprintItemPromise->tryIncreasePromisedQuantity(bundleItem.itemId, promiseIncrease)) {
                    LOG_WARN("Already fulfilled our promise in MoveToBlueprint");
                    cleanupSim(world, simRegistry, simAgent, SimTaskTickResult::Fail);
                    return;
                }
            }
            mState = State::PlaceItems;
        }
        else {
            // Lost our bundle somehow...
            LOG_WARN("Lost bundle in construct blueprint task");
            cleanupSim(world, simRegistry, simAgent, SimTaskTickResult::Fail);
            return;
        }
    }
}

void ConstructBuildingSimTask::updateMoveToBlueprintFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) {
    SimTaskTickResult moveResult = mMoveSubtask.tickFull(world, fullRegistry, fullAgent, elapsedSec);
    switch (moveResult) {
        case SimTaskTickResult::InProgress:
            break;
        case SimTaskTickResult::Success: {
            debugFullEntityPosition(world, fullRegistry, fullAgent, color::Pink, 600);
            if (DualResourceBundleComponent* bundle = fullRegistry.try_get<DualResourceBundleComponent>(fullAgent)) {
                SimpleItemStack& bundleItem = bundle->itemStack;

                // We have extra items, let the BP know we intend to use them!
                i32 existingPromiseSize = mBlueprintItemPromise->getRemainingQuantity(bundleItem.itemId);
                const i32 countDiff = bundleItem.count - existingPromiseSize;
                if (countDiff > 0) {
                    const i32 promiseIncrease = glm::min(countDiff, mContext.blueprint.getMaxPromiseSize(bundleItem.itemId));
                    if (!mBlueprintItemPromise->tryIncreasePromisedQuantity(bundleItem.itemId, promiseIncrease)) {
                        LOG_WARN("Already fulfilled our promise in MoveToBlueprint");
                        cleanupFull(world, fullRegistry, fullAgent, SimTaskTickResult::Fail);
                        return;
                    }
                }
                mState = State::PlaceItems;
            }
            else {
                // Lost our bundle somehow...
                LOG_WARN("Lost bundle in construct blueprint task");
                cleanupFull(world, fullRegistry, fullAgent, SimTaskTickResult::Fail);
                return;
            }
            break;
        }
        case SimTaskTickResult::Fail: {
            if (onMinorFailCheckCanRecoverFull(world, fullRegistry, fullAgent)) {
                fullTrySelectItemSource(world, fullRegistry, fullAgent);
            }
            break;
        }
        default:
            break;
    }
}

void ConstructBuildingSimTask::updatePlaceItemsSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {

    DualResourceBundleComponent* bundle = simRegistry.try_get<DualResourceBundleComponent>(simAgent);
    if (!bundle) [[unlikely]] {
        LOG_WARN("Lost bundle in construct blueprint task place items somehow");
        cleanupSim(world, simRegistry, simAgent, SimTaskTickResult::Fail);
        return;
    }
    
    // Put in all items at once
    do {
        SimpleItemStack& bundleItem = bundle->itemStack;
        std::optional<BuildContextTargetData> targetData = mContext.tryAquireTargetForItem(bundleItem.itemId);
        if (targetData) {
            BuildContextTargetData& data = *targetData;
            SimpleItemStack& bundleItem = bundle->itemStack;

            FillableRecipe& recipe = mContext.blueprint.getRecipeForTargetData(data);
            const i32 remainder = recipe.fillItemAndReturnRemainder(bundleItem.itemId, bundleItem.count);
            const i32 filledQuantity = bundleItem.count - remainder;
            if (!mBlueprintItemPromise->tryFulfillQuantity(bundleItem.itemId, filledQuantity)) {
                LOG_WARN("Already fulfilled our promise in PlaceItems");
                cleanupSim(world, simRegistry, simAgent, SimTaskTickResult::Fail);
                return;
            }
            bundleItem.count = remainder;

            if (recipe.isFullyFilled()) {
                // Will be pulled during construction
                mContext.returnTargetToConstruct(data);
            }
            else {
                mContext.returnTargetForItem(bundleItem.itemId, data);
            }

            if (bundleItem.count == 0) {
                simRegistry.remove<DualResourceBundleComponent>(simAgent);
                mState = State::SelectToConstruct;
                return;
            }
        }
        else {
            // No valid target to build, drop bundle and fall back to construction
            EntityActions::dropBundleSim(world, simRegistry, simAgent);
            mState = State::SelectToConstruct;
            return;
        }

    } while (true);
}

void ConstructBuildingSimTask::updatePlaceItemsFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) {
    DualResourceBundleComponent* bundle = fullRegistry.try_get<DualResourceBundleComponent>(fullAgent);
    if (!bundle) [[unlikely]] {
        LOG_WARN("Lost bundle in construct blueprint task place items somehow");
        cleanupFull(world, fullRegistry, fullAgent, SimTaskTickResult::Fail);
        return;
    }

    // Put in all items at once
    do {
        SimpleItemStack& bundleItem = bundle->itemStack;
        std::optional<BuildContextTargetData> targetData = mContext.tryAquireTargetForItem(bundleItem.itemId);
        if (targetData) {
            BuildContextTargetData& data = *targetData;
            SimpleItemStack& bundleItem = bundle->itemStack;

            FillableRecipe& recipe = mContext.blueprint.getRecipeForTargetData(data);
            const i32 remainder = recipe.fillItemAndReturnRemainder(bundleItem.itemId, bundleItem.count);
            const i32 filledQuantity = bundleItem.count - remainder;
            if (!mBlueprintItemPromise->tryFulfillQuantity(bundleItem.itemId, filledQuantity)) {
                LOG_WARN("Already fulfilled our promise in PlaceItems");
                cleanupSim(world, fullRegistry, fullAgent, SimTaskTickResult::Fail);
                return;
            }
            bundleItem.count = remainder;

            if (recipe.isFullyFilled()) {
                // Will be pulled during construction
                mContext.returnTargetToConstruct(data);
            }
            else {
                mContext.returnTargetForItem(bundleItem.itemId, data);
            }

            if (bundleItem.count == 0) {
                fullRegistry.remove<DualResourceBundleComponent>(fullAgent);
                mState = State::SelectToConstruct;
                return;
            }
        }
        else {
            // No valid target to build, drop bundle and fall back to construction
            EntityActions::dropBundleSim(world, fullRegistry, fullAgent);
            mState = State::SelectToConstruct;
            return;
        }

    } while (true);
}

void ConstructBuildingSimTask::updateConstructSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
    while (true) {
        std::optional<BuildContextTargetData> targetData = mContext.tryAquireTargetToConstruct();
        if (targetData) {
            assert(targetData->isValid());
            BuildContextTargetData& data = *targetData;

            TileIndex tileIndex;
            assert(data.isValid());
            switch (data.type) {
                case BuildContextTargetData::Type::Tile:
                    tileIndex = mContext.blueprint.tileTargets[data.targetIndex].tileIndex;
                    mContext.blueprint.tileTargets[data.targetIndex].fillableRecipe.setConstructed(true);
                    break;
                case BuildContextTargetData::Type::Wall:
                    tileIndex = mContext.blueprint.wallTargets[data.targetIndex].tileIndex;
                    mContext.blueprint.wallTargets[data.targetIndex].fillableRecipe.setConstructed(true);
                    break;
                case BuildContextTargetData::Type::Stairs:
                    tileIndex = mContext.blueprint.stairTargets[data.targetIndex].piece.pos;
                    mContext.blueprint.stairTargets[data.targetIndex].fillableRecipe.setConstructed(true);
                    break;
                default:
                    assert(false);
                    break;
            }
            static_assert(e_count(BuildContextTargetData::Type) == 3, "Update switch statement");

            // TODO: Flatten should be done in the beginning of construction, but also need to account player fucking with things
            // Perhaps the building remembers its vertices and their desired heights, and periodically the owner "checks integrity" of home, and
            // makes any repairs, including raising terrain. "CheckIntegrity" as a concept can be applied to lots of things, and can be run when
            // paths fail, for example
            if (mContext.shouldFlattenTile(tileIndex)) {
                const i32v2 bpWorldPos = mContext.blueprint.worldPosRootDTile.toTilePos();
                const i32AABB3& tileAABB = mContext.building.getTileAABB();
                const i32v2 tileWorldPos = bpWorldPos + i32v2(tileIndex % tileAABB.dims.x, tileIndex / tileAABB.dims.x);
                GameThreadTasks::getInstance().addGenericTask([zpos = tileAABB.z, tileWorldPos, &world]() {
                    world.getHeightmapGrid().setHeightAtWorldPos(tileWorldPos, zpos);
                });
                // TODO: Mark neighbor tiles as flattened too since this is an adjacent DTile flatten
                mContext.markFlattened(tileIndex);
            }

            --mContext.blueprint.totalTargetsUnbuilt; // Atomic
        }
        else {
            // Nothing to do anymore in this task cycle
            cleanupSim(world, simRegistry, simAgent, SimTaskTickResult::Success);
            return;
        }
    };
}

void ConstructBuildingSimTask::updateConstructFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) {
    std::optional<BuildContextTargetData> targetData = mContext.tryAquireTargetToConstruct();
    if (targetData) {
        assert(targetData->isValid());
        BuildContextTargetData& data = *targetData;

        // TODO: Need to handle building at the edge of the loaded grid, where a sim building exists
        if (!mContext.building.getTileContainer()) {
            return;
        }

        TileContainer& container = *mContext.building.getTileContainer();

        TileIndex tileIndex;
        assert(data.isValid());
        switch (data.type) {
            case BuildContextTargetData::Type::Tile: {
                BuildingBlueprintTileTarget& target = mContext.blueprint.tileTargets[data.targetIndex];
                tileIndex = target.tileIndex;
                target.fillableRecipe.setConstructed(true);
                container.setTileLayer(tileIndex, TileLayer::Main, target.id, 0);
                break;
            }
            case BuildContextTargetData::Type::Wall: {
                BuildingBlueprintWallTarget& target = mContext.blueprint.wallTargets[data.targetIndex];
                tileIndex = target.tileIndex;
                target.fillableRecipe.setConstructed(true);
                container.setTileLayer(tileIndex, TileLayer::Main, target.id, 0);
                break;
            }
            case BuildContextTargetData::Type::Stairs: {
                StairTileTarget& target = mContext.blueprint.stairTargets[data.targetIndex];
                tileIndex = target.piece.pos;
                target.fillableRecipe.setConstructed(true);
                container.setTileLayer(tileIndex, TileLayer::Main, target.piece.isFlatPart ? mContext.blueprint.stairsFlatTileID : mContext.blueprint.stairsTileID, 0);
                const f32 height = target.piece.height * STAIR_TILE_HEIGHT;
                // TODO: TileContainerLoader also uses floor true Z pos???
                container.setTileGroundZPosition(tileIndex, height);
                break;
            }
            default:
                assert(false);
                break;
        }
        static_assert(e_count(BuildContextTargetData::Type) == 3, "Update switch statement");

        // TODO: Flatten should be done in the beginning of construction, but also need to account player fucking with things
        // Perhaps the building remembers its vertices and their desired heights, and periodically the owner "checks integrity" of home, and
        // makes any repairs, including raising terrain. "CheckIntegrity" as a concept can be applied to lots of things, and can be run when
        // paths fail, for example
        if (mContext.shouldFlattenTile(tileIndex)) {
            const i32v2 bpWorldPos = mContext.blueprint.worldPosRootDTile.toTilePos();
            const i32AABB3& tileAABB = mContext.building.getTileAABB();
            const i32v2 tileWorldPos = bpWorldPos + i32v2(tileIndex % tileAABB.dims.x, tileIndex / tileAABB.dims.x);
            world.getHeightmapGrid().setHeightAtWorldPos(tileWorldPos, tileAABB.z);
            // TODO: Mark neighbor tiles as flattened too since this is an adjacent DTile flatten
            mContext.markFlattened(tileIndex);
        }

        --mContext.blueprint.totalTargetsUnbuilt; // Atomic
    }
    else {
        // Nothing to do anymore in this task cycle
        cleanupFull(world, fullRegistry, fullAgent, SimTaskTickResult::Success);
        return;
    }
}

bool ConstructBuildingSimTask::onMinorFailCheckCanRecoverSim(World& world, entt::registry& simRegistry, entt::entity simAgent) {
    ASSERT_SIM_THREAD();
    if (mRetryCountRemaining == 0) {
        cleanupSim(world, simRegistry, simAgent, SimTaskTickResult::Fail);
        return false;
    }
    else {
        EntityActions::dropBundleSim(world, simRegistry, simAgent);
        freeHandles();
        --mRetryCountRemaining;
        return true;
    }
}

bool ConstructBuildingSimTask::onMinorFailCheckCanRecoverFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) {
    ASSERT_GAME_THREAD();
    if (mRetryCountRemaining == 0) {
        cleanupFull(world, fullRegistry, fullAgent, SimTaskTickResult::Fail);
        return false;
    }
    else {
        EntityActions::dropBundleFull(world, fullRegistry, fullAgent);
        freeHandles();
        --mRetryCountRemaining;
        return true;
    }
}

void ConstructBuildingSimTask::cleanupSim(World& world, entt::registry& simRegistry, entt::entity simAgent, SimTaskTickResult result) {
    EntityActions::dropBundleSim(world, simRegistry, simAgent);
    mState = State::End;
    freeHandles();
    mCurrentResult = result;
}

void ConstructBuildingSimTask::cleanupFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, SimTaskTickResult result) {
    EntityActions::dropBundleFull(world, fullRegistry, fullAgent);
    mState = State::End;
    freeHandles();
    mCurrentResult = result;
}


bool ConstructBuildingSimTask::updateFuture(World& world, entt::registry& fullRegistry, entt::entity fullAgent) {
    std::future_status status = mSimEntityOperationFuture.wait_for(std::chrono::seconds(0));
    assert(status != std::future_status::deferred);
    if (status == std::future_status::ready) {
        bool success = mSimEntityOperationFuture.get();
        if (mSimEntityOperationCompleteFunc) {
            // Move so we can clear it and allow the func to set mSimEntityOperationCompleteFunc
            auto func = std::move(mSimEntityOperationCompleteFunc);
            mSimEntityOperationCompleteFunc = nullptr;
            func(world, fullRegistry, fullAgent, success);
        }
        return true;
    }
    else {
        // Waiting
        return false;
    }
}


void ConstructBuildingSimTask::freeHandles() {
    mBlueprintItemPromise.reset();
    mBlueprintItemTargetHandle.reset();
    mTileItemReservation.reset();
    mTileHarvestReservation.reset();
}
