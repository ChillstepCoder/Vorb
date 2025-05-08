#pragma once

#include "world/simulation/ISimTask.h"
#include "ai/tasks/MoveToPointSimTask.h"
#include "item/SimpleItemReservation.h"
#include "tile/SimTileReservation.h"
#include "item/SimChunkTileItemReservation.h"
#include "tile/TileHarvestable.h"
#include "ai/jobs/BuildContextTargetData.h"

#include <future>

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
		World& world, ConstructBuildingSimJob& parentJob, entt::registry& registry, entt::entity agent, bool isSim
	);
	~ConstructBuildingSimTask();

	POOLED_ALLOC_DECL();

    SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) override;
	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) override;


	void onTransitionToFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) override;
	void onTransitionToSim(World& world, entt::registry& simRegistry, entt::entity simAgent) override;

	const char* getTaskName() const override { return "Construct Building"; }
	std::string getDebugString() const override;

private:
	void initItemPromise(FillableSimpleItemStack& blueprintStack, i32 count, bool shouldUpdateBPCount);

	bool simTrySelectItemSource(World& world, entt::registry& simRegistry, entt::entity simAgent);
	void fullTrySelectItemSource(World& world, entt::registry& fullRegistry, entt::entity fullAgent);

    void updateMoveToItemStackSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec);
    void updateMoveToItemStackFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec);

	void updateMoveToHarvestableSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec);
	void updateMoveToHarvestableFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec);

	void updateHarvestSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec);
    void updateHarvestFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec);

	void updateMoveToBlueprintSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec);
    void updateMoveToBlueprintFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec);

	void updatePlaceItemsSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec);
    void updatePlaceItemsFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec);

    void updateConstructSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec);
    void updateConstructFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec);

	// Some steps may fail for various reasons, this allows retry up to retry count
    bool onMinorFailCheckCanRecoverSim(World& world, entt::registry& simRegistry, entt::entity simAgent);
    bool onMinorFailCheckCanRecoverFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent);

    void cleanupSim(World& world, entt::registry& simRegistry, entt::entity simAgent, SimTaskTickResult result);
    void cleanupFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, SimTaskTickResult result);

    bool updateFuture(World& world, entt::registry& fullRegistry, entt::entity fullAgent);

	void freeHandles();

	enum class State {
        Init              = 0,
        MoveToItemStack   = 1,
		MoveToHarvestable = 2,
		Harvest           = 3,
		MoveToBlueprint   = 4,
		PlaceItems        = 5,
		SelectToConstruct = 6,
		End               = 7,
		COUNT
	} mState = State::Init;
	SimTaskTickResult mCurrentResult = SimTaskTickResult::InProgress;

	ConstructBuildingSimJob& mParentJob;
	ConstructBuildingContext& mContext;
	MoveToChunkPointSimSubtask mMoveSubtask;
    SimpleItemReservationSourceHandlePtr mBlueprintItemPromise = nullptr;
	SimpleItemReservationTargetHandlePtr mBlueprintItemTargetHandle = nullptr;
	SimChunkTileItemReservationPtr mTileItemReservation;
	SimChunkTileReservationHandle mTileHarvestReservation;
	TileHarvestable mHarvestableToAquire = TileHarvestable::None;
    SimpleSimTaskTimer mTimer;
	i32 mRetryCountRemaining = 3;
	// Operation
	std::future<bool> mSimEntityOperationFuture;
	// TODO: Simple function pointer?
	std::function<void(World&, entt::registry&, entt::entity, bool)> mSimEntityOperationCompleteFunc;
	//std::unique_ptr<AquireResourceTask> mAquireResourceSubtask;
};

