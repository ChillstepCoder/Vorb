#pragma once

#include "IAgentTask.h"

#include "item/ItemReservation.h"


class ItemReservation;
struct BuildingBlueprint;

enum class BuildTaskState {
    FULFILL_RESERVATIONS,
    PATH_TO_STOCKPILE,
    GRAB_RESOURCES,
    PATH_TO_BLUEPRINT,
    BUILD,
    SUCCESS,
    FAIL
};

class BuildTask : public IAgentTask {
public:
    BuildTask(BuildingBlueprint& blueprint, std::vector<std::unique_ptr<ItemReservation>>&& sourceItems, std::vector<ui16>&& targetTiles);
    ~BuildTask();

	bool tick(World& world, entt::registry& registry, entt::entity agent) override;

    VORB_NON_COPYABLE_BUT_MOVABLE(BuildTask);

private:
    BuildTaskState mState = BuildTaskState::FULFILL_RESERVATIONS;
    std::vector<std::unique_ptr<ItemReservation>> mSourceItems;
    std::vector<ui16> mTargetTiles; // BP relative
    BuildingBlueprint& mBlueprint;
};

