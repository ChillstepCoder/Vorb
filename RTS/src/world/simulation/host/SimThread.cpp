#include "stdafx.h"
#include "SimThread.h"

#include "world/World.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/SimImmigrationManager.h"

#include "options/DebugOptions.h"

#include "world/simulation/host/component/SimCharacterComponents.h"

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

void SimThread::requestAllCharacters(std::shared_ptr<SimThreadEntityRequest> request) {
    request->filled = false;
    request->entities.resize(0);
    mSimThreadProcs.enqueue([this, request]() {
        SimECS& ecs = *mHostSimContext.mSimECS;
        entt::registry& registry = ecs.getRegistrySimThread();
        auto viewGroup = registry.view<DualCharacterComponent, SimPositionComponent, FactionComponent>();
        request->entities.reserve(viewGroup.size_hint());
        for (auto entity : viewGroup) {
            request->entities.emplace_back(SimThreadEntityRequest::Data{
                    entity,
                    registry.get<SimPositionComponent>(entity).getPosition(),
                    registry.get<FactionComponent>(entity).factionId
                }
            );
        }

        request->filled = true;
    });
}

void SimThread::simThreadFunc() {
    SIM_THREAD_ID = std::this_thread::get_id();
    setThreadName("Sim");
    mTimestepManager.init(mTargetTickRateMs / MS_PER_SECOND);

    while (!mStop.load()) {
        mThreadUtilizationTimer.beginFrame();

        updateTasks();

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
    mTimestepManager.setTargetTimestepSec((mTargetTickRateMs / MS_PER_SECOND) * sDebugOptions.mGlobalSimTimestepMult);

    {
        f64 sleepSec = 0.0f;
        if (!mTimestepManager.tryTick(&sleepSec)) {
            mThreadUtilizationTimer.beginSleep();
            // Don't sleep too long so we can poll task queue
            Sleep(glm::min(DWORD(sleepSec * MS_PER_SECOND), DWORD(SIM_THREAD_IDLE_SLEEP_MS)));
            mThreadUtilizationTimer.endSleep();
            return;
        }
    }

    // Skip the sleep
    PROFILE_FUNCTION();
    switch (state) {
        case SimThreadState::HistorySim:
        case SimThreadState::GameSim:
            // TODO: Grab the world time instead of incrementing sim time?
            mHostSimContext.mSimTime += ui64(mTargetTickRateMs * mTimeScale * sDebugOptions.mGlobalSimTimescale);
            break;
        default:
            assert(mTimeScale == 1.0f);
            break;
    }


    //LOG_TRACE("Sim step starting at {} seconds", (f64)mHostSimContext.mSimTime / MS_PER_SECOND);
    mHostSimContext.mSimECS->tickSimThread(mHostSimContext.mSimTime);

    mHostSimContext.mImmigrationManager->tickSimThread(mHostSimContext.mSimTime);

    mHostSimContext.mEntityTransitionManager->tickSimThread(mHostSimContext.mSimTime);

    //LOG_TRACE(" Sim thread {} ms", mThreadUtilizationTimer.getFrameTimeMS());
}

void SimThread::updateTasks() {
    PROFILE_FUNCTION();
    constexpr size_t BULK_DEQUEUE_COUNT = 256;
    std::function<void()> funcs[BULK_DEQUEUE_COUNT];

    if (size_t count = mSimThreadProcs.try_dequeue_bulk(funcs, BULK_DEQUEUE_COUNT)) {
        for (size_t i = 0; i < count; ++i) {
            funcs[i]();
        }
    }

}
