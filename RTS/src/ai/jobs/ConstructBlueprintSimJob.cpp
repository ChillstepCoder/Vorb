#include "stdafx.h"
#include "ConstructBlueprintSimJob.h"

#include "ai/tasks/ConstructBlueprintSimTask.h"
#include "world/World.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBlueprintSimJob, 256);

ConstructBlueprintSimJob::ConstructBlueprintSimJob(BuildingBlueprint& blueprint, SimECS& simEcs, entt::entity simJobOwner)
    : ISimJob(simJobOwner), mBlueprint(blueprint), mSimEcs(simEcs) {
    ASSERT_SIM_THREAD();
    assert(blueprint.itemCompositionCount);
    assert(blueprint.parentPlotID != INVALID_SETTLEMENT_PLOT_ID);
    assert(blueprint.parentSettlement != entt::null);
    assert(blueprint.worldPosRootDTile.x > -1);
}

std::unique_ptr<ISimTask> ConstructBlueprintSimJob::tryAquireNextSubtaskForSimCharacter(World& world, entt::registry& simRegistry, entt::entity simCharacter) {
    if (mBlueprint.totalItemsUnfulfilled == 0) {
        // TODO: Need to handle when BP has all items but tiles still need to be constructed
        return nullptr;
    }

    std::unique_ptr<ConstructBlueprintSimTask> newTask = std::make_unique<ConstructBlueprintSimTask>(mBlueprint, *this);
    // If state is END then the task could not initialize, likely due to no valid items
    if (newTask->mState == ConstructBlueprintSimTask::State::End) {
        return nullptr;
    }
    return newTask;
}

std::unique_ptr<ISimTask> ConstructBlueprintSimJob::tryAquireNextSubaskForFullCharacter(World& world, entt::registry& fullRegistry, entt::entity fullCharacter)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void ConstructBlueprintSimJob::onAbortTask(ISimTask& task) {

}

void ConstructBlueprintSimJob::onCompleteTask(ISimTask& task)
{

}
