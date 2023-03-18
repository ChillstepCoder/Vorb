#pragma once

#include "IAgentTask.h"
#include "city/BuildingBlueprint.h"

struct BuildBlueprintTask : public IAgentTask {
public:
    BuildBlueprintTask(BuildingBlueprint& blueprint, AgentTaskFinishedFunc finishedFunc);
    ~BuildBlueprintTask();

    // TODO:s Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);

    TaskTickResult tick(entt::registry& registry, entt::entity agent) override;

    const char* getTaskName() const override { return "BuildTilesFromPromise"; }

protected:
    bool selectTileToFill(entt::registry& registry, entt::entity agent);
    bool selectTileToBuild(entt::registry& registry, entt::entity agent);
    void placeItemsOnTile(entt::registry& registry, entt::entity agent);

    enum class TaskState : ui8 {
        SELECT_TILE_TO_FILL,
        SELECT_TILE_TO_BUILD,
        PATH_TO_TILE,
        PLACE_ITEMS,
        BUILD_TILE,
        FAIL
    };

    BuildingBlueprint& mBlueprint;
    TileIndex mTargetTileIndex;
    TaskState mState = TaskState::SELECT_TILE_TO_FILL;
    PlaceTileBlueprintItemsHandlePtr mPlaceTilesTarget;
    int mErrorCount = 0;
};

typedef std::unique_ptr<BuildBlueprintTask> BuildBlueprintTaskPtr;