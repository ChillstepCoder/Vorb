#include "stdafx.h"
#include "MoveToPointSimTask.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/simulation/host/component/SimCharacterComponents.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PositionComponent.h"

POOLED_ALLOC_DEF_THREADSAFE(MoveToPointSimTask, 256);

MoveToChunkPointSimSubtask::MoveToChunkPointSimSubtask(entt::registry& simRegistry, entt::entity simAgent, f32v2 worldPos, f32 successRadius) {
    init(simRegistry, simAgent, worldPos, successRadius);
}

void MoveToChunkPointSimSubtask::init(entt::registry& simRegistry, entt::entity simAgent, f32v2 worldPos, f32 successRadius) {
    mWorldPosTarget = worldPos;
    mSuccessRadiusSQ = SQ(successRadius);

    simRegistry.get<SimMovementComponent>(simAgent).targetPosition = mWorldPosTarget;
}

SimTaskTickResult MoveToChunkPointSimSubtask::tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) {
    UNUSED(elapsedSec);
    assert(mSuccessRadiusSQ > -1.0f);

    NavigationComponent& navCmp = fullRegistry.get<NavigationComponent>(fullAgent);

    if (mNavPathID == INVALID_NAV_PATH_ID) [[unlikely]] {
        PositionComponent& posCmp = fullRegistry.get<PositionComponent>(fullAgent);
        f32 zHeightTarget = world.getHeightmapGrid().computeHeightAtPoint<true>(mWorldPosTarget);

        mNavPathID = navCmp.requestCoarsePath(posCmp.mPosition, f32v3(mWorldPosTarget.x, mWorldPosTarget.y, zHeightTarget));
    }
    else if (navCmp.getCurrentNavPathID() == mNavPathID) {
        // Check if our pathing task is done
        NavigationStatus status = navCmp.getStatus();
        switch (status) {
            case NavigationStatus::INVALID:
                return SimTaskTickResult::Fail;
            case NavigationStatus::IN_PROGRESS:
                return SimTaskTickResult::InProgress;
            case NavigationStatus::SUCCESS:
                return SimTaskTickResult::Success;
                break;
            case NavigationStatus::FAIL:
                return SimTaskTickResult::Fail;
                break;
            default:
                panic("Invalid nav status in MoveToChunkPointSimSubtask::tickFull");
                break;
        }
    }
    else {
        // If we get here, our path was interrupted by another path task
        return SimTaskTickResult::Fail;
    }

}

SimTaskTickResult MoveToChunkPointSimSubtask::tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
    UNUSED(elapsedSec);

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
