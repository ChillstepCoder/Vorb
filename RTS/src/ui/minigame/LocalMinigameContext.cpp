#include "stdafx.h"
#include "LocalMinigameContext.h"

void LocalMinigameContext::updateAndRender(const f32v2 screenResolution, f32 elapsedSec) {
    ASSERT_RENDER_THREAD();

    { // Init new minigame if needed
        std::lock_guard lock(mQueuedMingameLock);
        if (mQueuedMinigameInit) {

            if (mCurrentMinigame) {
                mCurrentMinigame->abort();
            }

            mQueuedMinigameInit();
            mQueuedMinigameInit = nullptr;
        }
    }

    if (mCurrentMinigame) {
        if (mCurrentMinigame->updateAndRender(screenResolution, elapsedSec) != MinigameResultType::InProgress) {
            mCurrentMinigame.reset();
        }
    }
}

void LocalMinigameContext::abortCurrentMinigame() {
    ASSERT_RENDER_THREAD();
    if (mCurrentMinigame) {
        mCurrentMinigame->abort();
        mCurrentMinigame.reset();
    }
}

void LocalMinigameContext::beginFishingMinigame(const FishDef& fishData, std::function<void(const FishingMinigameResult& result)> onFinished) {
    std::lock_guard lock(mQueuedMingameLock);
    mQueuedMinigameInit = [this, &fishData, onFinished]() {
        mCurrentMinigame = std::make_unique<FishingMinigame>(fishData, onFinished);
    };

}

FishingMinigame* LocalMinigameContext::tryGetActiveFishingMinigame() const {
    ASSERT_RENDER_THREAD();
    if (!mCurrentMinigame) {
        return nullptr;
    }

    return dynamic_cast<FishingMinigame*>(mCurrentMinigame.get());
}
