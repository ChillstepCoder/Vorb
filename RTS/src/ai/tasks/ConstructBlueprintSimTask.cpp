#include "stdafx.h"
#include "ConstructBlueprintSimTask.h"

#include "building/BuildingBlueprint.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBlueprintSimTask, 256);

ConstructBlueprintSimTask::ConstructBlueprintSimTask(BuildingBlueprint& blueprint) : mBlueprint(blueprint) {
    SimpleItemStack itemToFill;
    constexpr i32 TMP_MAX_COUNT = 8;
    for (i32 i = 0; i < blueprint.itemCompositionCount; ++i) {
        const i32 quantityRemaining = blueprint.itemComposition[i].desiredQuantity - blueprint.itemComposition[i].filledQuantity;
        if (quantityRemaining > 0) {
            itemToFill.itemId = blueprint.itemComposition[i].itemId;

            // TODO: Account this entities carry weight, nearby items, equipped items, ect when deciding what to commit to
            itemToFill.quantity = glm::min(quantityRemaining, TMP_MAX_COUNT);
            break;
        }
    }

    assert(itemToFill.itemId != INVALID_ITEM_ID);
    ItemReservationPair reservationPair = SimpleItemReservation::createReservation(std::span<SimpleItemStack>(&itemToFill, 1));

    reservationPair.source->bindEndFunction([this](ItemReservationEndReason reason) {
        ASSERT_SIM_THREAD(); // What about full?
        mState = State::End;
        mBlueprint.onEndReservation(mTargetReservationId);
    });

    reservationPair.target->bindUpdateFunction([this](ItemReservationUpdateType type, ItemID id, i32 quantity) {
        if (type >= ItemReservationUpdateType::END_TYPES) {
            mState = State::End;
            mBlueprint.onEndReservation(mTargetReservationId);
        }
    });

    mBlueprint.totalItemsUnfulfilled -= itemToFill.quantity;
    assert(mBlueprint.totalItemsUnfulfilled >= 0);

    // Bind handles
    mItemReservation = std::move(reservationPair.source);
    // Store a reference to the blueprint handle so we can remove it
    mTargetReservationId = mBlueprint.nextReservationId++;
    mBlueprint.itemReservationHandles.emplace(mTargetReservationId, std::move(reservationPair.target));
}

ConstructBlueprintSimTask::~ConstructBlueprintSimTask()
{
    if (mItemReservation) {
        mItemReservation->cancel();
    }
}

void ConstructBlueprintSimTask::onBeginFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ConstructBlueprintSimTask::onBeginSim(World& world, entt::registry& simRegistry, entt::entity simAgent)
{
    throw std::logic_error("The method or operation is not implemented.");
}

SimTaskTickResult ConstructBlueprintSimTask::tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent)
{
    throw std::logic_error("The method or operation is not implemented.");
}

SimTaskTickResult ConstructBlueprintSimTask::tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent)
{
    throw std::logic_error("The method or operation is not implemented.");
}

const char* ConstructBlueprintSimTask::getTaskName() const
{
    throw std::logic_error("The method or operation is not implemented.");
}
