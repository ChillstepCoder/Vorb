#include "stdafx.h"
#include "TimedTileInteractComponent.h"

#include "world/IWorld.h"

TimedTileInteractComponent::TimedTileInteractComponent(
    TileHandle interactTile,
    ui8 tileLayer,
    ui32 ticksUntilFinished,
    ui16 repeatCount,
    std::function<void(bool, TimedTileInteractComponent&)> callback /*= nullptr*/
) :
    mInteractTile(std::make_unique<TileRef>(interactTile)),
    mTileLayer(tileLayer),
    mTimer(ticksUntilFinished, false /*tickAtStart*/),
    mRepeatCount(repeatCount),
    mInteractFinishedCallback(callback)
{
    assert(!mInteractTile->tile->hasFlag(TileFlags::IS_INTERACTING));
    mInteractTile->container->setTileFlag(mInteractTile->index, TileFlags::IS_INTERACTING);
}

TimedTileInteractComponent::~TimedTileInteractComponent() {
}

TimedTileInteractSystem::TimedTileInteractSystem() {

}

void TimedTileInteractSystem::update(entt::registry& registry) {
    auto view = registry.view<TimedTileInteractComponent>();
    // TODO: Is it safe to remove in iteration or is this undefined?
    for (auto entity : view) {
        auto& cmp = view.get<TimedTileInteractComponent>(entity);
        if (cmp.mTimer.tryTick()) {
            cmp.mProgress = 1.0f;
            if (cmp.mInteractFinishedCallback) {
                cmp.mInteractFinishedCallback(true, cmp);
            }
            if (cmp.mCurrRepeat >= cmp.mRepeatCount) {
                // Remove the component and reset flag.
                // This crashes in the destructor because it frees the unique_ptr first sadly
                assert(cmp.mInteractTile);
                cmp.mInteractTile->container->clearTileFlag(cmp.mInteractTile->index, TileFlags::IS_INTERACTING);
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
