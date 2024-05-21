#include "stdafx.h"
#include "ConstructBlueprintSimJob.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBlueprintSimJob, 256);

ConstructBlueprintSimJob::ConstructBlueprintSimJob(BuildingBlueprint& blueprint, SettlementPlotID plotId, SimECS& simEcs, entt::entity simJobOwner)
    : ISimJob(simJobOwner), mBlueprint(blueprint), mPlotId(plotId), mSimEcs(simEcs) {
    ASSERT_SIM_THREAD();
}

std::unique_ptr<ISimTask> ConstructBlueprintSimJob::tryAquireNextSubtaskForSimCharacter(entt::registry& simRegistry, entt::entity simCharacter) {
    
}

std::unique_ptr<ISimTask> ConstructBlueprintSimJob::tryAquireNextSubaskForFullCharacter(entt::registry& fullRegistry, entt::entity fullCharacter)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ConstructBlueprintSimJob::onAbortTask(ISimTask& task)
{

}

void ConstructBlueprintSimJob::onCompleteTask(ISimTask& task)
{

}
