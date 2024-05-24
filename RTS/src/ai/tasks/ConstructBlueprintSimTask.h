#pragma once

#include "world/simulation/ISimTask.h"
#include "ai/tasks/MoveToPointSimTask.h"
#include "item/SimpleItemReservation.h"

class BuildingBlueprint;

// TODO: Serialization?
class ConstructBlueprintSimTask : public ISimTask
{
public:
	ConstructBlueprintSimTask(BuildingBlueprint& blueprint);
	~ConstructBlueprintSimTask();

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
		BuildTile,
		End
	} mState = State::Init;

	BuildingBlueprint& mBlueprint;
	MoveToPointSimSubtask mMoveSubtask;
	SimpleItemReservationSourceHandlePtr mItemReservation = nullptr;
	ui32 mTargetReservationId = 0;
	//std::unique_ptr<AquireResourceTask> mAquireResourceSubtask;
};

