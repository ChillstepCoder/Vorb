#pragma once

#include "IAgentTask.h"

// TODO: we only need LiteTileHandle
#include "world/Chunk.h"

enum class GatherTaskState {
	INIT,
	PATH_TO_RESOURCE,
	BEGIN_HARVEST,
	HARVESTING,
	PATH_TO_HOME,
	SUCCESS,
	FAIL
};

class GatherTask : public IAgentTask
{
public:
	GatherTask(LiteTileHandle tileTarget, TileResource resource);

	// Returns True when done
	bool tick(World& world, entt::registry& registry, entt::entity agent) override;

protected:
	void init(World& world, entt::registry& registry, entt::entity agent);
	bool beginHarvest(World& world, entt::registry& registry, entt::entity agent);

	LiteTileHandle mTileTarget;
    TileResource mResource;
    ui32v2 mReturnPos;
	GatherTaskState mState = GatherTaskState::INIT;
};

