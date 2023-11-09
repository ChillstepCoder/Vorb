#include "stdafx.h"
#include "VisibilityManager.h"

#include "tile/TileContainerRepository.h"

#include "visibility/VisibilityThread.h"
#include "world/World.h"

VisibilityManager::VisibilityManager(World& world) : mWorld(world) {
    {
        static std::mutex sMutex;
        std::lock_guard<std::mutex> lock(sMutex); // Just in case we create two visibility managers at once
        if (!VisibilityThread::hasInstance()) {
            VisibilityThread::initInstance();
        }
    }
    initEvents();
}

VisibilityManager::~VisibilityManager() = default;

void VisibilityManager::initContainerVisibility(TileContainer& container) const {
    VisibilityThread::getInstance().addInitContainerVisibilityTask(container);
}

void VisibilityManager::initEvents() {
    TileContainerRepository& repo = mWorld.getTileContainerRepository();
    repo.registerTileContainerListeners(mTileContainerEventListeners);
    repo.addDestroyListener(mTileContainerEventListeners, [this](const TileContainerEvent& containerEvent) {
        ASSERT_GAME_THREAD();
       // mContainersToDestroy.gameThreadDirtyObject(container.getId());
    });
}
