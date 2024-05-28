#include "stdafx.h"
#include "ConstructBlueprintSimTask.h"

#include "building/BuildingBlueprint.h"
#include "world/World.h"

#include "world/simulation/host/component/SimCharacterComponents.h"

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
        const i32 quantityRemaining = blueprint.itemComposition[i].desiredQuantity - blueprint.itemComposition[i].filledQuantity;
        if (quantityRemaining > 0) {
            itemToFill.itemId = blueprint.itemComposition[i].itemId;

            // TODO: Account this entities carry weight, nearby items, equipped items, ect when deciding what to commit to
            itemToFill.quantity = glm::min(quantityRemaining, TMP_MAX_COUNT);
            blueprint.itemComposition[i].filledQuantity += itemToFill.quantity;
            break;
        }
    }

    assert(itemToFill.itemId != INVALID_ITEM_ID);
    ItemReservationPair reservationPair = SimpleItemReservation::createReservation(std::span<SimpleItemStack>(&itemToFill, 1));

    reservationPair.source->bindEndFunction([this](ItemReservationEndReason reason, SimpleItemReservationData& data) {
        ASSERT_SIM_THREAD(); // What about full?
        mState = State::End;
        mBlueprint.onEndReservation(mTargetReservationId);

        // Unfill any remaining
        std::span<const ItemID> items = data.getDesiredItems();
        std::span<const i32> desiredQuantities = data.getDesiredQuantities();
        std::span<const i32> filledQuantities = data.getFilledQuantities();
        for (int i = 0; i < items.size(); ++i) {
            const int unfulfulledCount = desiredQuantities[i] - filledQuantities[i];
            if (unfulfulledCount > 0) {
                for (int j = 0; j < mBlueprint.itemCompositionCount; ++j) {
                    if (mBlueprint.itemComposition[j].itemId == items[i]) {
                        mBlueprint.itemComposition[j].filledQuantity -= unfulfulledCount;
                        mBlueprint.totalItemsUnfulfilled -= unfulfulledCount;
                        break;
                    }
                }
            }
        }
    });

    mBlueprint.totalItemsUnfulfilled -= itemToFill.quantity;
    assert(mBlueprint.totalItemsUnfulfilled >= 0);

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

                // TODO: USE DROP TABLE
                //x;

                //assert(!simRegistry.try_get<SimResourceBundleComponent>(simAgent));
                //SimResourceBundleComponent& bundle = simRegistry.emplace<SimResourceBundleComponent>(simAgent);

                // TODO: BUNDLE
               // x;

                mState = State::MoveToBlueprint;
                mMoveSubtask.init(simRegistry, simAgent, mBlueprint.getCenterPosTile().v, 32.0f);
            }
            break;
        }
        case State::MoveToBlueprint: {
            if (mMoveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec) == SimTaskTickResult::Success) {
                // TODO: Do the thing
                return SimTaskTickResult::Success;
            }
            break;
        }
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
