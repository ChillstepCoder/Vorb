#pragma once

#include <boost/container/flat_set.hpp>

#include "rendering/mesh/TileGrassMeshType.h"

#include "rendering/mesh/GrassBillboardMeshRenderData.h"
#include "resources/asset/AssetHandleBundle.h"

class MaterialShaderDef;
class GrassMesh;
class Camera3D;
class World;

DECL_VG(class GLProgram);

struct GrassMeshFrameRenderData {
    GrassBillboardMeshRenderData renderData;
    f32v3 pos;
    int crossfadeDir;
    f32 crossfadeAlpha;
};

class GrassRenderer
{
public:
    GrassRenderer();
    ~GrassRenderer();

    void onWorldBegin(World& world);

    void renderGrass(const Camera3D& camera, const f32v3& playerPos, const boost::container::flat_set<const GrassMesh*>& grassMeshes);

    // Only call if you update any grass data through foliage editor or file reload
    static void updateUniformBuffer();

private:
    void renderDefaultGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshFrameRenderData>& grassMeshes);
    void renderPlaneGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshFrameRenderData>& grassMeshes);
    void renderBillboardGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshFrameRenderData>& grassMeshes);
    void uploadGrassMeshUniforms(const GrassMeshFrameRenderData& grassMesh);
    void cacheUniforms(const vg::GLProgram& program);
    
    const MaterialShaderDef* mMaterials[e_count(TileGrassMeshType)];
    AssetHandleBundle mShaderAssets;

    std::vector<GrassMeshFrameRenderData> mVisibleMeshes[e_count(TileGrassMeshType)];
    static VGBuffer sGrassUniformBuffer; // TODO: This will never be destroyed

    VGTexture mBiomeTexture = 0; // Owned by the world
    f32 mInverseWorldWidth = 0.0f;

    VGUniform mPositionUniform;
    VGUniform mCrossfadeAlphaUniform;
    VGUniform mCrossfadeDirectionUniform;
};

