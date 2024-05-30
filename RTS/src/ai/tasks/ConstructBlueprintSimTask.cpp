#include "stdafx.h"
#include "ConstructBlueprintSimTask.h"

#include "building/BuildingBlueprint.h"
#include "world/World.h"

#include "world/simulation/host/component/SimCharacterComponents.h"

#include "item/ItemDef.h"
#include "resources/TileRepository.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBlueprintSimTask, 256);

ConstructBlueprintSimTask::ConstructBlueprintSimTask(
    World& world, BuildingBlueprint& blueprint, ConstructBlueprintSimJob& parentJob, SimChunkTileReservationHandle&& tileReservation, TileHarvestable harvestableToAquire
)
    : mBlueprint(blueprint), mParentJob(parentJob), mTileReservation(std::move(tileReservation)), mHarvestableToAquire(harvestableToAquire){
    assert(mTileReservation);

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
        mBlueprint.onEndReservation(mTargetReservationId);
    });

    // TODO: Update on promise increase
    reservationPair.target->bindUpdateFunction([this]() {
        x;
    }

    mBlueprint.totalItemsUnpromised -= itemToFill.quantity;
    assert(mBlueprint.totalItemsUnpromised >= 0);

    // Bind handles
    mBlueprintItemPromise = std::move(reservationPair.source);
    // Store a reference to the blueprint handle so we can remove it
    mTargetReservationId = mBlueprint.nextReservationId++;
    mBlueprint.itemReservationHandles.emplace(mTargetReservationId, std::move(reservationPair.target));
    
}

ConstructBlueprintSimTask::~ConstructBlueprintSimTask()
{
    if (mBlueprintItemPromise) {
        mBlueprintItemPromise->cancel();
    }
}

void ConstructBlueprintSimTask::onBeginFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ConstructBlueprintSimTask::onBeginSim(World& world, entt::registry& simRegistry, entt::entity simAgent) {
    // TODO: Dynamic success radius based on the tile size?
    constexpr f32 SUCCESS_RADIUS = 2.0f;
    mMoveSubtask.init(simRegistry, simAgent, mTileReservation->getLiteTileHandle().getWorldPosition2D(world), SUCCESS_RADIUS);
    mState = State::MoveToHarvestable;
}

SimTaskTickResult ConstructBlueprintSimTask::tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec)
{
    throw std::logic_error("The method or operation is not implemented.");
}

SimTaskTickResult ConstructBlueprintSimTask::tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
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
                    mState = State::End;
                    return SimTaskTickResult::Fail;
                }

                // TODO: Inventory Operations helper?
                constexpr i32 MAX_ROLL_RESULTS = 16;
                ItemRollTable::Result rollResults[MAX_ROLL_RESULTS];
                const i32 resultCount = TileRepository::get().getLoadedOrUnloadedAsset(clearedTileID).itemDrops.rollN(std::span(rollResults, MAX_ROLL_RESULTS), 1);
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
                        assert(!simRegistry.try_get<SimResourceBundleComponent>(simAgent));
                        SimResourceBundleComponent& bundle = simRegistry.get_or_emplace<SimResourceBundleComponent>(simAgent);
                        // TODO: Drop old??
                        if (bundle.itemStack.itemId != INVALID_ITEM_ID) {
                            LOG_WARN("Overwriting item in bundle");
                            assert(false);
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
                mMoveSubtask.init(simRegistry, simAgent, mBlueprint.getCenterPosTile().v, 32.0f);
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
                        i32 promiseIncrease = glm::min(countDiff, mBlueprint.getMaxPromiseSize(bundleItem.itemId));
                        mBlueprintItemPromise->increasePromisedQuantity(bundleItem.itemId, promiseIncrease);
                    }
                        
                    mState = State::PlaceItems;
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
            break;
        case State::FlattenTerrain:
            break;
        case State::BuildTile:
            break;
    }
    return SimTaskTickResult::InProgress;
}

const char* ConstructBlueprintSimTask::getTaskName() const
{
    throw std::logic_error("The method or operation is not implemented.");
}
