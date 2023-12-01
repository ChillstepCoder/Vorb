#include "stdafx.h"
#include "RenderThreadTasks.h"

#include "rendering/RenderContext.h"
#include "rendering/mesh/mesher/builder/TileMeshBuilderMethods.h"
#include "rendering/mesh/mesher/builder/BillboardMeshBuilder.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/model/InstancedStaticModelGatherer.h"

#include "tile/TileContainer.h"

#include <boost/pool/singleton_pool.hpp>

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

// TODO: Move to characterRenderer?
void RenderThreadTasks::playOneShotAnimation(entt::entity characterEntity, ui32 animationId) {
    std::pair<ui32, ui32> animationTask{ e_cast(characterEntity), animationId };
    static_assert(sizeof(std::pair<ui32, ui32>) == sizeof(void*));
    mRenderThreadProcs.enqueue(std::make_pair([](RenderContext& context, void* data) {
        std::pair<ui32, ui32> animationTask = *((std::pair<ui32, ui32>*)&data);
        context.getCharacterRenderer().playOneShotAnimation(entt::entity(animationTask.first), animationTask.second);
    }, (void*)(*((void**)&animationTask)))); // Black magic casting TODO: Cleaner?
}
