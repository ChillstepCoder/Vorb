#pragma once

// We enforce no std::function so that we can avoid heap allocations and use singleton_pool for task allocations
typedef void(*RenderFunction)(class RenderContext& context, void*);

// Singleton class that manages passing render tasks so we don't need to include RenderContext in every object
class RenderThreadTasks
{
    friend class RenderContext;
protected:
    RenderThreadTasks();
    ~RenderThreadTasks();

public:
    RenderThreadTasks(RenderThreadTasks& other) = delete;
    void operator=(const RenderThreadTasks&) = delete;

protected:
    static RenderThreadTasks& initInstance();
public:
    static RenderThreadTasks& getInstance();
    static RenderThreadTasks* tryGetInstance() { return sInstance; }
    static bool exists() { return sInstance != nullptr; }

    // Tasks
    void playOneShotAnimation(entt::entity characterEntity, ui32 animationId);
    //void addStaticMeshFromBuilder(StaticPhysicsMeshBuilder&& meshBuilder, Mesh* mesh);
    // TODO: Add cancel logic for if we destroy the threadpool so we can free data ptr?
    void addGenericTask(RenderFunction func, void* data) { mRenderThreadProcs.enqueue(std::make_pair(func, data)); }
    void addShutdownTask(std::function<void()>&& func) { mShutdownTasks.enqueue(std::move(func)); }

    size_t getQueuedProcsApprox() const { return mRenderThreadProcs.size_approx(); }
    size_t getQueuedShutdownTasksApprox() const { return mShutdownTasks.size_approx(); }

    void processShutdownTasks() {
        std::function<void()> task;
        while (mShutdownTasks.try_dequeue(task)) {
            task();
        }
    }

private:
    // Task queue
    // TODO: Clear task queues on destroy?
    // TODO: Investigate performance of https://gitlab.com/rmettler/cpp_delegates instead
    moodycamel::ConcurrentQueue<std::pair<RenderFunction, void*>> mRenderThreadProcs;
    moodycamel::ConcurrentQueue<std::function<void()>> mShutdownTasks;

    static RenderThreadTasks* sInstance;
};

