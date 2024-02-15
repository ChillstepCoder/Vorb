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
    // REPLACE IS_SHUTTING_DOWN
    world.mIsShuttingDown = true;
    finishedGameThreadShutdown = false;

    if (world.isEditorWorld()) {
        GameThread::getInstance().setActiveEditorWorld(nullptr);
    }

    do {
        RenderContext::getInstance().updateRenderThreadProcs();
    } while (RenderThreadTasks::getInstance().getQueuedProcsApprox());

    GameRenderStateManager::getInstance().setActiveWorld(nullptr);

    world.dispatchOnWorldEndRenderThread(world);

    if (world.mDidBegin) {
        std::lock_guard lock(mShutdownWorldMutex);
        assert(!mCurrentlyShuttingDownWorld);
        mCurrentlyShuttingDownWorld = &world;
    }
    else {
        // If we never began (ie. generation world), no need to shut it down on game thread
        finishedGameThreadShutdown = true;
        world.shutdown();
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
        // Flush the game thread queue with many iterations of flush because threads can be still running
        // I know this isn't clean... but it will do for now
        for (int i = 0; i < 4; ++i) {
            do {
                GameThread::getInstance().updateAllProcs();
                Sleep(16); // Let the render thread produce a few more tasks
            } while (GameThreadTasks::getInstance().getQueuedProcsApprox() ||
                     Services::Threadpool::ref().getTasksSizeApprox() ||
                     Services::Threadpool::ref().getNumRunningThreads());

            // Let the render thread finish its stuff too
            while (RenderThreadTasks::getInstance().getQueuedProcsApprox()) {
                Sleep(4);
            }
        }

        worldToKill->shutdown();

        Sleep(32);

        {
            std::lock_guard lock(mShutdownWorldMutex);
            mCurrentlyShuttingDownWorld = nullptr;
        }

        finishedGameThreadShutdown = true;
    }
    return worldToKill;
}
