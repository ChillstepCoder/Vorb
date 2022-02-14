#pragma once

#include "IAgentTask.h"

#include "world/TileHandle.h"

class City;

class ItemStockpile;

enum class GatherTaskState : ui8 {
	INIT,
	PATH_TO_RESOURCE,
	BEGIN_HARVEST,
	HARVESTING,
    PATH_TO_STOCKPILE,
    PICK_STOCKPILE_SLOT,
	PATH_TO_STOCKPILE_SLOT,
	SUCCESS,
	FAIL
};

class GatherTask : public IAgentTask
{
public:
	GatherTask(TileHandle tileTarget, TileResource resource, City* city, ItemStockpile* dstStockpile);
	~GatherTask();

	// Returns True when done
	bool tick(World& world, entt::registry& registry, entt::entity agent) override;

protected:
	void init(World& world, entt::registry& registry, entt::entity agent);
	bool beginHarvest(World& world, entt::registry& registry, entt::entity agent);
	void pathToStockpile(World& world, entt::registry& registry, entt::entity agent);
	void addItemToStockpile(World& world, entt::registry& registry, entt::entity agent);
	void failTask();

    TileHandle mTileTarget;
    TileResource mResource;
    GatherTaskState mState = GatherTaskState::INIT;
	ItemStockpile* mDestinationStockpile;
	City* mCity;
};