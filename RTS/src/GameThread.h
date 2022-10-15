#pragma once

#include "network/WorldType.h"
#include "time/GameTimeManager.h"

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

    void addGameThreadProc(std::function<void()>&& f) { mGameThreadProcs.enqueue(std::move(f)); }

private:
    void mainFunc();
    void update();
    void updateClient();
    void updateHost();
    void updateTimeOfDay();
    void updateProcs();
    void initWorld();

    std::atomic_bool mIsRunning = false;
    std::atomic_bool mStop = false;
    std::unique_ptr<std::thread> mThread;
    WorldType mWorldType;

    GameTimeManager mTimeManager;

    moodycamel::ConcurrentQueue<std::function<void()>> mGameThreadProcs;

    static GameThread* sInstance;
};

