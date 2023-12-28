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

    world.dispatchOnWorldEndRenderThread(world);

    do {
        RenderContext::getInstance().updateRenderThreadProcs();
    } while (RenderThreadTasks::getInstance().getQueuedProcsApprox());

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

World* WorldDestroyer::gameThreadUpdate() {
    World* worldToKill = nullptr;
    {
        std::lock_guard lock(mShutdownWorldMutex);
        worldToKill = mCurrentlyShuttingDownWorld;
    }
    if (worldToKill) {
        // Flush the game thread queue
        GameThread::getInstance().updateAllProcs();

        GameThread::getInstance().setActiveEditorWorld(nullptr);
        worldToKill->shutdown();

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
