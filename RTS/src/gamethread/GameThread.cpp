#include "stdafx.h"
#include "GameThread.h"

#include "world/cli/CliWorld.h"
#include "world/host/HostWorld.h"

#include "network/cli/GameClient.h"
#include "network/srv/GameServer.h"

#include "ecs/IEntityComponentSystem.h"
#include "ecs/component/PhysicsComponent.h"

#include "gamethread/GameThreadTasks.h"



GameThread* GameThread::sInstance = nullptr;

GameThread::GameThread(WorldType worldType) : mWorldType(worldType) {
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

GameThread& GameThread::initInstance(WorldType worldType) {
    if (!sInstance) {
        sInstance = new GameThread(worldType);
        GameThreadTasks::initInstance();
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

void GameThread::mainFunc() {

    GAME_THREAD_ID = std::this_thread::get_id();
    setThreadName("Game");
    setThreadPriorityToMax();

    initWorld();

    // Init time
    mTimeManager.init(1.0 / SERVER_TICK_RATE_HZ);

    // TODO: Load world data

    // World can begin
    mIsRunning = true;
    while (!mStop.load()) {
        PROFILE_SCOPE("GameLoop");
        // Fixed timestep
        f64 sleepSec = 0.0;
        if (mTimeManager.tryTick(&sleepSec)) {
            tick();
            // Force a thread switch if anyone is waiting
            // // TODO: Profile if this matters
            // yojimbo_sleep(0);
        }
        else {
            // Try updating queues with sleepSec as a time budget?
            yojimbo_sleep(sleepSec);
        }
    }

}

void GameThread::tick() {

    updateTimeOfDay();
    updateProcs();

    // Update functions
    switch (mWorldType) {
        case WorldType::CLIENT:
            tickClient();
            break;
        case WorldType::HOST:
            tickHost();
            break;
        default:
            assert(false);
            break;
    }
}

void GameThread::tickClient() {
    PROFILE_FUNCTION();
    CliWorld* cliWorld = static_cast<CliWorld*>(sWorld);

    // Update main thread update queues
    cliWorld->onFrameBegin();

    // Update client
    GameClient& client = GameClient::getInstance();
    if (!client.isConnected()) {
        pError("LOST CONNECTION!");
        assert(false);
    }
    client.update(mTimeManager.getTimestep());

    cliWorld->tick(mTimeManager.getTimestep());

    //// Update editors
    //UIContext::getInstance().updateEditors(mCameraController->getOwnedCamera());

    //updateTilePicking();

    //cliWorld->frameUpdate(mCameraController->getOwnedCamera(), (f32)gameTime.elapsedSec);
}

void GameThread::tickHost() {
    PROFILE_FUNCTION();
    HostWorld* hostWorld = static_cast<HostWorld*>(sWorld);

    // TODO: We need to send packets at the end of the tick! We will accrue packets and we dont want to delay an entire frame
    if (GameServer::exists()) {
        GameServer::getInstance().tryTick();
    }

    // Update world
    hostWorld->tick(mTimeManager.getTimestep());

    // Update editors
    /*UIContext::getInstance().updateEditors(mCameraController->getOwnedCamera());

    updateTilePicking();*/

    // hostWorld->frameUpdate(mCameraController->getOwnedCamera(), (f32)gameTime.elapsedSec);
}

void GameThread::updateTimeOfDay() {

    //mGameTimer.startFrame();

    // DEBUG Time advance
    /*static constexpr float TIME_ADVANCE_MULT = 4.0f;
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_LEFT)) {
        if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
            sDebugOptions.mTimeOffset -= gameTime.elapsedSec * 250.0f;
        }
        else {
            sDebugOptions.mTimeOffset -= gameTime.elapsedSec * TIME_ADVANCE_MULT;
        }
        mGameTimer.setMsPerTick(MS_PER_GAME_TICK / 2.0f);
    }
    else if (vui::InputDispatcher::key.isKeyPressed(VKEY_RIGHT)) {
        if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
            sDebugOptions.mTimeOffset += gameTime.elapsedSec * 250.0f;
        }
        else {
            sDebugOptions.mTimeOffset += gameTime.elapsedSec * TIME_ADVANCE_MULT;
        }
        mGameTimer.setMsPerTick(MS_PER_GAME_TICK / 2.0f);
    }
    else {
        mGameTimer.setMsPerTick(MS_PER_GAME_TICK);
    }*/
}

void GameThread::updateProcs()
{
    PROFILE_FUNCTION();
    constexpr ui32 BULK_DEQUEUE_SIZE = 16;
    std::pair<GameFunction, void*> procs[BULK_DEQUEUE_SIZE];
    PreciseTimer timer;
    // TODO: Use optik for profiling
    if (const size_t count = GameThreadTasks::getInstance().mGameThreadProcs.try_dequeue_bulk(procs, BULK_DEQUEUE_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            procs[i].first(*this, procs[i].second);
        }
    }
    if (timer.stop() > 20.0f) {
        std::cout << timer.stop() << " ms *** RENDER SPIKE WARNING ***\n";
    }
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
    sWorld->setTimeOfDay(12.0f);

    // Begin world
    // TODO: Better pos?
    sWorld->onWorldBegin(WorldData::DEFAULT_PLAYER_SPAWN);

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
