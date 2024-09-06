#include "stdafx.h"
#include "WorldRenderDataManager.h"

#include "rendering/mesh/Mesh.h"
#include "rendering/TileContainerMeshData.h"

#include "rendering/mesh/TerrainMeshManager.h"
#include "rendering/mesh/GrassMeshManager.h"
#include "rendering/mesh/TileContainerMeshManager.h"
#include "rendering/model/InstancedStaticModelManager.h"
#include "weather/CloudMeshManager.h"

#include "world/World.h"


WorldRenderDataManager::WorldRenderDataManager(World& world) : mWorld(world) {
    ASSERT_GAME_THREAD(); // This is currently created on the game thread
    mCloudManager = std::make_unique<CloudMeshManager>(world.getChunkGenerator());
    mTerrainMeshManager = std::make_unique<TerrainMeshManager>(mWorld);
    mGrassMeshManager = std::make_unique<GrassMeshManager>(mWorld);
    mInstancedStaticModelManager = std::make_unique<InstancedStaticModelManager>();
    mTileContainerMeshManager = std::make_unique<TileContainerMeshManager>(mWorld , *mInstancedStaticModelManager);
}

WorldRenderDataManager::~WorldRenderDataManager() {

}

void WorldRenderDataManager::shutdown() {
    // Some things will crash if destroyed in destructor and must be destroyed before the world is
    mGrassMeshManager->shutdown();
    mTerrainMeshManager->shutdown();
    mTileContainerMeshManager->shutdown();

    mDidShutdown = true;
}

void WorldRenderDataManager::frameUpdate(const Camera3D& camera, f32 elapsedSec) {
    if (mDidShutdown) {
        return;
    }

    ASSERT_RENDER_THREAD();
    mInstancedStaticModelManager->frameUpdate(camera, elapsedSec);

    const f32v2 loadCenter = mWorld.getLoadCenter();
    mCloudManager->frameUpdate(loadCenter);
    mTileContainerMeshManager->frameUpdate();

    mTerrainMeshManager->frameUpdate(loadCenter, elapsedSec);
    mGrassMeshManager->frameUpdate(loadCenter, elapsedSec);
}
