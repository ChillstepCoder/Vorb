#pragma once

#include "world/WorldData.h"
#include "data_structure/FlatQuadtree.h"

constexpr ui32 TERRAIN_TREE_NODE_WIDTH_VERTS = 32;
constexpr ui32 TERRAIN_TREE_NODE_SIZE_VERTS = SQ(TERRAIN_TREE_NODE_WIDTH_VERTS);

class Camera3D;
class TerrainMesh;
DECL_VG(class GLProgram);

class HeightmapTerrainQuadtree : public FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>
{
public:
    HeightmapTerrainQuadtree();

    void init(const f32v2& worldPosition);

    void render(const Camera3D& camera, const vg::GLProgram& program) const;

private:
    void buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) override;
    void freeMeshForPatch(ui32 patchIndex) override;

    std::unique_ptr<TerrainMesh> mMeshes[FlatQuadtree<GRASS_QUADTREE_MAX_LOD, CHUNK_WIDTH>::NODE_COUNT];
    ui32 mRefCount = 0; // TODO: This is probably unneeded
};
