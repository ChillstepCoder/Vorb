#include "stdafx.h"
#include "WorldRenderDataManager.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/TileContainerMeshData.h"

#include "rendering/mesh/TerrainMeshManager.h"
#include "rendering/mesh/GrassMeshManager.h"
#include "rendering/mesh/TileContainerMeshManager.h"
#include "rendering/model/InstancedStaticModelManager.h"
#include "weather/CloudMeshManager.h"

#include "world/IWorld.h"


WorldRenderDataManager::WorldRenderDataManager(IWorld& world) : mWorld(world) {
    mCloudManager = std::make_unique<CloudMeshManager>(mWorld.getLoadCenter());
    mTerrainMeshManager = std::make_unique<TerrainMeshManager>(mWorld);
    mGrassMeshManager = std::make_unique<GrassMeshManager>();
    mInstancedStaticModelManager = std::make_unique<InstancedStaticModelManager>();
    mTileContainerMeshManager = std::make_unique<TileContainerMeshManager>(mInstancedStaticModelManager);
}

WorldRenderDataManager::~WorldRenderDataManager() {

}

void WorldRenderDataManager::tickGameThread() {
    ASSERT_GAME_THREAD();
    const f32v2 loadCenter = mWorld.getLoadCenter();
    mTerrainMeshManager->tickGameThread(loadCenter);
    mGrassMeshManager->tickGameThread(loadCenter);
}

void WorldRenderDataManager::frameUpdate(const Camera3D& camera) {
    ASSERT_RENDER_THREAD();
    mInstancedStaticModelManager->frameUpdate(camera);

    const f32v2 loadCenter = mWorld.getLoadCenter();
    mCloudManager->frameUpdate(loadCenter);
    mTileContainerMeshManager->frameUpdate(*this);
}

void WorldRenderDataManager::removeMeshesForData(TileContainerMeshData& meshData) {
    WorldRenderData& renderData = mWorldRenderData;

    if (meshData.mStaticMesh != nullptr) {
        renderData.mStaticMeshes.erase(meshData.mStaticMesh.get());
        meshData.mStaticMesh.reset();
    }
    if (meshData.mDynamicMesh != nullptr) {
        renderData.mDynamicMeshes.erase(meshData.mDynamicMesh.get());
        meshData.mDynamicMesh.reset();
    }
    if (meshData.mBillboardMesh != nullptr) {
        renderData.mBillboardMeshes.erase(meshData.mBillboardMesh.get());
        meshData.mBillboardMesh.reset();
    }
}
