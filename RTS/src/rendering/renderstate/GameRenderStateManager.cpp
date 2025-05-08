#include "stdafx.h"
#include "GameRenderStateManager.h"

GameRenderStateManager* GameRenderStateManager::sInstance = nullptr;

GameRenderStateManager& GameRenderStateManager::initInstance() {
    if (!sInstance) {
        sInstance = new GameRenderStateManager();
    }
    return *sInstance;
}

GameRenderStateManager& GameRenderStateManager::getInstance() {
    assert(sInstance);
    return *sInstance;
}

void GameRenderStateManager::setActiveWorld(const World* activeWorld) {
    std::lock_guard<std::mutex> lock(mWorldLock);
    mActiveWorld = activeWorld;
}

bool GameRenderStateManager::isActiveWorld(const World* world) {
    std::lock_guard<std::mutex> lock(mWorldLock);
    return world == mActiveWorld;
}

WorldRenderState& GameRenderStateManager::getRenderStateForUpdate() {
    ASSERT_GAME_THREAD();
    assert(mActiveWorld);
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

void GameRenderStateManager::finishUpdating() {
    ASSERT_GAME_THREAD();
    std::lock_guard<std::mutex> lock(mLock);
    // Mark the currently updating buffer as the last updated
    mLastUpdated = mUpdating;
}

WorldRenderState& GameRenderStateManager::getRenderStateForRender() {
    ASSERT_RENDER_THREAD();
    {
        std::lock_guard<std::mutex> lock(mLock);
        // Render the last updated state
        mRendering = mLastUpdated;
    }
    return mRenderState[mRendering];
}
