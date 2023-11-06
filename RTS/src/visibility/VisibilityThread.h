#pragma once

#include <Vorb/concurrentqueue.h>
#include <Vorb/blockingconcurrentqueue.h>

class World;

class VisibilityThread
{
public:
    VisibilityThread();
    ~VisibilityThread();

    void init(World& world);
    void mainThreadUpdate();

private:
    void initEventHandlers();

    //moodycamel::BlockingConcurrentQueue<NavThreadPathArgs> mPathTasks; ///< Holds tasks to execute
   // moodycamel::ConcurrentQueue<std::function<void()>> mMainThreadProcs; ///< Contains functions to run on main thread after complete

    World* mWorld = nullptr;
};