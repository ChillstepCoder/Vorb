#include "stdafx.h"
#include "WorldDestroyer.h"

#include "world/World.h"

#include "rendering/renderer/WorldRenderer.h"
#include "rendering/RenderContext.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/renderstate/GameRenderStateManager.h"
#include "gamethread/GameThread.h"
#include "gamethread/GameThreadTasks.h"

static std::atomic_bool finishedGameThreadShutdown;

void WorldDestroyer::shutdownWorld(World& world) {
    ASSERT_RENDER_THREAD(); // Render thread is responsible for driving world shutdown

    LOG_DEBUG("Shutting down world {}", (void*)&world);

    finishedGameThreadShutdown = false;
    GameThread::getInstance().setActiveEditorWorld(nullptr);

    do {
        RenderContext::getInstance().updateRenderThreadProcs();
    } while (RenderThreadTasks::getInstance().getQueuedProcsApprox());

    world.dispatchOnWorldEndRenderThread(world);

    GameRenderStateManager::getInstance().setActiveWorld(nullptr);

    {
        std::lock_guard lock(mShutdownWorldMutex);
        assert(!mCurrentlyShuttingDownWorld);
        mCurrentlyShuttingDownWorld = &world;
    }

    // Wait for game thread to shutdown the world
    do {
        Sleep(1);
        do {
            RenderContext::getInstance().updateRenderThreadProcs();
        } while (RenderThreadTasks::getInstance().getQueuedProcsApprox());
        RenderThreadTasks::getInstance().processShutdownTasks();
    } while (!finishedGameThreadShutdown);
    RenderThreadTasks::getInstance().processShutdownTasks(); // One more for good measure

    RenderContext::getInstance().getWorldRenderer().removeRenderDataManagerForWorld(world);

    LOG_DEBUG("Finished shutdown");
}

void WorldDestroyer::shutdownAllWorlds() {
    World* worldToDestroy;
    while (World::sWorlds.size()) {
        {
            std::lock_guard lock(World::sWorldsMutex);
            worldToDestroy = World::sWorlds.begin()->second;
        }
        shutdownWorld(*worldToDestroy);
    }
}

World* WorldDestroyer::gameThreadUpdate() {
    World* worldToKill = nullptr;
    {
        std::lock_guard lock(mShutdownWorldMutex);
        worldToKill = mCurrentlyShuttingDownWorld;
    }
    if (worldToKill) {
        // Flush the game thread queue
        do {
            GameThread::getInstance().updateAllProcs();
            Sleep(64); // Let the render thread produce a few more tasks
        } while (GameThreadTasks::getInstance().getQueuedProcsApprox());

        worldToKill->shutdown();

        Sleep(64);

        // Flush the game thread queue again for good measure
        GameThread::getInstance().updateAllProcs();

        {
            std::lock_guard lock(mShutdownWorldMutex);
            mCurrentlyShuttingDownWorld = nullptr;
        }

        finishedGameThreadShutdown = true;
    }
    return worldToKill;
}
