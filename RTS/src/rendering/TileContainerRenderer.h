#pragma once

#include <boost/container/flat_set.hpp>
#include "tile/TileContainerEvents.h"

DECL_VG(class SpriteBatch);

class Camera3D;
class MaterialShader;
class TileContainer;
class Mesh;
class BuildingMesher;
class ChunkMesher;
class InstancedStaticModelRenderer;
class ContainerMeshBuilders;

struct TileContainerMeshData {
    TileContainerMeshData() = default;
    ~TileContainerMeshData();

    VORB_NON_COPYABLE_BUT_MOVABLE(TileContainerMeshData);

    std::unique_ptr<Mesh> mStaticMesh;
    std::unique_ptr<Mesh> mDynamicMesh;
    std::unique_ptr<Mesh> mBillboardMesh;
};

// TODO: IRendererBase?
// TODO: DELETE ME
class TileContainerRenderer {
public:
	TileContainerRenderer(InstancedStaticModelRenderer& instancedStaticModelRenderer);
	~TileContainerRenderer();

    void frameUpdate();

    static void updateMeshFromBuilders(const TileContainer* containerToMesh, ContainerMeshBuilders&& builders);

    void renderStaticMeshes(const Camera3D& camera);
    void renderBillboards(const Camera3D& camera);
    void renderWorldShadows(const Camera3D& camera, f32 maxDistance);

    void addStaticMesh(const Mesh* mesh) { assert(IS_RENDER_THREAD()); mStaticMeshes.insert(mesh); }
    void removeStaticMesh(const Mesh* mesh) { assert(IS_RENDER_THREAD()); mStaticMeshes.erase(mesh); }
    void addDynamicMesh(const Mesh* mesh) { assert(IS_RENDER_THREAD()); mDynamicMeshes.insert(mesh); }
    void removeDynamicMesh(const Mesh* mesh) { assert(IS_RENDER_THREAD()); mDynamicMeshes.erase(mesh); }
    void addBillboardMesh(const Mesh* mesh) { assert(IS_RENDER_THREAD()); mBillboardMeshes.insert(mesh); }
    void removeBillboardMesh(const Mesh* mesh) { assert(IS_RENDER_THREAD()); mBillboardMeshes.erase(mesh); }

private:
    void initEventHandlers();
    void updateTileContainerMesh(TileContainer& tileContainer);
    void removeMeshesForData(TileContainerMeshData& meshData);

    boost::container::flat_set<const Mesh*> mStaticMeshes;
    boost::container::flat_set<const Mesh*> mDynamicMeshes;
    boost::container::flat_set<const Mesh*> mBillboardMeshes;

    const MaterialShader* mShadowMapperMaterial = nullptr;
    const MaterialShader* mShadowMapperMaterialBillboard = nullptr;
    const MaterialShader* mStandardMaterial = nullptr;
    const MaterialShader* mBillboardMaterial = nullptr;

    InstancedStaticModelRenderer& mInstancedStaticModelRenderer;

    // Mesh management
    std::unordered_map<TileContainerID, TileContainerMeshData> mTileContainerMeshData;

    moodycamel::ConcurrentQueue<TileContainerID> mTileContainersToRemove;

    // Meshers
    std::unique_ptr<BuildingMesher> mBuildingMesher;
    std::unique_ptr<ChunkMesher> mChunkMesher;

    // Events
    TileContainerListeners mTileContainerListeners;
};

