#pragma once

#include <Vorb/concurrentqueue.h>
#include "util/Timing/ThreadUtilizationTimer.h"
#include "time/TimestepManager.h"

class World;
class HostSimContext;
class RandomGenerator;

constexpr f64 SIM_TICK_RATE_MS = 64.0;

enum class SimThreadState : ui8 {
    Idle,
    HistorySim,
    GameSim
};

class SimThread {
public:
    SimThread(HostSimContext& simContext, World& world);
    ~SimThread();

    void start();
    void join();

    void setState(SimThreadState state) { mState = state; }
    void setTargetTickRateMs(f32 targetTickRateMs) { mTargetTickRateMs = targetTickRateMs; }
    void setTimeScale(f32 timeScale) { mTimeScale = timeScale; }

    size_t getTasksSizeApprox() const { return mSimThreadProcs.size_approx(); }
    void addTask(std::function<void()> task) { mSimThreadProcs.enqueue(task); }

    const ThreadUtilizationTimer& getThreadUtilizationTimer() const { return mThreadUtilizationTimer; }
    RandomGenerator& getRandomGenerator() { ASSERT_SIM_THREAD(); return *mRandomGenerator; }
private:
    void simThreadFunc();
    void tickSim(SimThreadState state);

    World& mWorld;
    HostSimContext& mHostSimContext;
    TickingTimer mSimTimer = TickingTimer(SIM_TICK_RATE_MS, SIM_TICK_RATE_MS * 2.0);
    ThreadUtilizationTimer mThreadUtilizationTimer;
    TimestepManager mTimestepManager;

    std::atomic_bool mStop = false;
    std::atomic<SimThreadState> mState = SimThreadState::Idle;
    std::atomic<f32> mTargetTickRateMs = 60.0f;
    std::atomic<f32> mTimeScale = 1.0f;
    std::unique_ptr<std::thread> mThread;
    std::unique_ptr<RandomGenerator> mRandomGenerator;

    moodycamel::ConcurrentQueue<std::function<void()>> mSimThreadProcs;
};

