#include "stdafx.h"
#include "RenderThreadTasks.h"

#include "rendering/RenderContext.h"
#include "rendering/mesh/mesher/builder/TileMeshBuilderMethods.h"
#include "rendering/mesh/mesher/builder/BillboardMeshBuilder.h"
#include "rendering/CharacterRenderer.h"
#include "rendering/model/InstancedStaticModelGatherer.h"

#include "tile/TileContainer.h"

#include <boost/pool/singleton_pool.hpp>

void RenderThreadTasks::processRenderThread(RenderContext& context){
    ASSERT_RENDER_THREAD();

    // TODO: We could bulk dequeue more, and then
    // process them in a loop until we hit a time limit,
    // and hold onto the unprocessed ones until the next frame
    constexpr int BULK_DEQUEUE_SIZE = 4;
    std::pair<RenderFunction, void*> procs[BULK_DEQUEUE_SIZE];

    constexpr f32 MAX_PROCESS_TIME_MS = 8.0f;
    PreciseTimer timer;
    // TODO: Use optik for profiling?
    do {
        if (size_t count = mRenderThreadProcs.try_dequeue_bulk(mToken, procs, BULK_DEQUEUE_SIZE)) {
            for (size_t i = 0; i < count; ++i) {
                procs[i].first(context, procs[i].second);
            }
        }
        else {
            break;
        }
    } while (timer.stop() < MAX_PROCESS_TIME_MS);

    if (timer.stop() > 16.0f) {
        std::cout << timer.stop() << " ms *** RENDER SPIKE WARNING ***\n";
    }
    checkGlError("RenderThreadTasks::processRenderThread");
}

RenderThreadTasks* RenderThreadTasks::sInstance = nullptr;

RenderThreadTasks::RenderThreadTasks() : mToken(mRenderThreadProcs) {

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
