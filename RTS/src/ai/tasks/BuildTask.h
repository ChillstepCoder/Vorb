#pragma once

#include "IAgentTask.h"

enum class BuildTaskState {
    INIT,
    PATH_TO_STOCKPILE,
    GRAB_RESOURCES,
    PATH_TO_BLUEPRINT,
    BUILD,
    SUCCESS,
    FAIL
};

class BuildTask : public IAgentTask
{
public:
    //BuildTask();

	bool tick(World& world, entt::registry& registry, entt::entity agent) override;

private:
    BuildTaskState mState = BuildTaskState::INIT;
};

