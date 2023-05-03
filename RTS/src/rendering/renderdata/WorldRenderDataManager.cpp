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
    mCloudManager = std::make_unique<CloudMeshManager>();
    mTerrainMeshManager = std::make_unique<TerrainMeshManager>(mWorld);
    mGrassMeshManager = std::make_unique<GrassMeshManager>(mWorld);
    mInstancedStaticModelManager = std::make_unique<InstancedStaticModelManager>();
    mTileContainerMeshManager = std::make_unique<TileContainerMeshManager>(mWorld , *mInstancedStaticModelManager);

    mCloudManager->init(mWorld.getLoadCenter());
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
    mTileContainerMeshManager->frameUpdate();
}
