#pragma once

#include "network/WorldType.h"
#include "time/GameTimeManager.h"

#include "util/Timing/ThreadUtilizationTimer.h"

class GameThread
{
protected:
    GameThread(WorldType worldType);
    ~GameThread();

public:
    GameThread(GameThread& other) = delete;
    void operator=(const GameThread&) = delete;

    static GameThread& initInstance(WorldType worldType);
    static GameThread& getInstance();
    static void destroyInstance();
    static bool exists() { return sInstance != nullptr; }
    
    bool isRunning() const { return mIsRunning.load(); }

    const ThreadUtilizationTimer& getThreadUtilizationTimer() const { return mThreadUtilizationTimer; }

private:
    void mainFunc();
    void tick();
    void tickClient();
    void tickHost();
    void updateTimeOfDay();
    void updateProcs();
    void initWorld();

    std::atomic_bool mIsRunning = false;
    std::atomic_bool mStop = false;
    std::unique_ptr<std::thread> mThread;
    WorldType mWorldType;

    ThreadUtilizationTimer mThreadUtilizationTimer;

    static GameThread* sInstance;
};

// TODO: Move this
inline TimeStampSec getCurrentTimeStamp() {
    return Services::GameTimeManager::ref().getCurrentTimeSec();
}
