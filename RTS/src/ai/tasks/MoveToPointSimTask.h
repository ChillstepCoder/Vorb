#pragma once

#include "world/simulation/ISimTask.h"

// Lightweight and reusable
class MoveToChunkPointSimSubtask {
public:
	MoveToChunkPointSimSubtask() = default;
	MoveToChunkPointSimSubtask(entt::registry& simRegistry, entt::entity simAgent, f32v2 worldPos, f32 successRadius);

	void init(entt::registry& simRegistry, entt::entity simAgent, f32v2 worldPos, f32 successRadius);

	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec);
	SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec);

    f32v2 mWorldPosTarget = f32v2(0.0f);
    f32 mSuccessRadiusSQ = -1.0f;
	NavPathID mNavPathID = INVALID_NAV_PATH_ID;
};

class MoveToPointSimTask : public ISimTask {
public:
	MoveToPointSimTask(entt::registry& simRegistry, entt::entity simAgent, f32v2 worldPos, f32 successRadius) :
		moveSubtask(simRegistry, simAgent, worldPos, successRadius) {}

    POOLED_ALLOC_DECL();

	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) override {
		return moveSubtask.tickFull(world, fullRegistry, fullAgent, elapsedSec);
	}
	SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) override {
		return moveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec);
	}

	const char* getTaskName() const override { return "Move To Point"; }

	MoveToChunkPointSimSubtask moveSubtask;
};