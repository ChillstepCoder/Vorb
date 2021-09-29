#pragma once

#include <Vorb/graphics/gtypes.h>
#include <Vorb/graphics/DepthState.h>
#include "rendering/TileVertex.h"

DECL_VG(class GLProgram);
class Camera2D;

enum class QuadMeshDrawMode {
    DYNAMIC = GL_DYNAMIC_DRAW,
    STREAM = GL_STREAM_DRAW,
    STATIC = GL_STATIC_DRAW
};

// TODO: Store material ID here?
class MeshBase {
public:
    MeshBase();
    virtual ~MeshBase();

    static void initStaticIBO();

    void init(); ///< Called automatically on construction, but can be safely called twice to no effect
    void destroy();

    void draw(const vg::GLProgram& program) const;

    bool isValid() const { return mIndexCount > 0; }
    void setAABB(ui32AABB2& aabb) { mAABB = aabb; }
    const ui32AABB2& getAABB() const { return mAABB; }

protected:
    virtual void bindVertexAttribs(const vg::GLProgram& program) const = 0;

    VGVertexArray mVao = 0; ///< Vertex Array Object
    VGBuffer mVbo = 0; ///< Vertex Buffer Object
    static VGBuffer sIbo; ///< Index Buffer Object
    ui32 mIndexCount = 0; ///< Current capacity of the m_ibo
    mutable const vg::GLProgram* mLastUsedProgram = nullptr;
    ui32AABB2 mAABB = ui32AABB2(0, 0, UINT32_MAX, UINT32_MAX);  ///< Optional AABB to describe the bounds
};

template <typename VERTEX>
class Mesh : public MeshBase {
public:
    void setData(const VERTEX* meshData, unsigned vertexCount, QuadMeshDrawMode drawMode);
};

class QuadMesh : public Mesh<TileVertex> {
public:
    void reserveQuadCount(size_t count);
    void addAxisAlignedQuad(f32v3 tilePosition, const f32v2& xyDims, const f32v2& xyOffset, const i32v2& xyAxis, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal);
    void addCross(f32v3 cornerPosition, ui16 spriteAtlasPage, const f32v4& uvs, float width, color4 color, bool shouldRandFlipHorizontal, ui8 windInfluence);
    void finishMesh(QuadMeshDrawMode drawMode);

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;

    std::vector<TileVertex> mVertexData; // TODO: Recycle?
};

class BillboardMesh : public Mesh<BillboardVertex> {
public:
    void reserveQuadCount(size_t count);
    void addQuad(f32v3 tilePosition, const f32v2& xyDims, ui16 spriteAtlasPage, const f32v4& uvs, color4 color, bool shouldRandFlipHorizontal);
    void finishMesh(QuadMeshDrawMode drawMode);

private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;

    std::vector<BillboardVertex> mVertexData; // TODO: Recycle?
};

// Templated Mesh implementation
constexpr unsigned MAX_MESH_INDICES = CHUNK_SIZE * 12 * 6;

template <typename VERTEX>
void Mesh<VERTEX>::setData(const VERTEX* meshData, unsigned vertexCount, QuadMeshDrawMode drawMode) {

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