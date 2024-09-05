#include "stdafx.h"
#include "TileContainerMeshManager.h"

#include "rendering/mesh/mesher/ChunkMesher.h"
#include "rendering/mesh/mesher/BuildingMesher.h"
#include "rendering/mesh/mesher/builder/ContainerMeshBuilders.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/renderdata/WorldRenderDataManager.h"
#include "rendering/RenderContext.h"

#include "resources/ModelRepository.h"
#include "resources/MaterialRepository.h"
#include "resources/TileRepository.h"
#include "resources/asset/AssetHandleBundle.h"

#include "gamethread/GameThreadTasks.h"

#include "physics/TrackedStaticModelColliderGatherer.h"

#include "world/World.h"

#include "tile/TileContainerRepository.h"

#include "rendering/model/InstancedStaticModelManager.h"

#include "rendering/tasks/MeshTask.inl"

TileContainerMeshManager::TileContainerMeshManager(World& world, InstancedStaticModelManager& instancedStaticModelManager) : mInstancedStaticModelManager(instancedStaticModelManager) {
    ASSERT_GAME_THREAD(); // This is currently created on the game thread

    mBuildingMesher = std::make_unique<BuildingMesher>(*this);
    mChunkMesher = std::make_unique<ChunkMesher>(*this);

    initEventHandlers(world);
}

TileContainerMeshManager::~TileContainerMeshManager()
{
}

void TileContainerMeshManager::shutdown() {
    mTileContainerListeners.reset();
}

void TileContainerMeshManager::initModelsForTileContainer(std::span<TileIndex> modelTileIndices, const TileContainer& container) {
    // This will run on the threadpool
    assert(!IS_RENDER_THREAD() && !IS_GAME_THREAD());

    std::unique_ptr<InstancedStaticModelGatherer> gatherer = std::make_unique<InstancedStaticModelGatherer>(container.getId(), container.getWorldPos());
    std::unique_ptr<TrackedStaticModelColliderGatherer> physics = std::make_unique<TrackedStaticModelColliderGatherer>(container.getId());
    TileRepository& tileRepo = TileRepository::get();
    const TileSpatialGrid& spatialGrid = container.getTileSpatialGrid();
    const FlatMap<TileIndex, TileDamageDataPtr>& damagedTiles = container.getDamagedTiles();

    // Turn all models into meshes and physics objects
    for (const TileIndex& tileIndex : modelTileIndices) {
        const Tile& tile = container.getTileAt(tileIndex);
        const TileDef& tileDef = tileRepo.getLoadedOrUnloadedAsset(tile.getMainID());

        f32v3 worldPos = spatialGrid.getTileCenterWorldPos3D(tileIndex, tile.getGroundZOffset());
        ui8 variantIndex = 0;
        variantIndex = tileDef.modelVariants[tile.getMainLayerVariant()];

        TileDamageDataPtr damageData;
        if (tile.hasFlag(TileFlags::IS_DAMAGED)) {
            auto it = damagedTiles.find(tileIndex);
            if (it != damagedTiles.end()) {
                damageData = std::make_unique<TileDamageData>(it->second->getCurrentHealth());
            }
        }
        const f32 rotation = getTileModelRotationAtPosition(worldPos);

        const ModelDef& modelDef = ModelRepository::get().getLoadedOrUnloadedAsset(tileDef.modelId);
        f32 scale;
        if (const FloraTileData* data = std::get_if<FloraTileData>(&tile.getTypeData())) {
            scale = modelDef.getScaleFromFloraAge(data->age);
        }
        else {
            scale = modelDef.getRandomScaleAtPosition(worldPos);
        }
        gatherer->addInstance(
            tileDef.modelId, tileIndex, worldPos, f32v3(0.0f, 0.0f, 1.0f), rotation, variantIndex, std::move(damageData), scale
        );
        
        if (modelDef.mCollisionShapeID != INVALID_COLLISION_SHAPE_ID) {
            physics->addTileModelCollider(tileIndex, tile.getMainID(), worldPos, f32q(f32v3(0.0f, 0.0f, rotation)), scale, tileDef.modelId);
        }
    }
    assert(false);
    /*GameThreadTasks::getInstance().addGenericTask([gatherer = std::move(gatherer)]() {


    });*/
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
                mInstancedStaticModelManager.removeTileInstancesFromContainer(containerId);

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
            std::move(builders),
            *containerToMesh
        );

    //assert(containerToMesh->getState() == TileContainerState::WAITING_MESH_AND_PHYSICS);
    // Pass result to the render thread
    RenderThreadTasks::getInstance().addGenericTask([taskData]() {
        PROFILE_SCOPE("TileContainerRenderer::updateMeshFromBuilders");

        const TileContainerID id = taskData->builders.containerId;
        World& world = taskData->builders.world;
        WorldRenderDataManager& renderDataManager = RenderContext::getInstance().getRenderDataManagerForWorld(world);
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
        const i32v3& worldPos3D = taskData->builders.tileData.spatialGrid.getWorldPos();
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
        instancedModelManager.addTileInstancesFromGatherer(taskData->builders.modelGatherer);

        // Release old dependencies and store new
        meshData.mAssetDependencies.swap(dependencies);

        // Release
        taskData->container.setDidInitMesh();
        taskData->container.decRef();

        delete taskData;
    });
}

void TileContainerMeshManager::removeMeshesForData(TileContainerMeshData& meshData) {
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

void TileContainerMeshManager::initEventHandlers(World& world) {

    TileContainerRepository& tileContainerRepository = world.getTileContainerRepository();
    tileContainerRepository.registerTileContainerListeners(mTileContainerListeners);

    tileContainerRepository.addLoadFinishedListener(mTileContainerListeners, [this](const TileContainerEvent& containerEvent) {
        ASSERT_GAME_THREAD();
        initTileContainerMesh(*containerEvent.container);
    });

    tileContainerRepository.addEditTilesListener(mTileContainerListeners, [this](const TileContainerEvent& containerEvent) {
        mInstancedStaticModelManager.onContainerEditEvent(containerEvent);
    });

    tileContainerRepository.addTileDamagedListener(mTileContainerListeners, [this](const TileContainerEvent& containerEvent) {
        mInstancedStaticModelManager.onTileDamagedEvent(containerEvent);
    });

    tileContainerRepository.addDestroyListener(mTileContainerListeners, [this](const TileContainerEvent& containerEvent) {
        assert(containerEvent.container->getRefCount() <= 1); // It must not be in a mesher task
        mTileContainersToRemove.enqueue(containerEvent.container->getId());
    });
}

void TileContainerMeshManager::initTileContainerMesh(TileContainer& tileContainer)
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
