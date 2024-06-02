#include "stdafx.h"
#include "ConstructBuildingSimTask.h"

#include "building/BuildingBlueprint.h"
#include "building/Building.h"
#include "world/World.h"

#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/simulation/host/component/SimSettlementComponents.h"
#include "world/IHeightmapGrid.h"

#include "world/chunk/SimChunkGrid.h"

#include "gamethread/GameThreadTasks.h"

#include "item/ItemDef.h"
#include "resources/TileRepository.h"

#include "ai/AIActions.h"
#include "ai/jobs/ConstructBuildingSimJob.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBuildingSimTask, 256);


constexpr i32 TMP_CARRY_COUNT = 6;

ConstructBuildingSimTask::ConstructBuildingSimTask(
    World& world, ConstructBuildingSimJob& parentJob, entt::registry& simRegistry, entt::entity simAgent
)
    : mContext(parentJob.mContext), mParentJob(parentJob) {

    if (trySelectItemSource(world, simRegistry)) {
        // TODO: Dynamic success radius based on the tile size?
        constexpr f32 SUCCESS_RADIUS = 2.0f;
        mMoveSubtask.init(simRegistry, simAgent, mTileReservation->getLiteTileHandle().getWorldPosition2D(world), SUCCESS_RADIUS);
        mState = State::MoveToHarvestable;
    }
    else {
        mState = State::SelectToConstruct;
    }
}

ConstructBuildingSimTask::~ConstructBuildingSimTask()
{
    if (mBlueprintItemPromise) {
        mBlueprintItemPromise->cancel();
    }
}

void ConstructBuildingSimTask::onBeginFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ConstructBuildingSimTask::onBeginSim(World& world, entt::registry& simRegistry, entt::entity simAgent) {
    
}

SimTaskTickResult ConstructBuildingSimTask::tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec)
{
    throw std::logic_error("The method or operation is not implemented.");
}

SimTaskTickResult ConstructBuildingSimTask::tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
    switch (mState) {
        case State::Init:
            assert(false);
            break;
        case State::MoveToHarvestable:
            if (mMoveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec) == SimTaskTickResult::Success) {

                // Check if harvestable still exists
                SimTileData tileData = mTileReservation->getCurrentTileDataCopy();
                if (tileData.tileId == TILE_ID_NONE ||
                    TileRepository::get().getLoadedOrUnloadedAsset(tileData.tileId).harvestable != mHarvestableToAquire) {
                    // TODO: Instead fall back to finding new tile
                    LOG_WARN("Need to find new tile in harvest state for construct blueprint due to lost harvestable");
                    cleanupSim(world, simRegistry, simAgent);
                    return SimTaskTickResult::Fail;
                }

                mState = State::Harvest;
                // TODO: Estimate duration better
                mTimer.begin(10.0f);
            }
            break;
        case State::Harvest: {
            if (mTimer.tick(elapsedSec)) {

                const TileID clearedTileID = mTileReservation->tryClearHarvestable(mHarvestableToAquire);
                if (clearedTileID == TILE_ID_NONE) {
                    // TODO: Instead fall back to finding new tile
                    LOG_WARN("Need to find new tile in harvest state for construct blueprint");
                    cleanupSim(world, simRegistry, simAgent);
                    return SimTaskTickResult::Fail;
                }

                // TODO: Inventory Operations helper?
                constexpr i32 MAX_ROLL_RESULTS = 16;
                ItemRollTable::Result rollResults[MAX_ROLL_RESULTS];
                const i32 resultCount = TileRepository::get().getLoadedOrUnloadedAsset(clearedTileID).itemDrops.roll(std::span(rollResults, MAX_ROLL_RESULTS));
                if (!resultCount) {
                    LOG_WARN("Failed item drop result on construct blueprint task");
                    cleanupSim(world, simRegistry, simAgent);
                    return SimTaskTickResult::Fail;
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
                        SimResourceBundleComponent& bundle = simRegistry.get_or_emplace<SimResourceBundleComponent>(simAgent);
                        // TODO use StatsComponent?
                        if (quantity > TMP_CARRY_COUNT) {
                            // Drop extra on floor
                            i32 quantityToDrop = quantity - TMP_CARRY_COUNT;
                            TileCoord worldPos(i32v2(simRegistry.get<SimPositionComponent>(simAgent).getPosition()));
                            ItemStack dropItem(itemAsset.getAssetID(), quantityToDrop);
                            const bool success = world.getSimChunkGrid().tryDropItemStackOnGround(dropItem, worldPos);
                            quantity -= quantityToDrop;
                        }

                        if (bundle.itemStack.itemId != INVALID_ITEM_ID) {
                            LOG_WARN("Dropping old bundle");
                            AIActions::dropBundleSim(world, simRegistry, simAgent);
                        }
                        bundle.itemStack.itemId = itemAsset.getAssetID();
                        bundle.itemStack.count = quantity;
                        bundle.itemStack.harvestableType = mHarvestableToAquire; // TODO: ensure this is correct?
                    }
                    else {
                        // Drop unneeded stack on floor
                        TileCoord worldPos(i32v2(simRegistry.get<SimPositionComponent>(simAgent).getPosition()));
                        ItemStack dropItem(itemAsset.getAssetID(), quantity);
                        const bool success = world.getSimChunkGrid().tryDropItemStackOnGround(dropItem, worldPos);
                        assert(success);
                    }
                }

                mState = State::MoveToBlueprint;
                mMoveSubtask.init(simRegistry, simAgent, mContext.blueprint.getCenterPosTile().v, 32.0f);
            }
            break;
        }
        case State::MoveToBlueprint: {
            if (mMoveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec) == SimTaskTickResult::Success) {
                if (SimResourceBundleComponent* bundle = simRegistry.try_get<SimResourceBundleComponent>(simAgent)) {
                    SimpleItemStack& bundleItem = bundle->itemStack;

                    // We have extra items, let the BP know we intend to use them!
                    i32 existingPromiseSize = mBlueprintItemPromise->getRemainingQuantity(bundleItem.itemId);
                    const i32 countDiff = bundleItem.count - existingPromiseSize;
                    if (countDiff > 0) {
                        const i32 promiseIncrease = glm::min(countDiff, mContext.blueprint.getMaxPromiseSize(bundleItem.itemId));
                        if (!mBlueprintItemPromise->tryIncreasePromisedQuantity(bundleItem.itemId, promiseIncrease)) {
                            LOG_WARN("Already fulfilled our promise in MoveToBlueprint");
                            cleanupSim(world, simRegistry, simAgent);
                            return SimTaskTickResult::Fail;
                        }
                    }

                    std::optional<BuildContextTargetData> targetData = mContext.tryAquireTargetForItem(bundleItem.itemId);
                    if (targetData) {
                        mTargetData = *targetData;
                        assert(mTargetData.isValid());
                        const i32v2 targetPosWorld = mContext.building.getTileWorldPos(mTargetData.targetIndex);
                        mMoveSubtask.init(simRegistry, simAgent, targetPosWorld, 1.0f);
                        mState = State::PlaceItems;
                    } else {
                        // No valid target to build, drop bundle and fall back to construction
                        LOG_WARN("No valid target for item in construct blueprint task, need impl drop bundle");
                        mState = State::SelectToConstruct;
                    }
                }
                else {
                    // Lost our bundle somehow...
                    LOG_WARN("Lost bundle in construct blueprint task");
                    cleanupSim(world, simRegistry, simAgent);
                    return SimTaskTickResult::Fail;
                }
            }
            break;
        }
        case State::PlaceItems:
            if (mMoveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec) == SimTaskTickResult::Success) {
                // Insert our items
                if (SimResourceBundleComponent* bundle = simRegistry.try_get<SimResourceBundleComponent>(simAgent)) {
                    SimpleItemStack& bundleItem = bundle->itemStack;

                    FillableRecipe& recipe = mContext.blueprint.getRecipeForTargetData(mTargetData);
                    const i32 remainder = recipe.fillItemAndReturnRemainder(bundleItem.itemId, bundleItem.count);
                    const i32 filledQuantity = bundleItem.count - remainder;
                    if (!mBlueprintItemPromise->tryFulfillQuantity(bundleItem.itemId, filledQuantity)) {
                        LOG_WARN("Already fulfilled our promise in PlaceItems");
                        mState = State::End;
                        return SimTaskTickResult::Fail;
                    }
                    mContext.blueprint.totalItemsUnfulfilled -= filledQuantity;
                    bundleItem.count = remainder;

                    if (recipe.isFullyFilled()) {
                        // Will be pulled during construction
                        mContext.returnTargetToConstruct(mTargetData);
                    } else {
                        mContext.returnTargetForItem(bundleItem.itemId, mTargetData);
                    }
                    mTargetData.invalidate();
                   
                    if (bundleItem.count > 0) {
                        std::optional<BuildContextTargetData> targetData = mContext.tryAquireTargetForItem(bundleItem.itemId);
                        if (targetData) {
                            mTargetData = *targetData;
                            assert(mTargetData.isValid());
                            i32v2 targetPosWorld = mContext.building.getTileWorldPos(mTargetData.targetIndex);
                            mMoveSubtask.init(simRegistry, simAgent, targetPosWorld, 1.0f);
                        }
                        else {
                            // TODO: Drop bundle!
                            mState = State::SelectToConstruct;
                        }
                    }
                    else {
                        mState = State::SelectToConstruct;
                    }
                }
                else {
                    // Lost our bundle somehow...
                    LOG_WARN("Lost bundle in construct blueprint task place items");
                    cleanupSim(world, simRegistry, simAgent);
                    return SimTaskTickResult::Fail;
                }
            }
            break;
        case State::SelectToConstruct: {
            std::optional<BuildContextTargetData> targetData = mContext.tryAquireTargetToConstruct();
            if (targetData) {
                assert(targetData->isValid());
                mTargetData = *targetData;
                const i32v2 targetPosWorld = mContext.building.getTileWorldPos(mTargetData.targetIndex);
                mMoveSubtask.init(simRegistry, simAgent, targetPosWorld, 1.0f);
                mState = State::MoveToConstruct;
                [[fallthrough]];
            }
            else {
                // Nothing to do anymore in this task cycle
                cleanupSim(world, simRegistry, simAgent);
                return SimTaskTickResult::Success;
            }
        }
        case State::MoveToConstruct: {
            if (mMoveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec) == SimTaskTickResult::Success) {
                mState = State::Construct;
                mTimer.begin(1.0f);
            }
            else {
                break;
            }
        }
        case State::Construct: {
            if (mTimer.tick(elapsedSec)) {
                TileIndex tileIndex;
                assert(mTargetData.isValid());
                switch (mTargetData.type) {
                    case BuildContextTargetData::Type::Tile:
                        tileIndex = mContext.blueprint.tileTargets[mTargetData.targetIndex].tileIndex;
                        mContext.blueprint.tileTargets[mTargetData.targetIndex].fillableRecipe.setConstructed(true);
                        break;
                    case BuildContextTargetData::Type::Wall:
                        tileIndex = mContext.blueprint.wallTargets[mTargetData.targetIndex].tileIndex;
                        mContext.blueprint.wallTargets[mTargetData.targetIndex].fillableRecipe.setConstructed(true);
                        break;
                    case BuildContextTargetData::Type::Stairs:
                        tileIndex = mContext.blueprint.stairTargets[mTargetData.targetIndex].piece.pos;
                        mContext.blueprint.stairTargets[mTargetData.targetIndex].fillableRecipe.setConstructed(true);
                        break;
                    default:
                        assert(false);
                        break;
                }
                static_assert(e_count(BuildContextTargetData::Type) == 3, "Update switch statement");

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

                --mContext.blueprint.totalTargetsUnbuilt;

                std::optional<BuildContextTargetData> targetData = mContext.tryAquireTargetToConstruct();
                if (targetData) {
                    assert(targetData->isValid());
                    mTargetData = *targetData;
                    const i32v2 targetPosWorld = mContext.building.getTileWorldPos(mTargetData.targetIndex);
                    mMoveSubtask.init(simRegistry, simAgent, targetPosWorld, 1.0f);
                    mState = State::MoveToConstruct;
                    [[fallthrough]];
                }
                else {
                    // Nothing to do anymore in this task cycle
                    cleanupSim(world, simRegistry, simAgent);
                    return SimTaskTickResult::Success;
                }
            }
            break;
        }
    }
    return SimTaskTickResult::InProgress;
}

const char* ConstructBuildingSimTask::getTaskName() const {
    throw std::logic_error("The method or operation is not implemented.");
}

bool ConstructBuildingSimTask::trySelectItemSource(World& world, entt::registry& simRegistry) {
    BuildingBlueprint& blueprint = mContext.blueprint;
    entt::entity settlementEntity = blueprint.parentSettlement;
    assert(settlementEntity != entt::null);
    SettlementHarvestableTrackerComponent& harvestTracker = simRegistry.get<SettlementHarvestableTrackerComponent>(settlementEntity);

    const TileCoord settlementCenter = simRegistry.get<SettlementSimComponent>(settlementEntity).getCenterPos(world.getWidthChunks());

    SimChunkGrid& simGrid = world.getSimChunkGrid();

    // Determine what we should harvest
    // Send characters to harvestables if we need more items
    if (blueprint.totalItemsUnpromised != 0) {
        i32 itemCompIndex = 0;
        i32 neededCount = 0;
        for (; itemCompIndex < blueprint.itemCompositionCount; ++itemCompIndex) {
            FillableSimpleItemStack& itemStack = blueprint.itemComposition[itemCompIndex];
            neededCount = itemStack.getMaxPromiseSize();
            if (neededCount > 0) {
                if (itemStack.harvestableType != TileHarvestable::None) {
                    mTileReservation = harvestTracker.tryReserveNearestHarvestable(
                        itemStack.harvestableType, settlementCenter, simGrid
                    );
                }
                else {
                    assert(false); // Need to handle non harvestables
                }
                // Check if we managed to reserve a tile for harvest
                if (mTileReservation) {
                    mHarvestableToAquire = itemStack.harvestableType;
                    break;
                }
            }
        }

        // Make item reservation now that we have a worthy tile to harvest
        if (mTileReservation) {
            FillableSimpleItemStack& blueprintStack = blueprint.itemComposition[itemCompIndex];

            SimpleItemStack itemToFill;
            itemToFill.itemId = blueprintStack.itemId;
            itemToFill.count = glm::min(neededCount, TMP_CARRY_COUNT);
            blueprintStack.promisedQuantity += itemToFill.count;

            assert(itemToFill.itemId != INVALID_ITEM_ID);
            ItemReservationPair reservationPair = SimpleItemReservation::createReservation(std::span<SimpleItemStack>(&itemToFill, 1));

            reservationPair.source->bindEndFunction([this](ItemReservationEndReason reason, SimpleItemReservationData& data) {
                ASSERT_SIM_THREAD(); // What about full?
                mState = State::End;
                mContext.blueprint.onEndItemReservation(mTargetReservationId);
            });

            // Notify blueprint when we increase our promised amount due to overharvest
            reservationPair.target->bindUpdateFunction([this](ItemReservationUpdateType type, ItemID id, i32 quantity) {
                if (type == ItemReservationUpdateType::PromiseIncrease) {
                    for (i32 i = 0; i < mContext.blueprint.itemCompositionCount; ++i) {
                        FillableSimpleItemStack& itemStack = mContext.blueprint.itemComposition[i];
                        if (itemStack.itemId == id) {
                            itemStack.promisedQuantity += quantity;
                            mContext.blueprint.totalItemsUnpromised -= quantity;
                            break;
                        }
                    }
                }
            });

            blueprint.totalItemsUnpromised -= itemToFill.count;
            assert(blueprint.totalItemsUnpromised >= 0);

            // Bind handles
            mBlueprintItemPromise = std::move(reservationPair.source);
            // Store a reference to the blueprint handle so we can remove it
            mTargetReservationId = blueprint.getNextReservationID();
            blueprint.itemReservationHandles.emplace(mTargetReservationId, std::move(reservationPair.target));

            return true;
        }
    }
    return false;
}

void ConstructBuildingSimTask::cleanupSim(World& world, entt::registry& simRegistry, entt::entity simAgent) {
    AIActions::dropBundleSim(world, simRegistry, simAgent);
    mState = State::End;
}
