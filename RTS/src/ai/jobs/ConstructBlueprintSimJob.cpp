#include "stdafx.h"
#include "ConstructBlueprintSimJob.h"

#include "ai/tasks/MoveToPointSimTask.h"
#include "world/World.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBlueprintSimJob, 256);

ConstructBlueprintSimJob::ConstructBlueprintSimJob(BuildingBlueprint& blueprint, SettlementPlotID plotId, SimECS& simEcs, entt::entity simJobOwner)
    : ISimJob(simJobOwner), mBlueprint(blueprint), mPlotId(plotId), mSimEcs(simEcs) {
    ASSERT_SIM_THREAD();
}

std::unique_ptr<ISimTask> ConstructBlueprintSimJob::tryAquireNextSubtaskForSimCharacter(World& world, entt::registry& simRegistry, entt::entity simCharacter) {
    std::unique_ptr<MoveToPointSimTask> moveTask = std::make_unique<MoveToPointSimTask>(world.getWorldCenter(), 128.0f);
    return moveTask;
}

std::unique_ptr<ISimTask> ConstructBlueprintSimJob::tryAquireNextSubaskForFullCharacter(World& world, entt::registry& fullRegistry, entt::entity fullCharacter)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ConstructBlueprintSimJob::onAbortTask(ISimTask& task)
{

}

void ConstructBlueprintSimJob::onCompleteTask(ISimTask& task)
{

}
