#pragma once

#include "world/simulation/ISimTask.h"
#include "ai/tasks/MoveToPointSimTask.h"
#include "item/SimpleItemReservation.h"
#include "tile/SimTileReservation.h"

class BuildingBlueprint;
class ConstructBlueprintSimJob;

// TODO: Serialization?
class ConstructBlueprintSimTask : public ISimTask
{
	friend class ConstructBlueprintSimJob;
public:
	// We will aquire the tileReservations, and the job will release them after
	ConstructBlueprintSimTask(BuildingBlueprint& blueprint, ConstructBlueprintSimJob& parentJob, std::span<SimChunkTileReservationHandle> tileReservations);
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

	ConstructBlueprintSimJob& mParentJob;
	BuildingBlueprint& mBlueprint;
	MoveToPointSimSubtask mMoveSubtask;
    SimpleItemReservationSourceHandlePtr mBlueprintItemPromise = nullptr;
    std::unique_ptr<SimChunkTileReservationHandle[]> mTileReservations;
    i32 mNumTileReservations = 0;
	ui32 mTargetReservationId = 0;
	//std::unique_ptr<AquireResourceTask> mAquireResourceSubtask;
};

