#pragma once

#include "IAgentTask.h"

#include "item/ItemReservation.h"


class ItemReservation;
struct BuildingBlueprint;

enum class BuildTaskState {
    FULFILL_RESERVATIONS,
    PATH_TO_STOCKPILE_SLOT,
    PULL_ITEM_FROM_STOCKPILE_SLOT,
    PATH_TO_BLUEPRINT_TILE,
    BUILD_TILE,
    SUCCESS,
    FAIL
};

class BuildTask : public IAgentTask {
public:
    BuildTask(BuildingBlueprint& blueprint, std::vector<std::unique_ptr<ItemReservation>>&& sourceItems, std::vector<ui16>&& targetTiles);
    ~BuildTask();

	bool tick(entt::registry& registry, entt::entity agent) override;

    VORB_NON_COPYABLE_BUT_MOVABLE(BuildTask);

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);

private:

    void pathToStockpileSlot(entt::registry& registry, entt::entity agent);
    void pullItemFromStockpile(entt::registry& registry, entt::entity agent);
    void pathToBlueprint(entt::registry& registry, entt::entity agent);
    void buildTile(entt::registry& registry, entt::entity agent);
    void failTask();

    BuildTaskState mState = BuildTaskState::FULFILL_RESERVATIONS;
    std::vector<std::unique_ptr<ItemReservation>> mSourceItems;
    std::vector<ui16> mTargetTiles; // BP relative
    BuildingBlueprint& mBlueprint;
};

