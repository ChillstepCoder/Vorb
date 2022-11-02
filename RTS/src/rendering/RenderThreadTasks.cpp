#include "stdafx.h"
#include "RenderThreadTasks.h"

#include "rendering/mesh/MeshBuilder.h"
#include "rendering/RenderContext.h"
#include "rendering/mesh/TileMeshBuilderMethods.h"
#include "rendering/mesh/BillboardMeshBuilder.h"
#include "rendering/CharacterRenderer.h"

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

void RenderThreadTasks::addTileContainerMeshUpdateTask(TileContainer* containerToMesh) {
    assert(IS_GAME_THREAD());
    containerToMesh->incRef();

    TileContainerRenderData& tileRenderData = containerToMesh->getRenderData();
    tileRenderData.mHasMesh = true;

    Services::Threadpool::ref().addTask([this, containerToMesh](ThreadPoolWorkerData*) {
        MeshTaskData* taskData = new MeshTaskData(containerToMesh);
        constexpr ui32 RESERVE_VERT_COUNT_STATIC = 512; // Most chunks are less than this
        TileContainer* containerToMesh = taskData->container;
        taskData->staticMeshBuilder.reserveVertexCount(RESERVE_VERT_COUNT_STATIC);
        taskData->billboardMeshBuilder.reserveBillboardCount(CHUNK_SIZE / 2);

        // ========================== Mesh Tiles ===============================
        TileMeshBuilderMethods::meshTileContainerStatic(taskData->staticMeshBuilder, &taskData->billboardMeshBuilder, *containerToMesh, nullptr /*physicsMesh*/);
        TileMeshBuilderMethods::meshTileContainerDynamic(taskData->staticMeshBuilder, *containerToMesh);

        // Pass result to the render thread
        mRenderThreadProcs.enqueue(std::make_pair([](RenderContext& context, void* meshTaskData) {
            MeshTaskData* taskData = static_cast<MeshTaskData*>(meshTaskData);
            TileContainer* tileContainer = taskData->container;
            TileContainerRenderData& tileRenderData = tileContainer->getRenderData();

            auto&& it = context.mTileContainerMeshData.find(tileContainer);
            if (it != context.mTileContainerMeshData.end()) {
                // No need to dispose previous meshes as the meshbuilder will handle it
                TileContainerMeshData& meshData = context.mTileContainerMeshData[tileContainer];

                const Mesh* prevStatic = meshData.mStaticMesh.get();
                assert(!prevStatic || prevStatic->isValid());
                const Mesh* prevDynamic = meshData.mDynamicMesh.get();
                assert(!prevDynamic || prevDynamic->isValid());
                const Mesh* prevBillboard = meshData.mBillboardMesh.get();
                assert(!prevBillboard || prevBillboard->isValid());

                // Upload mesh buffers
                taskData->staticMeshBuilder.finishMesh(meshData.mStaticMesh, MeshDrawMode::STATIC, tileContainer->getWorldPos3D());
                taskData->dynamicMeshBuilder.finishMesh(meshData.mDynamicMesh, MeshDrawMode::DYNAMIC, tileContainer->getWorldPos3D());
                taskData->billboardMeshBuilder.finishMesh(meshData.mBillboardMesh, MeshDrawMode::STATIC, tileContainer->getWorldPos3D());

                bool hadAny = false;
                // Static
                if (meshData.mStaticMesh) {
                    hadAny = true;
                    if (!prevStatic) {
                        // Only add if we arent already in the renderer
                        context.addStaticMesh(meshData.mStaticMesh.get());
                    }
                }
                else if (prevStatic) {
                    context.removeStaticMesh(prevStatic);
                }
                // Dynamic
                if (meshData.mDynamicMesh) {
                    hadAny = true;
                    if (!prevDynamic) {
                        // Only add if we arent already in the renderer
                        context.addDynamicMesh(meshData.mDynamicMesh.get());
                    }
                }
                else if (prevDynamic) {
                    context.removeDynamicMesh(prevDynamic);
                }
                // Billboard
                if (meshData.mBillboardMesh) {
                    hadAny = true;
                    if (!prevBillboard) {
                        // Only add if we arent already in the renderer
                        context.addBillboardMesh(meshData.mBillboardMesh.get());
                    }
                    else {
                        assert(prevBillboard == meshData.mBillboardMesh.get());
                    }
                }
                else if (prevBillboard) {
                    context.removeBillboardMesh(prevBillboard);
                }

                // If we no longer have any valid mesh, remove it from any render list
                if (!hadAny) {
                    context.mTileContainerMeshData.erase(tileContainer);
                }
            }
            else {
                // Creating a new mesh
                TileContainerMeshData meshData;

                // Upload mesh buffers
                taskData->staticMeshBuilder.finishMesh(meshData.mStaticMesh, MeshDrawMode::STATIC, tileContainer->getWorldPos3D());
                taskData->dynamicMeshBuilder.finishMesh(meshData.mDynamicMesh, MeshDrawMode::DYNAMIC, tileContainer->getWorldPos3D());
                taskData->billboardMeshBuilder.finishMesh(meshData.mBillboardMesh, MeshDrawMode::STATIC, tileContainer->getWorldPos3D());

                // If any mesh is valid, track for draw
                bool hadAny = false;
                if (meshData.mStaticMesh) {
                    hadAny = true;
                    context.addStaticMesh(meshData.mStaticMesh.get());
                }
                if (meshData.mDynamicMesh) {
                    hadAny = true;
                    context.addDynamicMesh(meshData.mDynamicMesh.get());
                }
                if (meshData.mBillboardMesh) {
                    hadAny = true;
                    context.addBillboardMesh(meshData.mBillboardMesh.get());
                }

                if (hadAny) {
                    context.mTileContainerMeshData[tileContainer] = std::move(meshData);
                }
            }

            // Release
            tileContainer->decRef();

            delete taskData;
        }, taskData));
    }, nullptr);
}

void RenderThreadTasks::removeTileContainerMesh(TileContainer* container) {
    TileContainerRenderData& tileRenderData = container->getRenderData();
    assert(IS_GAME_THREAD());
    if (tileRenderData.mHasMesh) {
       // if (IS_GAME_THREAD()) {
            // It is OK for the container to be nulled or re-used after we dangle this pointer, because we are only using it as an ID
            mRenderThreadProcs.enqueue(std::make_pair([](RenderContext& context, void* container) {
                TileContainer* tileContainer = static_cast<TileContainer*>(container);
                auto&& it = context.mTileContainerMeshData.find(tileContainer);
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
            }, container));
            /*}
        else {
            assert(IS_RENDER_THREAD());
            TileContainer* tileContainer = static_cast<TileContainer*>(container);
            RenderContext& context = RenderContext::getInstance();
            auto&& it = context.mTileContainerMeshData.find(tileContainer);
            if (it != context.mTileContainerMeshData.end()) {
                context.mTileContainerMeshData.erase(it);
            }
        }*/
        tileRenderData.reset();
    }
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
    LOG_CRITICAL("Animation task game side: {} {}", animationTask.first, animationTask.second);
    mRenderThreadProcs.enqueue(std::make_pair([](RenderContext& context, void* data) {
        std::pair<ui32, ui32> animationTask = *((std::pair<ui32, ui32>*)&data);
        LOG_CRITICAL("  Animation task render side: {} {}", animationTask.first, animationTask.second);
        context.getCharacterRenderer().playOneShotAnimation(entt::entity(animationTask.first), animationTask.second);
    }, (void*)(*((void**)&animationTask)))); // Black magic casting TODO: Cleaner?
}
