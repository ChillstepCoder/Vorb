#pragma once

#include <Vorb/graphics/gtypes.h>
#include <Vorb/graphics/DepthState.h>

DECL_VG(class GLProgram);
class Camera2D;
struct TileVertex;
struct BillboardVertex;

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
    VGBuffer mIbo = 0; ///< Index Buffer Object
    ui32 mIndexCount = 0; ///< Current capacity of the m_ibo
    mutable const vg::GLProgram* mLastUsedProgram = nullptr;
    ui32AABB2 mAABB = ui32AABB2(0, 0, UINT32_MAX, UINT32_MAX);  ///< Optional AABB to describe the bounds
};

template <typename VERTEX>
class Mesh : public MeshBase {
public:
    void setData(const VERTEX* meshData, int vertexCount, QuadMeshDrawMode drawMode);
};

class QuadMesh : public Mesh<TileVertex> {
private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;
};

class BillboardMesh : public Mesh<BillboardVertex> {
private:
    void bindVertexAttribs(const vg::GLProgram& program) const override;
};

// Templated Mesh implementation
constexpr unsigned MAX_MESH_INDICES = CHUNK_SIZE * 4 * 6;
extern bool sQuadIndicesInitialized;
extern ui32 sQuadIndices[MAX_MESH_INDICES];
extern void initSharedQuadIndices();

template <typename VERTEX>
void Mesh<VERTEX>::setData(const VERTEX* meshData, int vertexCount, QuadMeshDrawMode drawMode) {

    if (!sQuadIndicesInitialized) {
        initSharedQuadIndices();
    }

    const unsigned indexCount = (vertexCount / 4) * 6;
    assert(indexCount < MAX_MESH_INDICES);

    // build IBO (todo: shared)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIbo);
    // Orphan the buffer for speed
    // TODO: Do we want dynamic draw or static/stream?
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(ui32), nullptr, enum_cast(drawMode));

    mIndexCount = indexCount;
    // TODO: Can we get away with just a single IBO?
    // Set data
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexCount * sizeof(ui32), sQuadIndices);

    const int bufferSizeBytes = vertexCount * sizeof(VERTEX);

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    // Orphan the buffer for speed
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, enum_cast(drawMode));
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, meshData);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}