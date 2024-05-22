#pragma once

#include "world/simulation/ISimTask.h"
#include "ai/tasks/MoveToPointSimTask.h"

// TODO: Serialization?
class ConstructBlueprintSimTask : public ISimTask
{
public:

	POOLED_ALLOC_DECL();

	void onBeginFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) override;
	void onBeginSim(World& world, entt::registry& simRegistry, entt::entity simAgent) override;

	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) override;
	SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent) override;

	const char* getTaskName() const override;

private:

	enum class State {
		Init,
		AquireResources,
		MoveToBlueprint,
		FlattenTerrain,
		BuildTile
	} mState = State::Init;

	MoveToPointSimSubtask mMoveSubtask;
	//std::unique_ptr<AquireResourceTask> mAquireResourceSubtask;
};

