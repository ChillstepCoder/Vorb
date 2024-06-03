#pragma once

#include "world/simulation/ISimTask.h"
#include "ai/tasks/MoveToPointSimTask.h"
#include "item/SimpleItemReservation.h"
#include "tile/SimTileReservation.h"
#include "item/SimChunkTileItemReservation.h"
#include "tile/TileHarvestable.h"
#include "ai/jobs/BuildContextTargetData.h"

class BuildingBlueprint;
class ConstructBuildingSimJob;
class ConstructBuildingContext;

// TODO: Serialization?
class ConstructBuildingSimTask : public ISimTask
{
	friend class ConstructBuildingSimJob;
public:
	// We will aquire the tileReservations, and the job will release them after
	ConstructBuildingSimTask(
		World& world, ConstructBuildingSimJob& parentJob, entt::registry& simRegistry, entt::entity simAgent
	);
	~ConstructBuildingSimTask();

	POOLED_ALLOC_DECL();

	void onBeginFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) override;
	void onBeginSim(World& world, entt::registry& simRegistry, entt::entity simAgent) override;

	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) override;
	SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) override;

	const char* getTaskName() const override;

private:
	void initItemPromise(FillableSimpleItemStack& blueprintStack, i32 count, bool shouldUpdateBPCount);
	bool trySelectItemSource(World& world, entt::registry& simRegistry, entt::entity simAgent);

	void cleanupSim(World& world, entt::registry& simRegistry, entt::entity simAgent);

	enum class State {
        Init,
        MoveToItemStack,
		MoveToHarvestable,
		Harvest,
		MoveToBlueprint,
		PlaceItems,
		SelectToConstruct,
		MoveToConstruct,
		Construct,
		End
	} mState = State::Init;

	ConstructBuildingSimJob& mParentJob;
	ConstructBuildingContext& mContext;
	MoveToPointSimSubtask mMoveSubtask;
    SimpleItemReservationSourceHandlePtr mBlueprintItemPromise = nullptr;
	SimpleItemReservationTargetHandlePtr mBlueprintItemTargetHandle = nullptr;
	SimChunkTileItemReservationPtr mTileItemReservation;
	SimChunkTileReservationHandle mTileHarvestReservation;
	TileHarvestable mHarvestableToAquire = TileHarvestable::None;
	SimpleSimTaskTimer mTimer;
	BuildContextTargetData mTargetData;
	//std::unique_ptr<AquireResourceTask> mAquireResourceSubtask;
};

