#include "stdafx.h"
#include "MoveToPointSimTask.h"

#include "world/simulation/host/component/SimCharacterComponents.h"

POOLED_ALLOC_DEF_THREADSAFE(MoveToPointSimTask, 256);

MoveToPointSimSubtask::MoveToPointSimSubtask(entt::registry& simRegistry, entt::entity simAgent, f32v2 worldPos, f32 successRadius) {
    init(simRegistry, simAgent, worldPos, successRadius);
}

void MoveToPointSimSubtask::init(entt::registry& simRegistry, entt::entity simAgent, f32v2 worldPos, f32 successRadius) {
    mWorldPosTarget = worldPos;
    mSuccessRadiusSQ = SQ(successRadius);

    simRegistry.get<SimMovementComponent>(simAgent).targetPosition = mWorldPosTarget;
}

SimTaskTickResult MoveToPointSimSubtask::tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) {
    assert(mSuccessRadiusSQ > -1.0f);
    throw std::logic_error("The method or operation is not implemented.");
}

SimTaskTickResult MoveToPointSimSubtask::tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
    assert(mSuccessRadiusSQ > -1.0f);
    // Movement is handled by the SimMovementComponent tick, we merely check proximity here
    SimMovementComponent& moveCmp = simRegistry.get<SimMovementComponent>(simAgent);
    
    const f32v2 offset = simRegistry.get<SimPositionComponent>(simAgent).getPosition() - mWorldPosTarget;
    if (glm::length2(offset) <= mSuccessRadiusSQ) {
        moveCmp.clearTarget();
        return SimTaskTickResult::Success;
    }

    // Refresh move target every frame in case it got changed
    moveCmp.targetPosition = mWorldPosTarget;
    return SimTaskTickResult::InProgress;
}
