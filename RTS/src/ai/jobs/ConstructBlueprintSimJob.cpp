#include "stdafx.h"
#include "ConstructBlueprintSimJob.h"

POOLED_ALLOC_DEF_THREADSAFE(ConstructBlueprintSimJob, 256);

ConstructBlueprintSimJob::ConstructBlueprintSimJob(BuildingBlueprint& blueprint, entt::registry& simRegistry, entt::entity simJobOwner) {
    ASSERT_SIM_THREAD();
}

bool ConstructBlueprintSimJob::tryAquireNextTaskForSimCharacter(entt::registry& simRegistry, entt::entity simCharacter)
{
    throw std::logic_error("The method or operation is not implemented.");
}

bool ConstructBlueprintSimJob::tryAquireNextTaskForFullCharacter(entt::registry& fullRegistry, entt::entity fullCharacter)
{
    throw std::logic_error("The method or operation is not implemented.");
}
