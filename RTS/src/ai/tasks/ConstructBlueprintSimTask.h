#pragma once

#include "world/simulation/ISimTask.h"
#include "ai/tasks/MoveToPointSimTask.h"
#include "item/SimpleItemReservation.h"
#include "tile/SimTileReservation.h"
#include "tile/TileHarvestable.h"

class BuildingBlueprint;
class ConstructBlueprintSimJob;

// TODO: Serialization?
class ConstructBlueprintSimTask : public ISimTask
{
	friend class ConstructBlueprintSimJob;
public:
	// We will aquire the tileReservations, and the job will release them after
	ConstructBlueprintSimTask(
		World& world, BuildingBlueprint& blueprint, ConstructBlueprintSimJob& parentJob, SimChunkTileReservationHandle&& tileReservation, TileHarvestable harvestableToAquire
	);
	~ConstructBlueprintSimTask();

	POOLED_ALLOC_DECL();

	void onBeginFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) override;
	void onBeginSim(World& world, entt::registry& simRegistry, entt::entity simAgent) override;

	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) override;
	SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) override;

	const char* getTaskName() const override;

private:

	enum class State {
		Init,
		MoveToHarvestable,
		Harvest,
		MoveToBlueprint,
		PlaceItems,
		FlattenTerrain,
		BuildTile,
		End
	} mState = State::Init;

	ConstructBlueprintSimJob& mParentJob;
	BuildingBlueprint& mBlueprint;
	MoveToPointSimSubtask mMoveSubtask;
    SimpleItemReservationSourceHandlePtr mBlueprintItemPromise = nullptr;
	SimChunkTileReservationHandle mTileReservation;
	ui32 mTargetReservationId = 0;
	TileHarvestable mHarvestableToAquire;
	SimpleSimTaskTimer mTimer;
	//std::unique_ptr<AquireResourceTask> mAquireResourceSubtask;
};

