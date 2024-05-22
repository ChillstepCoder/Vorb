#include "stdafx.h"
#include "ConstructBlueprintSimTask.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBlueprintSimTask, 256);

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
