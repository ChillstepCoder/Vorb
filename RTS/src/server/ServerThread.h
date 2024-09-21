#pragma once

class World;
class GameServerNew;

#include <Vorb/concurrentqueue.h>

#include "util/Timing/ThreadUtilizationTimer.h"

// Runs on host only (Or dedicated server when/if we support that)
class ServerThread {
public:
    ServerThread(GameServerNew& gameServer, World& world);
    ~ServerThread();

    VORB_NON_COPYABLE(ServerThread);

    static const ServerThread* tryGetInstance();
    // Not thread safe on shutdown
    const ThreadUtilizationTimer& getThreadUtilizationTimer() const { return mThreadUtilizationTimer; }

    void addTask(std::function<void()> task) {
        mTasks.enqueue(std::move(task));
    }
    void addTask(moodycamel::ProducerToken& token, std::function<void()> task) {
        mTasks.enqueue(token, std::move(task));
    }

    std::unique_ptr<moodycamel::ProducerToken> getNewProducerToken() {
        return std::make_unique<moodycamel::ProducerToken>(mTasks);
    }

protected:
    void serverThreadFunc();
    void tick();

    std::atomic_bool mStop = false;
    std::unique_ptr<std::thread> mThread;

    moodycamel::ConcurrentQueue<std::function<void()>> mTasks; ///< Contains functions to run on main thread after complete
    moodycamel::ConsumerToken mToken;

    GameServerNew& mGameServer;
    World& mWorld;
    ThreadUtilizationTimer mThreadUtilizationTimer;
};

