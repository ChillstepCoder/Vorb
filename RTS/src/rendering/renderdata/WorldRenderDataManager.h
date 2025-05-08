#pragma once

struct TileContainerMeshData;
class World;
class TerrainMeshManager;
class GrassMeshManager;
class TileContainerMeshManager;
class CloudMeshManager;
class InstancedStaticModelManager;
class Camera3D;

class WorldRenderDataManager {
public:
    WorldRenderDataManager(World& world);
    ~WorldRenderDataManager();

    void shutdown();

    void frameUpdate(const Camera3D& camera, f32 elapsedSec);

    // Accessors
    InstancedStaticModelManager& getInstancedStaticModelManager() const { return *mInstancedStaticModelManager; }
    TileContainerMeshManager& getTileContainerMeshManager() const { return *mTileContainerMeshManager; }
    TerrainMeshManager& getTerrainMeshManager() const { return *mTerrainMeshManager; }
    GrassMeshManager& getGrassMeshManager() const { return *mGrassMeshManager; }
    CloudMeshManager& getCloudMeshManager() const { return *mCloudManager; }

private:
    World& mWorld;

    // Mesh managers 
    std::unique_ptr<InstancedStaticModelManager> mInstancedStaticModelManager;
    std::unique_ptr<TileContainerMeshManager> mTileContainerMeshManager;
    std::unique_ptr<TerrainMeshManager> mTerrainMeshManager;
    std::unique_ptr<GrassMeshManager> mGrassMeshManager;
    std::unique_ptr<CloudMeshManager> mCloudManager;

    bool mDidShutdown = false;
};

