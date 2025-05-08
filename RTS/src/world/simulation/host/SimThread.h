#pragma once

#include <Vorb/concurrentqueue.h>
#include "util/Timing/ThreadUtilizationTimer.h"
#include "time/TimestepManager.h"

class World;
class HostSimContext;
class RandomGenerator;

enum class SimThreadState : ui8 {
    Idle,
    HistorySim,
    GameSim
};


// Gets entities from the sim thread asynchronously
struct SimThreadEntityRequest {

    struct Data {
        entt::entity entity;
        i32v2 pos;
        FactionID faction;
    };
    std::vector<Data> entities;
    std::atomic_bool filled = false;
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
    void addTask(std::function<void()> task) { mSimThreadProcs.enqueue(std::move(task)); }
    void requestAllCharacters(std::shared_ptr<SimThreadEntityRequest> request);

    const ThreadUtilizationTimer& getThreadUtilizationTimer() const { return mThreadUtilizationTimer; }
    RandomGenerator& getRandomGenerator() { ASSERT_SIM_THREAD(); return *mRandomGenerator; }
private:
    void simThreadFunc();
    void tickSim(SimThreadState state);
    void updateTasks();

    World& mWorld;
    HostSimContext& mHostSimContext;
    ThreadUtilizationTimer mThreadUtilizationTimer;
    TimestepManager mTimestepManager;

    std::atomic_bool mStop = false;
    std::atomic<SimThreadState> mState = SimThreadState::Idle;
    std::atomic<f32> mTargetTickRateMs = 60.0f;
    std::atomic<f32> mTimeScale = 1.0f;
    std::unique_ptr<std::thread> mThread;
    std::unique_ptr<RandomGenerator> mRandomGenerator;

    moodycamel::ConcurrentQueue<std::function<void()>> mSimThreadProcs;
    moodycamel::ConsumerToken mToken;
};

