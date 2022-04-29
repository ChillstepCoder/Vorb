#pragma once

#include <Vorb/graphics/gtypes.h>
#include "MeshBase.h"

#include "rendering/TileVertex.h"
#include "rendering/RenderCommon.h"

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
    void addTerrainAlignedQuad(f32v2 tilePosition, f32 terrainCorners[4], ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool flipTriangleDir, bool shouldRandFlipHorizontal);
    void addCross(f32v3 cornerPosition, ui16 spriteAtlasPage, const f32v4& uvs, float width, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence);
    void finishMesh(MeshDrawMode drawMode) override;

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;

    std::vector<TileVertex> mVertexData; // TODO: Recycle?
};
struct GrassBillboardInstanceData {
    GrassBillboardInstanceData(ui8v2&& dims, ui8 grassType) : dims(dims), grassType(grassType) {};
    ui8v2 dims;
    ui8 grassType;
    ui8 pad;
};
static_assert(sizeof(GrassBillboardInstanceData) == 4);

class GrassBillboardMesh : public IQuadMesh<GrassBillboardInstanceData> {
public:
    GrassBillboardMesh() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(GrassBillboardMesh);

    void reserveQuadCount(size_t count);
    void addBladeQuad(const f32v3& tilePosition, const f32v2& xyDims, ui8 grassType);
    void draw(const vg::GLProgram& program) const override;
    void finishMesh(MeshDrawMode drawMode) override;
    void destroy() override;

private:

    void bindVertexAttribs(const vg::GLProgram& program) const override;

    std::vector<GrassBillboardInstanceData> mInstanceData; // TODO: Recycle?
    std::vector<f32v3> mPositionData; // TODO: Recycle?
    VGTexture mTboInstanceData = 0;
    VGTexture mTboPositionData = 0;
    VGBuffer mVboPosition = 0;
};

// Templated Mesh implementation
template <typename VERTEX>
void IQuadMesh<VERTEX>::setData(const VERTEX* meshData, unsigned vertexCount, MeshDrawMode drawMode) {

    lazyInitBuffers();
    const unsigned indexCount = (vertexCount / 4) * 6;
    assert(indexCount < MAX_QUAD_MESH_INDICES);

    mIndexCount = indexCount;

    const unsigned bufferSizeBytes = vertexCount * sizeof(VERTEX);

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    // Orphan the buffer for speed
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, meshData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
