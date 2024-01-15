#pragma once

class World;
class HostSimContext;

constexpr f64 SIM_TICK_RATE_MS = 64.0;

class SimThread
{
public:
    SimThread(HostSimContext& simContext, World& world);
    ~SimThread();

    void processProcsMainThread();

private:
    void simThreadFunc();

    World& mWorld;
    HostSimContext& mSimContext;
    TickingTimer mSimTimer = TickingTimer(SIM_TICK_RATE_MS, SIM_TICK_RATE_MS * 2.0);

    moodycamel::ConcurrentQueue<std::function<void()>> mSimThreadProcs;
    moodycamel::ConcurrentQueue<std::function<void()>> mMainThreadProcs;
};

