#pragma once

#include "world/simulation/ISimTask.h"

// Lightweight and reusable
class MoveToChunkPointSimSubtask {
public:

    void initSim(entt::registry& simRegistry, entt::entity simAgent, f32v2 worldPos, f32 successRadius, f32 targetRadius);
	void initFull(f32v2 worldPos, f32 successRadius, f32 targetRadius);

    void onTransitionToSim(entt::registry& simRegistry, entt::entity simAgent);

	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec);
	SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec);

    f32v2 mWorldPosTarget = f32v2(-1.0f);
    f32 mSuccessRadiusSQ = -1.0f;
	NavPathID mNavPathID = INVALID_NAV_PATH_ID;
	f32 mTargetRadius = false;
};

class MoveToPointSimTask : public ISimTask {
public:
	MoveToPointSimTask(entt::registry& registry, entt::entity agent, f32v2 worldPos, f32 successRadius, bool isFull, f32 targetRadius);

    POOLED_ALLOC_DECL();

	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) override {
		return moveSubtask.tickFull(world, fullRegistry, fullAgent, elapsedSec);
	}
	SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) override {
		return moveSubtask.tickSim(world, simRegistry, simAgent, elapsedSec);
	}

	virtual void onTransitionToSim(World&, entt::registry& simRegistry, entt::entity simAgent) { moveSubtask.onTransitionToSim(simRegistry, simAgent); }

	const char* getTaskName() const override { return "Move To Point"; }
	std::string getDebugString() const override;

	MoveToChunkPointSimSubtask moveSubtask;
};