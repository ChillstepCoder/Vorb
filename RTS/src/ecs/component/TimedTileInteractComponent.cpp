#include "stdafx.h"
#include "TimedTileInteractComponent.h"

#include "World.h"

TimedTileInteractComponent::TimedTileInteractComponent(
    TileHandle interactPos,
    ui8 tileLayer,
    ui32 ticksUntilFinished,
    ui16 repeatCount,
    std::function<void(bool, TimedTileInteractComponent&)> callback /*= nullptr*/
) :
    mWorldInteractPos(interactPos),
    mTileLayer(tileLayer),
    mTimer(ticksUntilFinished, false /*tickAtStart*/),
    mRepeatCount(repeatCount),
    mInteractFinishedCallback(callback)
{
    interactPos.tile.
}

TimedTileInteractSystem::TimedTileInteractSystem(World& world) : mWorld(world) {

}

void TimedTileInteractSystem::update(entt::registry& registry) {
    auto view = registry.view<TimedTileInteractComponent>();
    // TODO: what happens if the chunk we are interacting with is deallocated?
    // TODO: SafeTileHandle (refcounted)
    // TODO: Is it safe to remove in iteration or is this undefined?
    for (auto entity : view) {
        auto& cmp = view.get<TimedTileInteractComponent>(entity);
        if (cmp.mTimer.tryTick()) {
            cmp.mProgress = 1.0f;
            if (cmp.mInteractFinishedCallback) {
                cmp.mInteractFinishedCallback(true, cmp);
            }
            if (cmp.mCurrRepeat >= cmp.mRepeatCount) {
                // Remove the component
                registry.remove<TimedTileInteractComponent>(entity);
            }
            else {
                ++cmp.mCurrRepeat;
            }
        }
        else {
            cmp.mProgress = cmp.mTimer.getTickDelta();
        }
    };
}
