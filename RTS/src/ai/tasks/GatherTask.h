#pragma once

#include "IAgentTask.h"

// TODO: we only need LiteTileHandle
#include "world/Chunk.h"

class City;

enum class GatherTaskState {
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
	GatherTask(LiteTileHandle tileTarget, TileResource resource, City* city);

	// Returns True when done
	bool tick(World& world, entt::registry& registry, entt::entity agent) override;

protected:
	void init(World& world, entt::registry& registry, entt::entity agent);
	bool beginHarvest(World& world, entt::registry& registry, entt::entity agent);
	void pathToStockpile(World& world, entt::registry& registry, entt::entity agent);
	void addItemToStockpile(World& world, entt::registry& registry, entt::entity agent);

	LiteTileHandle mTileTarget;
    TileResource mResource;
	City* mCity;
	GatherTaskState mState = GatherTaskState::INIT;
};

