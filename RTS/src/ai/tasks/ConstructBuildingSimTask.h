#pragma once

#include "world/simulation/ISimTask.h"
#include "ai/tasks/MoveToPointSimTask.h"
#include "item/SimpleItemReservation.h"
#include "tile/SimTileReservation.h"
#include "tile/TileHarvestable.h"

class BuildingBlueprint;
class ConstructBuildingSimJob;

// TODO: Serialization?
class ConstructBuildingSimTask : public ISimTask
{
	friend class ConstructBuildingSimJob;
public:
	// We will aquire the tileReservations, and the job will release them after
	ConstructBuildingSimTask(
		World& world, BuildingBlueprint& blueprint, ConstructBuildingSimJob& parentJob, SimChunkTileReservationHandle&& tileReservation, TileHarvestable harvestableToAquire
	);
	~ConstructBuildingSimTask();

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

	ConstructBuildingSimJob& mParentJob;
	BuildingBlueprint& mBlueprint;
	MoveToPointSimSubtask mMoveSubtask;
    SimpleItemReservationSourceHandlePtr mBlueprintItemPromise = nullptr;
	SimChunkTileReservationHandle mTileReservation;
	ui32 mTargetReservationId = 0;
	TileHarvestable mHarvestableToAquire;
	SimpleSimTaskTimer mTimer;
	//std::unique_ptr<AquireResourceTask> mAquireResourceSubtask;
};

