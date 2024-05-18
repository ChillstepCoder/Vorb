#pragma once

#include "world/simulation/ISimJob.h"
#include "building/BuildingBlueprint.h"

class ConstructBlueprintSimJob : public ISimJob {
public:
	ConstructBlueprintSimJob(BuildingBlueprint& blueprint, entt::registry& simRegistry, entt::entity simJobOwner);
	~ConstructBlueprintSimJob() = default;

	POOLED_ALLOC_DECL(ConstructBlueprintSimJob);

	bool tryAquireNextTaskForSimCharacter(entt::registry& simRegistry, entt::entity simCharacter) override;
	bool tryAquireNextTaskForFullCharacter(entt::registry& fullRegistry, entt::entity fullCharacter) override;

};

