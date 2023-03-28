#pragma once

class TileContainer;
class ContainerMeshBuilders;
struct ModelDef;

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
    void addTileContainerMeshInitTask(
        const TileContainer* containerToMesh,
        ContainerMeshBuilders&& builders
    );
    void addCharacterModel(entt::entity characterEntity, ui32 modelId);
    void removeCharacterModel(entt::entity characterEntity);
    void playOneShotAnimation(entt::entity characterEntity, ui32 animationId);
    //void addStaticMeshFromBuilder(StaticPhysicsMeshBuilder&& meshBuilder, Mesh* mesh);
    // TODO: Add cancel logic for if we destroy the threadpool so we can free data ptr?
    void addGenericTask(RenderFunction func, void* data) { mRenderThreadProcs.enqueue(std::make_pair(func, data)); }

    size_t getQueuedProcsApprox() const { return mRenderThreadProcs.size_approx(); }

private:
    // Task queue
    // TODO: Clear task queues on destroy?
    moodycamel::ConcurrentQueue<std::pair<RenderFunction, void*>> mRenderThreadProcs;


    static RenderThreadTasks* sInstance;
};

