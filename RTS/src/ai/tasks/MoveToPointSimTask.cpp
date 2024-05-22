#include "stdafx.h"
#include "MoveToPointSimTask.h"

#include "world/simulation/host/component/SimCharacterComponents.h"

POOLED_ALLOC_DEF_THREADSAFE(MoveToPointSimTask, 256);

MoveToPointSimSubtask::MoveToPointSimSubtask(f32v2 worldPos, f32 successRadius) : mWorldPosTarget(worldPos), mSuccessRadiusSQ(SQ(successRadius)) {

}

void MoveToPointSimSubtask::init(f32v2 worldPos, f32 successRadius) {
    mWorldPosTarget = worldPos;
    mSuccessRadiusSQ = SQ(successRadius);
}

SimTaskTickResult MoveToPointSimSubtask::tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) {
    assert(mSuccessRadiusSQ > -1.0f);
    throw std::logic_error("The method or operation is not implemented.");
}

SimTaskTickResult MoveToPointSimSubtask::tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent) {
    assert(mSuccessRadiusSQ > -1.0f);
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
