#include "stdafx.h"
#include "ConstructBuildingSimTask.h"

#include "building/BuildingBlueprint.h"
#include "building/Building.h"
#include "world/World.h"

#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/IHeightmapGrid.h"

#include "gamethread/GameThreadTasks.h"

#include "item/ItemDef.h"
#include "resources/TileRepository.h"

#include "ai/jobs/ConstructBuildingSimJob.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBuildingSimTask, 256);

ConstructBuildingSimTask::ConstructBuildingSimTask(
    World& world, ConstructBuildingSimJob& parentJob, SimChunkTileReservationHandle&& tileReservation, TileHarvestable harvestableToAquire
)
    : mContext(parentJob.mContext), mParentJob(parentJob), mTileReservation(std::move(tileReservation)), mHarvestableToAquire(harvestableToAquire){
    assert(mTileReservation);

    BuildingBlueprint& blueprint = mContext.blueprint;

    SimpleItemStack itemToFill;
    constexpr i32 TMP_MAX_COUNT = 8;
    for (i32 i = 0; i < blueprint.itemCompositionCount; ++i) {
        FillableSimpleItemStack& stack = blueprint.itemComposition[i];
        const i32 quantityPromisable = stack.getMaxPromiseSize();
        if (quantityPromisable > 0) {
            itemToFill.itemId = blueprint.itemComposition[i].itemId;

            // TODO: Account this entities carry weight, nearby items, equipped items, ect when deciding what to commit to
            itemToFill.quantity = glm::min(quantityPromisable, TMP_MAX_COUNT);
            blueprint.itemComposition[i].promisedQuantity += itemToFill.quantity;
            break;
        }
    }

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

    blueprint.totalItemsUnpromised -= itemToFill.quantity;
    assert(blueprint.totalItemsUnpromised >= 0);

    // Bind handles
    mBlueprintItemPromise = std::move(reservationPair.source);
    // Store a reference to the blueprint handle so we can remove it
    mTargetReservationId = blueprint.nextItemReservationId++;
    blueprint.itemReservationHandles.emplace(mTargetReservationId, std::move(reservationPair.target));
    
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
    // TODO: Dynamic success radius based on the tile size?
    constexpr f32 SUCCESS_RADIUS = 2.0f;
    mMoveSubtask.init(simRegistry, simAgent, mTileReservation->getLiteTileHandle().getWorldPosition2D(world), SUCCESS_RADIUS);
    mState = State::MoveToHarvestable;
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
                    mState = State::End;
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
                    mState = State::End;
                    return SimTaskTickResult::Fail;
                }

                // TODO: Inventory Operations helper?
                constexpr i32 MAX_ROLL_RESULTS = 16;
                ItemRollTable::Result rollResults[MAX_ROLL_RESULTS];
                const i32 resultCount = TileRepository::get().getLoadedOrUnloadedAsset(clearedTileID).itemDrops.roll(std::span(rollResults, MAX_ROLL_RESULTS));
                if (!resultCount) {
                    LOG_WARN("Failed item drop result on construct blueprint task");
                    mState = State::End;
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
                    const i32 quantity = rollResults[i].quantity;
                    if (i == bundleSelect) {
                        SimResourceBundleComponent& bundle = simRegistry.get_or_emplace<SimResourceBundleComponent>(simAgent);
                        // TODO: Drop old??
                        if (bundle.itemStack.itemId != INVALID_ITEM_ID) {
                            LOG_WARN("Overwriting item in bundle");
                        }
                        bundle.itemStack.itemId = itemAsset.getAssetID();
                        bundle.itemStack.quantity = quantity;
                        bundle.itemStack.harvestableType = mHarvestableToAquire; // TODO: ensure this is correct?
                    }
                    else {
                        assert(false);
                        // TODO: drop on the ground
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
                    const i32 countDiff = bundleItem.quantity - existingPromiseSize;
                    if (countDiff > 0) {
                        const i32 promiseIncrease = glm::min(countDiff, mContext.blueprint.getMaxPromiseSize(bundleItem.itemId));
                        if (!mBlueprintItemPromise->tryIncreasePromisedQuantity(bundleItem.itemId, promiseIncrease)) {
                            LOG_WARN("Already fulfilled our promise in MoveToBlueprint");
                            mState = State::End;
                            return SimTaskTickResult::Fail;
                        }
                    }

                    std::optional<BuildContextTargetData> targetData = mContext.tryAquireTargetForItem(bundleItem.itemId);
                    if (targetData) {
                        mTargetData = *targetData;
                        const i32v2 targetPosWorld = mContext.building.getTileWorldPos(mTargetData.targetIndex);
                        mMoveSubtask.init(simRegistry, simAgent, targetPosWorld, 1.0f);
                        mState = State::PlaceItems;
                    } else {
                        // No valid target to build, drop bundle and fall back to construction
                        LOG_WARN("No valid target for item in construct blueprint task, need impl drop bundle");
                        mState = State::Construct;
                    }
                }
                else {
                    // Lost our bundle somehow...
                    LOG_WARN("Lost bundle in construct blueprint task");
                    mState = State::End;
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
                    const i32 remainder = recipe.fillItemAndReturnRemainder(bundleItem.itemId, bundleItem.quantity);
                    const i32 filledQuantity = bundleItem.quantity - remainder;
                    if (!mBlueprintItemPromise->tryFulfillQuantity(bundleItem.itemId, filledQuantity)) {
                        LOG_WARN("Already fulfilled our promise in PlaceItems");
                        mState = State::End;
                        return SimTaskTickResult::Fail;
                    }
                    mContext.blueprint.totalItemsUnfulfilled -= filledQuantity;
                    bundleItem.quantity = remainder;

                    if (recipe.isFullyFilled()) {
                        // Will be pulled during construction
                        mContext.returnTargetToConstruct(mTargetData);
                    } else {
                        mContext.returnTargetForItem(bundleItem.itemId, mTargetData);
                    }
                    mTargetData.invalidate();
                   
                    if (bundleItem.quantity > 0) {
                        std::optional<BuildContextTargetData> targetData = mContext.tryAquireTargetForItem(bundleItem.itemId);
                        if (targetData) {
                            mTargetData = *targetData;
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
                    mState = State::End;
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
                mState = State::End;
                return SimTaskTickResult::Success;
            }
        }
        case State::MoveToConstruct: {
            if (mMoveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec) == SimTaskTickResult::Success) {
                mState = State::Construct;
                mTimer.begin(1.0f);
            }
            break;
        }
        case State::Construct: {
            if (mTimer.tick(elapsedSec)) {
                TileIndex tileIndex;
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
                    // Epsilon to prevent Z fighting
                    GameThreadTasks::getInstance().addGenericTask([zpos = tileAABB.z - 0.005f, tileWorldPos, &world]() {
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
                    mState = State::End;
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
