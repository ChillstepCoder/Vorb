#pragma once

class StaticPhysicsMeshBuilder;
class TileContainer;
class World;

typedef std::function<void()> GameFunction;

class GameThreadTasks
{
    friend class GameThread;
private:
    GameThreadTasks();
public:
    GameThreadTasks(GameThreadTasks& other) = delete;
    void operator=(const GameThreadTasks&) = delete;

    static GameThreadTasks& getInstance();

    void updateMainThread();

    // Tasks
    void addGenericTask(GameFunction func) { mGameThreadFuncProcs.enqueue(std::move(func)); }
    void addCameraPickTeleportTask(World& world, f32v3 camPos, f32v3 camDir);
    void addHideLocalPlayerModelTask(World& world, bool hide);
    void addTileContainerStaticPhysicsMeshUpdateTask(const TileContainer* container, StaticPhysicsMeshBuilder&& meshBuilder);
    void addEntityCreateTask(World& world, const f32v3& pos, StrToken typeToken, bool shouldReplicate);

    size_t getQueuedProcsApprox() const { return mGameThreadFuncProcs.size_approx(); }

private:

    // Task queue
    // TODO: Clear task queues on destroy?
    //  TODO: Priority queues? One high priority queue can be  exhausted faster  than  lower  priority queues, use for input and such
    // Many queues can make up a "Scheduler"  which can try to balance thread time?
    moodycamel::ConcurrentQueue<GameFunction> mGameThreadFuncProcs;
    moodycamel::ConsumerToken mToken;
};

