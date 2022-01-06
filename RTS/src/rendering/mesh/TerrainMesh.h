#pragma once

#include "rendering/MeshBase.h"

constexpr ui32 TERRAIN_MESH_WIDTH_QUADS = 32;
constexpr ui32 TERRAIN_MESH_WIDTH_VERTS = TERRAIN_MESH_WIDTH_QUADS + 1;
constexpr ui32 TERRAIN_MESH_WIDTH_VERTS_SQ = SQ(TERRAIN_MESH_WIDTH_VERTS);
constexpr ui32 TERRAIN_MESH_SKIRT_VERTEX_COUNT = TERRAIN_MESH_WIDTH_VERTS * 4;
constexpr ui32 TERRAIN_MESH_SIZE_VERTS = SQ(TERRAIN_MESH_WIDTH_VERTS) + TERRAIN_MESH_SKIRT_VERTEX_COUNT;
constexpr ui32 TERRAIN_MESH_PADDED_WIDTH_VERTS = TERRAIN_MESH_WIDTH_VERTS + 2;
constexpr ui32 TERRAIN_MESH_PADDED_SIZE_VERTS = SQ(TERRAIN_MESH_PADDED_WIDTH_VERTS);

struct TerrainVertex {
public:
    TerrainVertex() {};
    TerrainVertex(const f32v3& pos, const f32v3& normal) :
        pos(pos), normal(normal) {
    }

    f32v3 pos; // TODO: Can we compress X/Y into some integer representation
    f32v3 normal;
    ui8 padding[8];
};
// Need power of 2 alignment
static_assert(sizeof(TerrainVertex) == 32, "Power of 2 byte alignment needed");

class TerrainMesh : public MeshBase
{
public:
    TerrainMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(TerrainMesh);

    //void init() override; // TODO: We cant override MeshBase::init because its called from constructor and that is illegal

    void beginMesh(const f32v2& cornerPos, f32 totalWidth);
    void setVertsFromPaddedHeightfield(const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    void draw(const vg::GLProgram & program) const override;
    void finishMesh(MeshDrawMode drawMode) override;

    static void initGlobalIBO();

private:
    void bindVertexAttribs(const vg::GLProgram & program) const override;

    std::vector<TerrainVertex> mVertexData;
    static VGBuffer sTerrainIbo; ///< Index Buffer Object
};

