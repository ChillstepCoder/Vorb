#pragma once

#include <Vorb/graphics/gtypes.h>
#include "MeshBase.h"

#include "rendering/TileVertex.h"

template <typename VERTEX>
class IQuadMesh : public MeshBase {
public:
    IQuadMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(IQuadMesh);

    void setData(const VERTEX* meshData, unsigned vertexCount, MeshDrawMode drawMode);
};

class QuadMesh : public IQuadMesh<TileVertex> {
public:
    QuadMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(QuadMesh);

    void reserveQuadCount(size_t count);
    void addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, CubeFacing axis, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal);
    void addCross(f32v3 cornerPosition, ui16 spriteAtlasPage, const f32v4& uvs, float width, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence);
    void finishMesh(MeshDrawMode drawMode) override;

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;

    std::vector<TileVertex> mVertexData; // TODO: Recycle?
};

class BillboardMesh : public IQuadMesh<BillboardVertex> {
public:
    BillboardMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(BillboardMesh);

    void reserveQuadCount(size_t count);
    void addQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence);
    void finishMesh(MeshDrawMode drawMode) override;

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;

    std::vector<BillboardVertex> mVertexData; // TODO: Recycle?
};

// Templated Mesh implementation
template <typename VERTEX>
void IQuadMesh<VERTEX>::setData(const VERTEX* meshData, unsigned vertexCount, MeshDrawMode drawMode) {

    const unsigned indexCount = (vertexCount / 4) * 6;
    assert(indexCount < MAX_MESH_INDICES);

    mIndexCount = indexCount;

    const unsigned bufferSizeBytes = vertexCount * sizeof(VERTEX);

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    // Orphan the buffer for speed
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, enum_cast(drawMode));
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, meshData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
