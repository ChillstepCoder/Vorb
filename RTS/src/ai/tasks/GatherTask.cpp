#include "stdafx.h"
#include "GatherTask.h"

#include "World.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PhysicsComponent.h"

GatherTask::GatherTask(LiteTileHandle tileTarget, TileResource resource) :
    mTileTarget(tileTarget),
    mResource(resource)
{

}

void initGatherTask();

bool GatherTask::tick(World& world, entt::registry& registry, entt::entity agent) {
    switch (mState) {
        case GatherTaskState::INIT: {
            init(world, registry, agent);
            break;
        }
        case GatherTaskState::PATH_TO_RESOURCE:
            // Awaiting callback
            break;
        case GatherTaskState::BEGIN_HARVEST:
            // Make sure tile still has the resource
            if (!beginHarvest(world, registry, agent)) {
                mState = GatherTaskState::FAIL;
                return true;
            }
            mState = GatherTaskState::HARVESTING;
            break;
        case GatherTaskState::HARVESTING:
            break;
        case GatherTaskState::PATH_TO_HOME:
            break;
        case GatherTaskState::SUCCESS:
        case GatherTaskState::FAIL:
            return true;
        default:
            assert(false);
            break;
    }
    return false;
}

void GatherTask::init(World& world, entt::registry& registry, entt::entity agent) {

    NavigationComponent& navCmp = registry.get_or_emplace<NavigationComponent>(agent);
    // If we already have a path, wait for it to finish
    if (navCmp.mPath) {
        return;
    }
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);

    // Make sure tile still has the resource
    if (!world.tileHasHarvestableResource(physCmp.getXYPosition(), mResource)) {
        mState = GatherTaskState::FAIL;
        return;
    }

    // TODO: temp    
    mReturnPos = physCmp.getXYPosition();

    navCmp.setPathWithCallback(
        Services::PathFinder::ref().generatePathSynchronous(world, mTileTarget.getWorldPos(), physCmp.getXYPosition()),
        [this](bool success) {
        if (success == true) {
            mState = GatherTaskState::BEGIN_HARVEST;
        }
        else {
            // Failed to path, fail he task
            mState = GatherTaskState::FAIL;
        }
    }
    );
    navCmp.mPath = Services::PathFinder::ref().generatePathSynchronous(world, mTileTarget.getWorldPos(), physCmp.getXYPosition());
    mState = GatherTaskState::PATH_TO_RESOURCE;
}

bool GatherTask::beginHarvest(World& world, entt::registry& registry, entt::entity agent)
{
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(agent);
    if (!world.tileHasHarvestableResource(physCmp.getXYPosition(), mResource)) {
        mState = GatherTaskState::FAIL;
        return false;
    }

    // Interact

    return true;
}
