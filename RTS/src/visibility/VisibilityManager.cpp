#include "stdafx.h"
#include "VisibilityManager.h"

#include "visibility/VisibilityThread.h"

VisibilityManager::VisibilityManager(World& world) : mWorld(world) {
    {
        static std::mutex sMutex;
        std::lock_guard<std::mutex> lock(sMutex); // Just in case we create two visibility managers at once
        if (!VisibilityThread::hasInstance()) {
            VisibilityThread::initInstance();
        }
    }
}

VisibilityManager::~VisibilityManager() = default;

void VisibilityManager::initContainerVisibility(TileContainer& container) const {
    VisibilityThread::getInstance().addInitContainerVisibilityTask(container);
}
