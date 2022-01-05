#pragma once

#include "world/WorldData.h"
#include "data_structure/FlatQuadtree.h"

constexpr ui32 TERRAIN_TREE_NODE_WIDTH_VERTS = 32;
constexpr ui32 TERRAIN_TREE_NODE_SIZE_VERTS = SQ(TERRAIN_TREE_NODE_WIDTH_VERTS);

class Camera3D;
DECL_VG(class GLProgram);

class HeightmapTerrainQuadtree
{
public:

    void render(const Camera3D& camera, const vg::GLProgram& program);

    void update(const f32v2& loadCenter);

private:
    ui32v2 mWorldPos;
};

