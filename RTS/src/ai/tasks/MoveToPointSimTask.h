#pragma once

#include "world/simulation/ISimTask.h"

// Lightweight and reusable
class MoveToPointSimSubtask {
public:
	MoveToPointSimSubtask() = default;
	MoveToPointSimSubtask(f32v2 worldPos, f32 successRadius);

	void init(f32v2 worldPos, f32 successRadius);

	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent);
	SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent);

    f32v2 mWorldPosTarget = f32v2(0.0f);
    f32 mSuccessRadiusSQ = -1.0f;
};

class MoveToPointSimTask : public ISimTask {
public:
	MoveToPointSimTask(f32v2 worldPos, f32 successRadius) : moveSubtask(worldPos, successRadius) {}

    POOLED_ALLOC_DECL();

	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) override {
		return moveSubtask.tickFull(world, fullRegistry, fullAgent);
	}
	SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent) override {
		return moveSubtask.tickSim(world, simRegistry, simAgent);
	}

	const char* getTaskName() const override { return "Move To Point"; }

	MoveToPointSimSubtask moveSubtask;
};