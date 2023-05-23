#pragma once

#include <boost/container/flat_set.hpp>

#include "rendering/mesh/TileGrassMeshType.h"

#include "rendering/mesh/GrassBillboardMeshRenderData.h"

class MaterialShader;
class GrassMesh;
class Camera3D;

struct GrassMeshRenderDataWithPos {
    GrassBillboardMeshRenderData renderData;
    f32v3 pos;
};

class GrassRenderer
{
public:
    GrassRenderer();
    ~GrassRenderer();
    void renderGrass(const Camera3D& camera, const f32v3& playerPos, const boost::container::flat_set<const GrassMesh*>& grassMeshes);

    // Only call if you update any grass data through foliage editor or file reload
    static void updateUniformBuffer();

private:
    void renderDefaultGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshRenderDataWithPos>& grassMeshes);
    void renderPlaneGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshRenderDataWithPos>& grassMeshes);
    void renderBillboardGrass(const Camera3D& camera, const f32v3& playerPos, const std::vector<GrassMeshRenderDataWithPos>& grassMeshes);
    const MaterialShader* mMaterials[e_count(TileGrassMeshType)];

    std::vector<GrassMeshRenderDataWithPos> mVisibleMeshes[e_count(TileGrassMeshType)];
    static VGBuffer sGrassUniformBuffer; // TODO: This will never be destroyed
};

