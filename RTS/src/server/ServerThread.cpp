#include "stdafx.h"
#include "ServerThread.h"

#include "time/TimestepManager.h"
#include "server/GameServerNew.h"
#include "server/ServerReportStateManager.h"

constexpr int SERVER_TICK_RATE = 60;

static ServerThread* sInstance = nullptr;

ServerThread::ServerThread(GameServerNew& gameServer, World& world) :
    mGameServer(gameServer),
    mWorld(world),
    mToken(mTasks)
{
    assert(!sInstance); // No double init
    sInstance = this;
    mThread = std::make_unique<std::thread>(&ServerThread::serverThreadFunc, this);
}

ServerThread::~ServerThread() {
    sInstance = nullptr;
    mStop = true;
    mThread->join();
}

const ServerThread* ServerThread::tryGetInstance() {
    return sInstance;
}

void ServerThread::serverThreadFunc() {
    setThreadName("Server");
    setThreadPriorityToMax();
    SERVER_THREAD_ID = std::this_thread::get_id();

    constexpr ui32 BULK_DEQUEUE_SIZE = 128;

    TimestepManager timestepManager;
    timestepManager.init((f32)1.0f / SERVER_TICK_RATE, 1);
    std::function<void()> procs[BULK_DEQUEUE_SIZE];

    TickingTimer timer((f32)MS_PER_SECOND / SERVER_TICK_RATE);
    while (!mStop) {
        PROFILE_SCOPE("ServerLoop");

        if (const size_t count = mTasks.try_dequeue_bulk(mToken, procs, BULK_DEQUEUE_SIZE)) {
            for (size_t i = 0; i < count; ++i) {
                procs[i]();
            }
        }

        // Fixed timestep
        f64 sleepSec = 0.0;
        if (timestepManager.tryTick(&sleepSec)) {
            mThreadUtilizationTimer.beginFrame();
            tick();
            std::this_thread::yield();
        }
        else {
            // Try updating queues with sleepSec as a time budget?
            mThreadUtilizationTimer.beginSleep();
            Sleep(sleepSec * MS_PER_SECOND);
            mThreadUtilizationTimer.endSleep();
        }
    }
}

void ServerThread::tick() {
    PROFILE_FUNCTION();
    mGameServer.mPlayerManager->tick();
}
