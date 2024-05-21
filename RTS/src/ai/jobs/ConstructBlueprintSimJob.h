#pragma once

#include "world/simulation/ISimJob.h"
#include "building/BuildingBlueprint.h"

class SimECS;
class ISimTask;

class ConstructBlueprintSimJob : public ISimJob {
public:
	ConstructBlueprintSimJob(BuildingBlueprint& blueprint, SettlementPlotID plotId, SimECS& simEcs, entt::entity simJobOwner);
	~ConstructBlueprintSimJob() = default;

	POOLED_ALLOC_DECL(ConstructBlueprintSimJob);

	std::unique_ptr<ISimTask> tryAquireNextSubtaskForSimCharacter(World& world, entt::registry& simRegistry, entt::entity simCharacter) override;
	std::unique_ptr<ISimTask> tryAquireNextSubaskForFullCharacter(World& world, entt::registry& fullRegistry, entt::entity fullCharacter) override;

	void onAbortTask(ISimTask& task) override;
	void onCompleteTask(ISimTask& task) override;

private:

	BuildingBlueprint& mBlueprint;
	SettlementPlotID mPlotId;
	SimECS& mSimEcs;
};

