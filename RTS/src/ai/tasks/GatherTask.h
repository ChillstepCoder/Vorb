#pragma once

#include "IAgentTask.h"

#include "tile/TileHandle.h"

class City;

class ItemStockpile;
class ItemReservation;

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
	GatherTask(TileHandle tileTarget, TileResource resource, std::unique_ptr<ItemReservation> itemPromise);
	~GatherTask();

	// Returns True when done
	bool tick(entt::registry& registry, entt::entity agent) override;

    VORB_NON_COPYABLE_BUT_MOVABLE(GatherTask);

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
	static void operator delete(void* pointer, size_t size);

protected:
	void init(World& world, entt::registry& registry, entt::entity agent);
	bool beginHarvest(World& world, entt::registry& registry, entt::entity agent);
	void pathToStockpileSlot(World& world, entt::registry& registry, entt::entity agent);
	void addItemToStockpile(World& world, entt::registry& registry, entt::entity agent);
	void failTask();

    TileHandle mTileTarget;
    TileResource mResource;
	std::unique_ptr<ItemReservation> mItemPromise;
    GatherTaskState mState = GatherTaskState::INIT;
	City* mCity;
};