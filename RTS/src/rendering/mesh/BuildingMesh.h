#pragma once

#include "rendering/MeshBase.h"

#include "rendering/TileVertex.h"

class BuildingMesh : public MeshBase
{
public:
    void addTriangle(TriangleVertex verts[3]);
    void addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, CubeFacing axis, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal);

    void draw(const vg::GLProgram& program) const override;
    void finishMesh(MeshDrawMode drawMode) override;

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;

    VGBuffer mIbo = 0;
    std::vector<TriangleVertex> mVertexData;
    std::vector<ui32> mIndexData;
};

