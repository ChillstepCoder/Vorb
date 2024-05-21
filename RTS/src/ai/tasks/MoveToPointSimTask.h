#pragma once

#include "world/simulation/ISimTask.h"

class MoveToPointSimTask : public ISimTask
{
public:
	MoveToPointSimTask(f32v2 worldPos, f32 successRadius);

    POOLED_ALLOC_DECL();

	SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) override;
	SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent) override;

	const char* getTaskName() const override { return "Move To Point"; }

	f32v2 mWorldPosTarget;
	f32 mSuccessRadiusSQ;
};
