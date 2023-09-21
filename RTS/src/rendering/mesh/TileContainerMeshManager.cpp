#include "stdafx.h"
#include "TileContainerMeshManager.h"

#include "rendering/mesh/mesher/ChunkMesher.h"
#include "rendering/mesh/mesher/BuildingMesher.h"
#include "rendering/mesh/mesher/builder/ContainerMeshBuilders.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/renderdata/WorldRenderDataManager.h"
#include "rendering/RenderContext.h"

#include "resources/MaterialRepository.h"
#include "resources/asset/AssetHandleBundle.h"

#include "world/IWorld.h"

#include "tile/TileContainerRepository.h"

#include "rendering/model/InstancedStaticModelManager.h"

#include <boost/pool/singleton_pool.hpp>
#include "rendering/tasks/MeshTask.inl"

TileContainerMeshManager::TileContainerMeshManager(IWorld& world, InstancedStaticModelManager& instancedStaticModelManager) : mInstancedStaticModelManager(instancedStaticModelManager) {

    mBuildingMesher = std::make_unique<BuildingMesher>(*this);
    mChunkMesher = std::make_unique<ChunkMesher>(*this);

    initEventHandlers(world);
}

TileContainerMeshManager::~TileContainerMeshManager()
{
}

void TileContainerMeshManager::frameUpdate()
{
    PROFILE_FUNCTION();

    constexpr ui32 BULK_DEQUEUE_SIZE = 512;
    TileContainerID containersToRemove[BULK_DEQUEUE_SIZE];

    if (size_t count = mTileContainersToRemove.try_dequeue_bulk(containersToRemove, BULK_DEQUEUE_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            TileContainerID containerId = containersToRemove[i];
            auto&& it = mTileContainerMeshData.find(containerId);
            if (it != mTileContainerMeshData.end()) {
                TileContainerMeshData& meshData = it->second;
                removeMeshesForData(meshData);

                // Notify instanced models to be removed
                mInstancedStaticModelManager.removeInstancesFromContainer(containerId);

                mTileContainerMeshData.erase(it);
            }
        }
    }
}

void TileContainerMeshManager::updateMeshFromBuilders(const TileContainer* containerToMesh, ContainerMeshBuilders&& builders) {
    assert(!IS_RENDER_THREAD());

    // Dependencies

    MeshTaskData* taskData =
        new MeshTaskData(
            std::move(builders)
        );

    //assert(containerToMesh->getState() == TileContainerState::WAITING_MESH_AND_PHYSICS);
    // Pass result to the render thread
    RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* meshTaskData) {
        PROFILE_SCOPE("TileContainerRenderer::updateMeshFromBuilders");

        MeshTaskData* taskData = static_cast<MeshTaskData*>(meshTaskData);
        const TileContainer& tileContainer = taskData->builders.container;
        const TileContainerID id = tileContainer.getId();
        IWorld& world = tileContainer.getWorld();
        WorldRenderDataManager& renderDataManager = context.getRenderDataManagerForWorld(world);
        TileContainerMeshManager& meshManager = renderDataManager.getTileContainerMeshManager();
        TileContainerMeshData& meshData = meshManager.getMeshDataForTileContainer(id);
        InstancedStaticModelManager& instancedModelManager = renderDataManager.getInstancedStaticModelManager();

        // Load dependencies
        std::unique_ptr<AssetHandleBundle> dependencies;
        if (taskData->builders.materialDependencies.size()) {
            dependencies = std::make_unique<AssetHandleBundle>();
            for (MaterialID id : taskData->builders.materialDependencies) {
                dependencies->addAssetHandle(MaterialRepository::get().getAssetHandle((AssetID)id));
            }
        }

        // Remove existing meshes
        meshManager.removeMeshesForData(meshData);

        // Upload mesh buffers
        const i32v3& worldPos3D = tileContainer.getTileSpatialGrid().getWorldPos3D();
        taskData->builders.staticBuilder.finishMesh(meshData.mStaticMesh, worldPos3D);
        taskData->builders.dynamicBuilder.finishMesh(meshData.mDynamicMesh, worldPos3D);
        taskData->builders.billboardBuilder.finishMesh(meshData.mBillboardMesh, worldPos3D, 0 /*bufferFlags*/);

        // Static
        if (meshData.mStaticMesh) {
            meshManager.addStaticMesh(meshData.mStaticMesh.get());
        }

        // Dynamic
        if (meshData.mDynamicMesh) {
            meshManager.addDynamicMesh(meshData.mDynamicMesh.get());
        }

        // Billboard
        if (meshData.mBillboardMesh) {
            meshManager.addBillboardMesh(meshData.mBillboardMesh.get());
        }

        // Model instances
        instancedModelManager.addInstancesFromGatherer(taskData->builders.modelGatherer);

        // Release old dependencies and store new
        meshData.mAssetDependencies.swap(dependencies);

        // Release
        tileContainer.setDidInitMesh();
        tileContainer.decRef();

        delete taskData;
    }, taskData);
}

void TileContainerMeshManager::removeMeshesForData(TileContainerMeshData& meshData)
{
    if (meshData.mStaticMesh != nullptr) {
        mStaticMeshes.erase(meshData.mStaticMesh.get());
        meshData.mStaticMesh.reset();
    }
    if (meshData.mDynamicMesh != nullptr) {
        mDynamicMeshes.erase(meshData.mDynamicMesh.get());
        meshData.mDynamicMesh.reset();
    }
    if (meshData.mBillboardMesh != nullptr) {
        mBillboardMeshes.erase(meshData.mBillboardMesh.get());
        meshData.mBillboardMesh.reset();
    }
}

void TileContainerMeshManager::initEventHandlers(IWorld& world) {

    TileContainerRepository& tileContainerRepository = world.getTileContainerRepository();
    tileContainerRepository.registerTileContainerListeners(mTileContainerListeners);

    tileContainerRepository.addLoadFinishedListener(mTileContainerListeners, [this](const TileContainerEvent& containerEvent) {
        updateTileContainerMesh(*containerEvent.container);
    });

    tileContainerRepository.addEditTilesListener(mTileContainerListeners, [this](const TileContainerEvent& containerEvent) {
        updateTileContainerMesh(*containerEvent.container);
        //mInstancedStaticModelRenderer.onContainerEditEvent(containerEvent);
    });

    tileContainerRepository.addTileDamagedListener(mTileContainerListeners, [this](const TileContainerEvent& containerEvent) {
        mInstancedStaticModelManager.onTileDamagedEvent(containerEvent);
    });

    tileContainerRepository.addDestroyListener(mTileContainerListeners, [this](const TileContainerEvent& containerEvent) {
        assert(containerEvent.container->getRefCount() == 0); // It must not be in a mesher task
        mTileContainersToRemove.enqueue(containerEvent.container->getId());
    });
}

void TileContainerMeshManager::updateTileContainerMesh(TileContainer& tileContainer)
{
    switch (tileContainer.getOwnerType()) {
        case TileContainerOwnerType::CHUNK:
            mChunkMesher->initMeshAndPhysicsAsync(tileContainer);
            break;
        case TileContainerOwnerType::BUILDING:
            mBuildingMesher->buildMeshAndPhysicsAsync(*tileContainer.getOwnerBuilding());
            break;
        default:
            assert(false);
            break;

    }
    static_assert(e_cast(TileContainerOwnerType::COUNT) == 2);
}
