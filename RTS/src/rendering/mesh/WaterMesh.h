#pragma once

#include "world/TerrainConstants.h"
#include "rendering/MeshBase.h"


struct WaterVertex {
public:
    WaterVertex() {};
    WaterVertex(const f32v3& pos, const f32 depth) :
        pos(pos), depth(depth) {
    }

    f32v3 pos; // TODO: Can we compress X/Y into some integer representation
    f32 depth;
};
// Need power of 2 alignment
static_assert(sizeof(WaterVertex) == 16, "Power of 2 byte alignment needed");

class WaterMesh : public MeshBase
{
public:
    WaterMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(WaterMesh);

    //void init() override; // TODO: We cant override MeshBase::init because its called from constructor and that is illegal

    void beginMesh(const f32v2& cornerPos, f32 totalWidth);
    void setVertsFromPaddedHeightfield(const f32 paddedHeightfield[TERRAIN_MESH_PADDED_WIDTH_VERTS][TERRAIN_MESH_PADDED_WIDTH_VERTS]);
    void draw(const vg::GLProgram& program) const override;
    void finishMesh(MeshDrawMode drawMode) override;

    static void initGlobalIBO();

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;

    std::vector<WaterVertex> mVertexData;
    static VGBuffer sWaterIbo; ///< Index Buffer Object
};

