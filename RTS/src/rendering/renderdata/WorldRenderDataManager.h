#pragma once
#include "WorldRenderData.h"

struct TileContainerMeshData;
class IWorld;
class TerrainMeshManager;
class GrassMeshManager;
class TileContainerMeshManager;
class CloudMeshManager;
class InstancedStaticModelManager;
class Camera3D;

class WorldRenderDataManager {
public:
    WorldRenderDataManager(IWorld& world);
    ~WorldRenderDataManager();

    void tickGameThread();
    void frameUpdate(const Camera3D& camera);

    void addTerrainMesh(const TerrainMesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mTerrainMeshes.insert(mesh); }
    void removeTerrainMesh(const TerrainMesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mTerrainMeshes.erase(mesh); }
    void addTerrainWaterMesh(const TerrainMesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mTerrainWaterMeshes.insert(mesh); }
    void removeTerrainWaterMesh(const TerrainMesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mTerrainWaterMeshes.erase(mesh); }
    void addGrassMesh(const GrassMesh* mesh) {  ASSERT_RENDER_THREAD(); mWorldRenderData.mGrassMeshes.insert(mesh); }
    void removeGrassMesh(const GrassMesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mGrassMeshes.erase(mesh); }

    void addStaticMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mStaticMeshes.insert(mesh); }
    void removeStaticMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mStaticMeshes.erase(mesh); }
    void addDynamicMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mDynamicMeshes.insert(mesh); }
    void removeDynamicMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mDynamicMeshes.erase(mesh); }
    void addBillboardMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mBillboardMeshes.insert(mesh); }
    void removeBillboardMesh(const Mesh* mesh) { ASSERT_RENDER_THREAD(); mWorldRenderData.mBillboardMeshes.erase(mesh); }

    void removeMeshesForData(TileContainerMeshData& meshData);

    // Accessors
    TileContainerMeshManager& getTileContainerMeshManager() const { return *mTileContainerMeshManager; }

private:
    IWorld& mWorld;

    // Mesh managers 
    std::unique_ptr<InstancedStaticModelManager> mInstancedStaticModelManager;
    std::unique_ptr<TileContainerMeshManager> mTileContainerMeshManager;
    std::unique_ptr<TerrainMeshManager> mTerrainMeshManager;
    std::unique_ptr<GrassMeshManager> mGrassMeshManager;
    std::unique_ptr<CloudMeshManager> mCloudManager;

    // Data
    WorldRenderData mWorldRenderData;
};

