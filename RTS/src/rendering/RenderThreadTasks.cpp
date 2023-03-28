#include "stdafx.h"
#include "RenderThreadTasks.h"

#include "rendering/RenderContext.h"
#include "rendering/mesh/mesher/builder/TileMeshBuilderMethods.h"
#include "rendering/mesh/mesher/builder/BillboardMeshBuilder.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/model/InstancedStaticModelGatherer.h"

#include "tile/TileContainer.h"

#include <boost/pool/singleton_pool.hpp>

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
