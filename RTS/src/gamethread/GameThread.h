#pragma once

#include "network/WorldNetMode.h"
#include "time/TimestepManager.h"

#include "util/Timing/ThreadUtilizationTimer.h"

class World;

class GameThread
{
protected:
    GameThread(World& world, WorldNetMode worldType);
    ~GameThread();

public:
    GameThread(GameThread& other) = delete;
    void operator=(const GameThread&) = delete;

    static GameThread& initInstance(World& world, WorldNetMode worldType);
    static GameThread& getInstance();
    static void destroyInstance();
    static bool exists() { return sInstance != nullptr; }
    
    bool isRunning() const { return mIsRunning.load(); }

    const ThreadUtilizationTimer& getThreadUtilizationTimer() const { return mThreadUtilizationTimer; }

    void setActiveEditorWorld(World* editorWorld);

    void updateAllProcs();
private:
    void mainFunc();
    void tick();
    void tickClient();
    void tickHost();
    void updateProcs();
    void initWorld();

    std::mutex mActiveEditorWorldMutex;
    World* mActiveEditorWorld = nullptr;
    World& mWorld;
    std::atomic_bool mIsRunning = false;
    std::atomic_bool mStop = false;
    std::unique_ptr<std::thread> mThread;
    WorldNetMode mNetMode;

    ThreadUtilizationTimer mThreadUtilizationTimer;

    static GameThread* sInstance;
};

// TODO: Move this
inline TimeStampSec getCurrentTimeStamp() {
    return Services::TimestepManager::ref().getCurrentTimeSec();
}
