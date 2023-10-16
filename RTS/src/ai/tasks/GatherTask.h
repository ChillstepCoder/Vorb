#pragma once

#include "IAgentTask.h"
#include "tile/TileHandle.h"
#include "item/ItemReservation.h"

class World;

class HarvestItemsTask : public IAgentTask {
public:
	HarvestItemsTask(ItemID itemId, ui16 itemCount, AgentTaskFinishedFunc finishedFunc);
	~HarvestItemsTask();

	POOLED_ALLOC_DECL();

    // TODO: Override allocation to use boost::singleton_pool
    // static void* operator new(size_t count);
    // static void operator delete(void* pointer, size_t size);

	enum class TaskState : ui8 {
		FIND_ITEM,
		PATH_TO_ITEM,
		HARVESTING,
		SUCCESS,
		FAIL
	};

	TaskTickResult tick(World& world, entt::registry& registry, entt::entity agent) override;

	const char* getTaskName() const override { return "GatherItemsForPromise"; }

protected:
    void findItem(entt::registry& registry, entt::entity agent);
    void harvestItem(World& world, entt::registry& registry, entt::entity agent, TileHandle targetTileHandle);
	void failTask();

	ItemID mItemId;
	ui16 mTargetCount;
	ui16 mCurrentCount = 0;
	ui16 mFailCount = 0;
	TileHarvestable mTargetHarvestable = TileHarvestable::NONE;
	TaskState mState = TaskState::FIND_ITEM;
};

typedef std::unique_ptr<HarvestItemsTask> HarvestItemsTaskPtr;