#include "stdafx.h"
#include "RenderStateManager.h"

RenderStateManager* RenderStateManager::sInstance = nullptr;

// Increments i in modulo 3 for triple buffering
void incrementMod3(OUT int& i) {
    i = (i + 1) % 3;
}

RenderStateManager& RenderStateManager::initInstance() {
    if (!sInstance) {
        sInstance = new RenderStateManager();
    }
    return *sInstance;
}

RenderStateManager& RenderStateManager::getInstance() {
    assert(sInstance);
    return *sInstance;
}

RenderState& RenderStateManager::getRenderStateForUpdate() {
    assert(IS_GAME_THREAD());
    {
        std::lock_guard<std::mutex> lock(mLock);
        // Get the next free state
        incrementMod3(mUpdating);
        if (mUpdating == mRendering) {
            incrementMod3(mUpdating);
        }
    }
    return mRenderState[mUpdating];
}

void RenderStateManager::finishUpdating() {
    assert(IS_GAME_THREAD());
    std::lock_guard<std::mutex> lock(mLock);
    // Mark the currently updating buffer as the last updated
    mLastUpdated = mUpdating;
}

const RenderState& RenderStateManager::getRenderStateForRender() {
    assert(IS_RENDER_THREAD());
    {
        std::lock_guard<std::mutex> lock(mLock);
        // Render the last updated state
        mRendering = mLastUpdated;
    }
    return mRenderState[mRendering];
}
