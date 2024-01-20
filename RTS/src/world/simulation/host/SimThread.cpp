#include "stdafx.h"
#include "SimThread.h"

#include "world/World.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/SimImmigrationManager.h"

#include "math/Random.h"

constexpr int SIM_THREAD_IDLE_SLEEP_MS = 60;

SimThread::SimThread(HostSimContext& simContext, World& world) : mWorld(world), mHostSimContext(simContext)
{
}

SimThread::~SimThread() {
    join();
}

void SimThread::start()
{
    assert(!mThread);
    mRandomGenerator = std::make_unique<RandomGenerator>(mWorld.getSeed());
    mThread = std::make_unique<std::thread>(&SimThread::simThreadFunc, this);
}

void SimThread::join() {
    if (mThread) {
        mStop.store(true);
        mThread->join();
        mThread.reset();
    }
}

void SimThread::simThreadFunc() {
    SIM_THREAD_ID = std::this_thread::get_id();
    setThreadName("Sim");
    mTimestepManager.init(mTargetTickRateMs / MS_PER_SECOND);

    constexpr size_t BULK_DEQUEUE_COUNT = 64;
    std::function<void()> funcs[BULK_DEQUEUE_COUNT];
    while (!mStop.load()) {
        mThreadUtilizationTimer.beginFrame();

        if (size_t count = mSimThreadProcs.try_dequeue_bulk(funcs, BULK_DEQUEUE_COUNT)) {
            for (size_t i = 0; i < count; ++i) {
                funcs[i]();
            }
        }

        const SimThreadState state = mState.load();
        switch (state) {
            case SimThreadState::Idle:
                mThreadUtilizationTimer.beginSleep();
                Sleep(SIM_THREAD_IDLE_SLEEP_MS);
                mThreadUtilizationTimer.endSleep();
                break;
            case SimThreadState::HistorySim:
            case SimThreadState::GameSim:
                tickSim(state);
                break;
            default:
                break;

        }
    }
}

void SimThread::tickSim(SimThreadState state) {
    mTimestepManager.setTargetTimestepSec(mTargetTickRateMs / MS_PER_SECOND);

    {
        f64 sleepSec = 0.0f;
        if (!mTimestepManager.tryTick(&sleepSec)) {
            Sleep(sleepSec * MS_PER_SECOND);
            return;
        }
    }


    if (state == SimThreadState::HistorySim) {
        mHostSimContext.mSimTime += mTimestepManager.getTimestepSec() * mTimeScale * MS_PER_SECOND;
    }
    else {
        assert(mTimeScale == 1.0f);
        // TODO: Grab the world time instead of incrementing sim time
    }

    LOG_TRACE("Sim step starting at {} seconds", (f64)mHostSimContext.mSimTime / MS_PER_SECOND);
    mHostSimContext.mSimECS->tickSimThread(mHostSimContext.mSimTime);

    mHostSimContext.mImmigrationManager->tickSimThread(mHostSimContext.mSimTime);
}
