#pragma once

#include "util/StrToken.h"

class StaticPhysicsMeshBuilder;
class TileContainer;
class IWorld;

typedef void(*GameFunction)(class GameThread& gameThread, void*);
typedef std::function<void(GameThread&, void*)> GameFunctionWithCapture;

class GameThreadTasks
{
    friend class GameThread;
protected:
    GameThreadTasks(IWorld& mainGameWorld);
    ~GameThreadTasks();

public:
    GameThreadTasks(GameThreadTasks& other) = delete;
    void operator=(const GameThreadTasks&) = delete;

protected:
    static GameThreadTasks& initInstance(IWorld& world);
public:
    static GameThreadTasks& getInstance();
    static bool exists();

    // Tasks
    void addGenericTask(GameFunction func, void* data) { mGameThreadProcs.enqueue(std::make_pair(func, data)); }
    void addGenericTaskWithCapture(GameFunctionWithCapture func, void* data) { mGameThreadFuncProcs.enqueue(std::make_pair(func, data)); }
    void addCameraPickTeleportTask(const f32v3& camPos, const f32v3& camDir);
    void addHideLocalPlayerModelTask(bool hide);
    void addTileContainerStaticPhysicsMeshInitTask(const TileContainer* container, StaticPhysicsMeshBuilder&& meshBuilder);
    void addEntityCreateTask(const f32v3& pos, StrToken typeToken, bool shouldReplicate);
    void setActiveEditorWorld(IWorld* editorWorld);

    size_t getQueuedProcsApprox() const { return mGameThreadProcs.size_approx() + mGameThreadFuncProcs.size_approx(); }

private:
    // Task queue
    // TODO: Clear task queues on destroy?
    //  TODO: Priority queues? One high priority queue can be  exhausted faster  than  lower  priority queues, use for input and such
    // Many queues can make up a "Scheduler"  which can try to balance thread time?
    moodycamel::ConcurrentQueue<std::pair<GameFunction, void*>> mGameThreadProcs;
    moodycamel::ConcurrentQueue<std::pair<GameFunctionWithCapture, void*>> mGameThreadFuncProcs;
    IWorld& mMainGameWorld;

    static GameThreadTasks* sInstance;
};

