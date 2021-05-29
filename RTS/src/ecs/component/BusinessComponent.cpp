#include "stdafx.h"
#include "BusinessComponent.h"

const int UPDATE_INTERVAL = 60;

BusinessSystem::BusinessSystem(World& world) :
    mWorld(world)
{

}

void updateComponent(World& world, BusinessComponent& cmp, float deltaTime) {

}

void BusinessSystem::update(entt::registry& registry, float deltaTime)
{
    // Update slower
    if (--mFramesUntilUpdate <= 0) {
        mFramesUntilUpdate = UPDATE_INTERVAL;
    }
    else {
        return;
    }

    // Update components
    registry.view<BusinessComponent>().each([&](auto& cmp) {
        updateComponent(mWorld, cmp, deltaTime);
    });
}
