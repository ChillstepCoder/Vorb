#pragma once

typedef void(*GameFunction)(class GameThread& gameThread, void*);

class GameThreadTasks
{
    friend class GameThread;
protected:
    GameThreadTasks();
    ~GameThreadTasks();

public:
    GameThreadTasks(GameThreadTasks& other) = delete;
    void operator=(const GameThreadTasks&) = delete;

protected:
    static GameThreadTasks& initInstance();
public:
    static GameThreadTasks& getInstance();

    // Tasks
    void addGenericTask(GameFunction func, void* data) { mGameThreadProcs.enqueue(std::make_pair(func, data)); }
    void addCameraPickTeleportTask(const f32v3& camPos, const f32v3& camDir);
    void addCameraPickInteractTask(const f32v3& camPos, const f32v3& camDir);

    size_t getQueuedProcsApprox() const { return mGameThreadProcs.size_approx(); }

private:
    // Task queue
    // TODO: Clear task queues on destroy?
    moodycamel::ConcurrentQueue<std::pair<GameFunction, void*>> mGameThreadProcs;

    static GameThreadTasks* sInstance;
};

