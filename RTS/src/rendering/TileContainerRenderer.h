#pragma once

#include "tile/TileContainerEvents.h"
#include "TileContainerMeshData.h"
#include <boost/container/flat_set.hpp>

DECL_VG(class SpriteBatch);

class Camera3D;
class MaterialShader;
class TileContainer;
class Mesh;
class BuildingMesher;
class ChunkMesher;
class InstancedStaticModelRenderer;
class ContainerMeshBuilders;
struct ShadowPassShaderData;

class TileContainerRenderer {
public:
	TileContainerRenderer(InstancedStaticModelRenderer& instancedStaticModelRenderer);
	~TileContainerRenderer();

    void frameUpdate();

    // TODO: TileContainerMeshManager
    static void updateMeshFromBuilders(const TileContainer* containerToMesh, ContainerMeshBuilders&& builders);

    void renderStaticMeshes(const boost::container::flat_set<const Mesh*>& meshes, const Camera3D& camera);
    void renderBillboards(const boost::container::flat_set<const Mesh*>& meshes, const Camera3D& camera);
    void renderWorldShadows(const boost::container::flat_set<const Mesh*>& meshes, const ShadowPassShaderData& shaderData, const Camera3D& camera, f32 maxDistance);

private:
    void initEventHandlers();
    void updateTileContainerMesh(TileContainer& tileContainer);

    const MaterialShader* mShadowMapperMaterial = nullptr;
    const MaterialShader* mShadowMapperMaterialBillboard = nullptr;
    const MaterialShader* mStandardMaterial = nullptr;
    const MaterialShader* mBillboardMaterial = nullptr;

    InstancedStaticModelRenderer& mInstancedStaticModelRenderer;

    // Mesh management
    std::unordered_map<TileContainerID, TileContainerMeshData> mTileContainerMeshData; // TODO: MOVE TO WorldRenderData

    moodycamel::ConcurrentQueue<TileContainerID> mTileContainersToRemove;

    // Meshers
    std::unique_ptr<BuildingMesher> mBuildingMesher;
    std::unique_ptr<ChunkMesher> mChunkMesher;

    // Events
    TileContainerListeners mTileContainerListeners;
};

