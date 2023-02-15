#include "stdafx.h"
#include "RenderThreadTasks.h"

#include "rendering/mesh/mesher/builder/ContainerMeshBuilders.h"
#include "rendering/RenderContext.h"
#include "rendering/mesh/mesher/builder/TileMeshBuilderMethods.h"
#include "rendering/mesh/mesher/builder/BillboardMeshBuilder.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/model/InstancedStaticModelGatherer.h"

#include "tile/TileContainer.h"

#include <boost/pool/singleton_pool.hpp>

#include "tasks/MeshTask.inl"
#include "tasks/CharacterModelTask.inl"

RenderThreadTasks* RenderThreadTasks::sInstance = nullptr;

RenderThreadTasks::RenderThreadTasks()
{

}

RenderThreadTasks::~RenderThreadTasks()
{

}


RenderThreadTasks& RenderThreadTasks::initInstance() {
    if (!sInstance) {
        sInstance = new RenderThreadTasks();
    }
    return *sInstance;
}

RenderThreadTasks& RenderThreadTasks::getInstance()
{
    assert(sInstance);
    return *sInstance;
}

void RenderThreadTasks::addTileContainerMeshInitTask(
    const TileContainer* containerToMesh,
    ContainerMeshBuilders&& builders
) {

    MeshTaskData* taskData = 
        new MeshTaskData(
            containerToMesh,
            std::move(builders)
        );
    //assert(containerToMesh->getState() == TileContainerState::WAITING_MESH_AND_PHYSICS);
    // Pass result to the render thread
    mRenderThreadProcs.enqueue(std::make_pair([](RenderContext& context, void* meshTaskData) {
        PROFILE_SCOPE("RenderThreadTasks::AddTileContainerMeshInitTask");
        MeshTaskData* taskData = static_cast<MeshTaskData*>(meshTaskData);
        const TileContainer* tileContainer = taskData->container;
        TileContainerID id = tileContainer->getId();

        assert(context.mTileContainerMeshData.find(id) == context.mTileContainerMeshData.end());

        TileContainerMeshData& meshData = context.mTileContainerMeshData[id];
          
        // Upload mesh buffers
        taskData->builders.staticBuilder.finishMesh(meshData.mStaticMesh, tileContainer->getWorldPos3D());
        taskData->builders.dynamicBuilder.finishMesh(meshData.mDynamicMesh, tileContainer->getWorldPos3D());
        taskData->builders.billboardBuilder.finishMesh(meshData.mBillboardMesh, tileContainer->getWorldPos3D(), 0 /*bufferFlags*/);

        bool hadAny = false;
        // Static
        if (meshData.mStaticMesh) {
            hadAny = true;
            context.addStaticMesh(meshData.mStaticMesh.get());
        }

        // Dynamic
        if (meshData.mDynamicMesh) {
            hadAny = true;
            context.addDynamicMesh(meshData.mDynamicMesh.get());
        }

        // Billboard
        if (meshData.mBillboardMesh) {
            hadAny = true;
            context.addBillboardMesh(meshData.mBillboardMesh.get());
        }
        
        // Model instances
        context.addStaticModelInstancesFromGatherer(taskData->builders.modelGatherer);

        // Release
        tileContainer->setDidInitMesh();
        tileContainer->decRef();

        delete taskData;
    }, taskData));
}

void RenderThreadTasks::removeTileContainerMesh(TileContainerID id) {
    assert(IS_GAME_THREAD());
            // It is OK for the container to be nulled or re-used after we dangle this pointer, because we are only using it as an ID
    mRenderThreadProcs.enqueue(std::make_pair([](RenderContext& context, void* containerId) {
        TileContainerID id = (TileContainerID)containerId;
        auto&& it = context.mTileContainerMeshData.find(id);
        if (it != context.mTileContainerMeshData.end()) {
            if (it->second.mBillboardMesh) {
                context.removeBillboardMesh(it->second.mBillboardMesh.get());
            }
            if (it->second.mStaticMesh) {
                context.removeStaticMesh(it->second.mStaticMesh.get());
            }
            if (it->second.mDynamicMesh) {
                context.removeStaticMesh(it->second.mDynamicMesh.get());
            }
            context.mTileContainerMeshData.erase(it);
        }
    }, (void*)id));
}

void RenderThreadTasks::addCharacterModel(entt::entity characterEntity, ui32 modelId) {
    assert(IS_GAME_THREAD());
    CharacterModelTaskData* taskData = new CharacterModelTaskData();
    taskData->entityId = characterEntity;
    taskData->modelId = modelId;
    
    // TODO: maybe this should be its own queue?
    mRenderThreadProcs.enqueue(std::make_pair([](RenderContext& context, void* data) {
        CharacterModelTaskData* taskData = static_cast<CharacterModelTaskData*>(data);
        context.getCharacterRenderer().addCharacterModel(taskData->entityId, taskData->modelId);
        delete taskData;
    }, taskData));
}

void RenderThreadTasks::removeCharacterModel(entt::entity characterEntity) {
    assert(IS_GAME_THREAD());
    // TODO: maybe this should be its own queue?
    mRenderThreadProcs.enqueue(std::make_pair([](RenderContext& context, void* data) {
        entt::entity entityId = entt::entity(reinterpret_cast<entt::id_type>(data));
        context.getCharacterRenderer().removeCharacterModel(entityId);
    }, (void*)characterEntity));
}

void RenderThreadTasks::playOneShotAnimation(entt::entity characterEntity, ui32 animationId) {
    std::pair<ui32, ui32> animationTask{ e_cast(characterEntity), animationId };
    static_assert(sizeof(std::pair<ui32, ui32>) == sizeof(void*));
    mRenderThreadProcs.enqueue(std::make_pair([](RenderContext& context, void* data) {
        std::pair<ui32, ui32> animationTask = *((std::pair<ui32, ui32>*)&data);
        context.getCharacterRenderer().playOneShotAnimation(entt::entity(animationTask.first), animationTask.second);
    }, (void*)(*((void**)&animationTask)))); // Black magic casting TODO: Cleaner?
}
