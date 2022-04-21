#pragma once

#include "world/TerrainConstants.h"
#include "rendering/MeshBase.h"

struct alignas(32) TerrainVertex {
public:
    TerrainVertex() {};
    TerrainVertex(const f32v3& pos, const f32v3& normal) :
        pos(pos), normal(normal) {
    }

    f32v3 pos;
    f32v3 normal;
};
// Need power of 2 alignment
static_assert(sizeof(TerrainVertex) == 32, "32 byte alignment needed");

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
