#pragma once

#include "IAgentTask.h"

struct BuildingBlueprint;

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
    bool selectTileToBuild(entt::registry& registry, entt::entity agent);

    enum class TaskState : ui8 {
        SELECT_TILE,
        PATH_TO_TILE,
        BUILD_TILE
    };

    BuildingBlueprint& mBlueprint;
    TileIndex mTargetTileIndex;
    TaskState mTaskState = TaskState::SELECT_TILE;
};

typedef std::unique_ptr<BuildBlueprintTask> BuildBlueprintTaskPtr;