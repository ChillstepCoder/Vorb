#pragma once

#include "IAgentTask.h"
#include "tile/TileHandle.h"
#include "item/ItemReservation.h"

class City;

class ItemStockpile;

enum class GatherTaskState : ui8 {
	INIT,
	PATH_TO_RESOURCE,
	BEGIN_HARVEST,
	HARVESTING,
	PATH_TO_STOCKPILE_SLOT,
	ADD_ITEM_TO_STOCKPILE_SLOT,
	SUCCESS,
	FAIL
};

class GatherTask : public IAgentTask
{
public:
	GatherTask(TileHandle tileTarget, TileHarvestable resource, std::unique_ptr<ItemReservation> itemPromise);
	~GatherTask();

	// Returns True when done
	bool tick(entt::registry& registry, entt::entity agent) override;

    VORB_NON_COPYABLE_BUT_MOVABLE(GatherTask);

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
	static void operator delete(void* pointer, size_t size);

protected:
	void init(entt::registry& registry, entt::entity agent);
	bool beginHarvest(entt::registry& registry, entt::entity agent);
	void pathToStockpileSlot(entt::registry& registry, entt::entity agent);
	void addItemToStockpile(entt::registry& registry, entt::entity agent);
	void failTask();

    TileHandle mTileTarget;
    TileHarvestable mResource;
	std::unique_ptr<ItemReservation> mItemPromise;
    GatherTaskState mState = GatherTaskState::INIT;
	City* mCity;
};

//class GatherResourceTask : public IAgentTask {
//
//};

class GatherItemsForPromiseTask : public IAgentTask {
public:
	GatherItemsForPromiseTask(ItemPromiseWeakPtr&& itemPromise);
	~GatherItemsForPromiseTask();

    // TODO: Override allocation to use boost::singleton_pool
    // static void* operator new(size_t count);
    // static void operator delete(void* pointer, size_t size);

	enum class TaskState : ui8 {
		FIND_ITEM,
		PATH_TO_ITEM,
		HARVEST_ITEM,
		HARVESTING,
		SUCCESS,
		FAIL
	};

	bool tick(entt::registry& registry, entt::entity agent) override;

protected:
    void findItem(entt::registry& registry, entt::entity agent);
    void harvestItem(entt::registry& registry, entt::entity agent);

    ItemPromiseWeakPtr mItemPromise;
    TileHandle mCurrentTileTarget;
	TaskState mState = TaskState::FIND_ITEM;
};

typedef std::unique_ptr<GatherItemsForPromiseTask> GatherItemsForPromiseTaskPtr;