#include "stdafx.h"
#include "MoveToPointSimTask.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/simulation/host/component/SimCharacterComponents.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PositionComponent.h"

POOLED_ALLOC_DEF_THREADSAFE(MoveToPointSimTask, 256);

MoveToPointSimTask::MoveToPointSimTask(entt::registry& registry, entt::entity agent, f32v2 worldPos, f32 successRadius, bool isFull, f32 targetRadius) {
    if (isFull) {
        moveSubtask.initFull(worldPos, successRadius, targetRadius);
    }
    else {
        moveSubtask.initSim(registry, agent, worldPos, successRadius, targetRadius);
    }
}

void MoveToChunkPointSimSubtask::initSim(entt::registry& simRegistry, entt::entity simAgent, f32v2 worldPos, f32 successRadius, f32 targetRadius) {
    mWorldPosTarget = worldPos;
    mSuccessRadiusSQ = SQ(successRadius);
    mTargetRadius = targetRadius;

    simRegistry.get<SimMovementComponent>(simAgent).targetPosition = mWorldPosTarget;
}

void MoveToChunkPointSimSubtask::initFull(f32v2 worldPos, f32 successRadius, f32 targetRadius) {
    mWorldPosTarget = worldPos;
    mSuccessRadiusSQ = SQ(successRadius);
    mTargetRadius = targetRadius;
}

void MoveToChunkPointSimSubtask::onTransitionToSim(entt::registry& simRegistry, entt::entity simAgent) {
    if (mWorldPosTarget != f32v2(-1.0f)) {
        simRegistry.get<SimMovementComponent>(simAgent).targetPosition = mWorldPosTarget;
    }
}

SimTaskTickResult MoveToChunkPointSimSubtask::tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) {
    UNUSED(elapsedSec);
    assert(mSuccessRadiusSQ > -1.0f);

    NavigationComponent& navCmp = fullRegistry.get<NavigationComponent>(fullAgent);

    if (mNavPathID == INVALID_NAV_PATH_ID) [[unlikely]] {
        PositionComponent& posCmp = fullRegistry.get<PositionComponent>(fullAgent);
        f32 zHeightTarget = world.getHeightmapGrid().computeHeightAtPoint<true>(mWorldPosTarget);

        mNavPathID = navCmp.requestCoarsePath(posCmp.mPosition, f32v3(mWorldPosTarget.x, mWorldPosTarget.y, zHeightTarget), mTargetRadius);
        return SimTaskTickResult::InProgress;
    }
    else if (navCmp.getCurrentNavPathID() == mNavPathID) {
        // Check if our pathing task is done
        NavigationStatus status = navCmp.getStatus();
        switch (status) {
            case NavigationStatus::INVALID:
            case NavigationStatus::FAIL:
                mWorldPosTarget = f32v2(-1.0f);
                mNavPathID = INVALID_NAV_PATH_ID;
                return SimTaskTickResult::Fail;
            case NavigationStatus::IN_PROGRESS:
                return SimTaskTickResult::InProgress;
            case NavigationStatus::SUCCESS:
                mWorldPosTarget = f32v2(-1.0f);
                mNavPathID = INVALID_NAV_PATH_ID;
                return SimTaskTickResult::Success;
                break;
            default:
                panic("Invalid nav status in MoveToChunkPointSimSubtask::tickFull");
                break;
        }
    }
    
    // If we get here, our path was interrupted by another path task
    mWorldPosTarget = f32v2(-1.0f);
    mNavPathID = INVALID_NAV_PATH_ID;
    return SimTaskTickResult::Fail;
}

SimTaskTickResult MoveToChunkPointSimSubtask::tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) {
    UNUSED(elapsedSec);

    assert(mSuccessRadiusSQ > -1.0f);
    // Movement is handled by the SimMovementComponent tick, we merely check proximity here
    SimMovementComponent& moveCmp = simRegistry.get<SimMovementComponent>(simAgent);
    
    const f32v2 offset = simRegistry.get<SimPositionComponent>(simAgent).getPosition() - mWorldPosTarget;
    if (glm::length2(offset) <= mSuccessRadiusSQ) {
        moveCmp.clearTarget();
        mWorldPosTarget = f32v2(-1.0f);
        return SimTaskTickResult::Success;
    }

    // Refresh move target every frame in case it got changed
    moveCmp.targetPosition = mWorldPosTarget;
    return SimTaskTickResult::InProgress;
}
