#include "stdafx.h"
#include "GameThread.h"

#include "network/cli/GameClient.h"
#include "network/srv/GameServer.h"

#include "world/World.h"
#include "world/WorldDestroyer.h"

#include "ecs/IFullECS.h"
#include "ecs/component/PhysicsComponent.h"

#include "gamethread/GameThreadTasks.h"

#include "time/TimeOfDayManager.h"

#include "rendering/renderstate/GameRenderStateManager.h"


GameThread* GameThread::sInstance = nullptr;

GameThread::GameThread(World& world, WorldNetMode worldType) : mWorld(world), mNetMode(worldType) {
    assert(!mThread); // No double init
    if (!mThread) {
        mThread = std::make_unique<std::thread>(&GameThread::mainFunc, this);
    }
}

GameThread::~GameThread() {
    if (mThread) {
        mStop.store(true);
        mThread->join();
    }
}

GameThread& GameThread::initInstance(World& world, WorldNetMode worldType) {
    if (!sInstance) {
        sInstance = new GameThread(world, worldType);
    }
    return *sInstance;
}

GameThread& GameThread::getInstance() {
    assert(sInstance);
    return *sInstance;
}

void GameThread::destroyInstance() {
    delete sInstance;
    sInstance = nullptr;
}

void GameThread::setActiveEditorWorld(World* editorWorld)
{
    {
        std::lock_guard lock(mActiveEditorWorldMutex);
        mActiveEditorWorld = editorWorld;
        if (GameRenderStateManager::exists()) {
            if (mActiveEditorWorld) {
                GameRenderStateManager::getInstance().setActiveWorld(mActiveEditorWorld);
            }
            else {
                GameRenderStateManager::getInstance().setActiveWorld(&mWorld);
            }
        }
    }
}

void GameThread::updateAllProcs() {
    ASSERT_GAME_THREAD();
    do {
        updateProcs();
    } while (GameThreadTasks::getInstance().mGameThreadFuncProcs.size_approx());
}

void GameThread::mainFunc() {

    GAME_THREAD_ID = std::this_thread::get_id();
    setThreadName("Game");
    setThreadPriorityToMax();

    initWorld();

    // Init time
    TimestepManager& TimestepManager = Services::TimestepManager::ref();
    TimestepManager.init(1.0 / SERVER_TICK_RATE_HZ);

    // TODO: Load world data

    // World can begin
    mIsRunning = true;
    while (!mStop.load()) {
        PROFILE_SCOPE("GameLoop");
        // Fixed timestep
        f64 sleepSec = 0.0;
        if (TimestepManager.tryTick(&sleepSec)) {
            mThreadUtilizationTimer.beginFrame();
            tick();
            // Force a thread switch if anyone is waiting
            // // TODO: Profile if this matters
            // yojimbo_sleep(0);
        }
        else {
            // Try updating queues with sleepSec as a time budget?
            mThreadUtilizationTimer.beginSleep();
            yojimbo_sleep(sleepSec);
            mThreadUtilizationTimer.endSleep();
        }
    }

}

void GameThread::tick() {

    updateProcs();
    // If a world is shutting down, handle it here
    if (World* destroyedWorld = WorldDestroyer::gameThreadUpdate()) {
        if (destroyedWorld == &mWorld) {
            LOG_CRITICAL("Destroyed main game world");
        }
    }

    // Update functions
    switch (mNetMode) {
        case WorldNetMode::Client:
            tickClient();
            break;
        case WorldNetMode::Host:
            tickHost();
            break;
        default:
            assert(false);
            break;
    }

    {
        std::lock_guard lock(mActiveEditorWorldMutex);
        if (mActiveEditorWorld) {
            const f64 timeStep = Services::TimestepManager::ref().getTimestepSec();
            mActiveEditorWorld->tick(timeStep);
        }
    }
}

void GameThread::tickClient() {
    PROFILE_FUNCTION();

    // Update client
    GameClient& client = GameClient::getInstance();
    if (!client.isConnected()) {
        pError("LOST CONNECTION!");
        assert(false);
    }
    client.setActiveWorld(&mWorld);
    const f64 timeStep = Services::TimestepManager::ref().getTimestepSec();
    client.update(timeStep);

    mWorld.tick(timeStep);

    //// Update editors
    //UIContext::getInstance().updateEditors(mCameraController->getOwnedCamera());

    //updateTilePicking();

    //cliWorld->frameUpdate(mCameraController->getOwnedCamera(), (f32)gameTime.elapsedSec);
}

void GameThread::tickHost() {
    PROFILE_FUNCTION();

    // TODO: We need to send packets at the end of the tick! We will accrue packets and we dont want to delay an entire frame
    if (GameServer::exists()) {
        GameServer::getInstance().tryTick();
    }

    // Update world
    mWorld.tick(Services::TimestepManager::ref().getTimestepSec());

    // Update editors
    /*UIContext::getInstance().updateEditors(mCameraController->getOwnedCamera());

    updateTilePicking();*/

    // hostWorld->frameUpdate(mCameraController->getOwnedCamera(), (f32)gameTime.elapsedSec);
}

void GameThread::updateProcs()
{
    PROFILE_FUNCTION();
    GameThreadTasks::getInstance().updateMainThread();
}

void GameThread::initWorld()
{
    //// Create player if hosting
    //f32v3 playerPos(WorldData::WORLD_CENTER.x, WorldData::WORLD_CENTER.y, 20.0f);
    //if (mWorldType == WorldType::HOST) {
    //    //mWorld->getHeightmapGrid().tryComputeHeightAtPoint(playerPos, &playerPos.z);
    //    SrvEntityComponentSystem& ecs = (SrvEntityComponentSystem&)mWorld->getECS();
    //    ecs.setLocalPlayer(ecs.createPlayerEntity(CLIENT_INDEX_HOST, playerPos));

    //   // initCamera();
    //}

    // Preload
    //displayLoadScreen("Loading...", true);

    // Starting time of day to noon
    mWorld.getTimeOfDayManager().setTimeOfDay(12.0f);

    // Begin world
    // TODO: Better pos?
    mWorld.onWorldBeginGame(mWorld.getWorldCenter());

    // Start world rendering
    //mRenderContext->onWorldBegin();

    // Hacky
    //{
    //    ScopedTimer timer("Main thread preload hack");
    //    update(gameTime);
    //    while (Services::Threadpool::ref().getTasksSizeApprox()) {
    //        Sleep(1);
    //        update(gameTime);
    //        mRenderContext->updateMeshManagers(sWorld->getLoadCenter(), true /*forceUpdate*/);
    //    }
    //}
}
