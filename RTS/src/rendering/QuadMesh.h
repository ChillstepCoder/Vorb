#pragma once

#include <Vorb/graphics/gtypes.h>
#include <Vorb/graphics/DepthState.h>

DECL_VG(class GLProgram);
class Camera2D;
struct TileVertex;

enum class QuadMeshDrawMode {
    DYNAMIC = GL_DYNAMIC_DRAW,
    STREAM = GL_STREAM_DRAW,
    STATIC = GL_STATIC_DRAW
};

// TODO: Store material ID here?
class QuadMesh {
public:
    QuadMesh();
    ~QuadMesh();

    void init(); ///< Called automatically on construction, but can be safely called twice to no effect
    void destroy();

    /*class Batch {
    public:
        void set(ui32 iOff, ui32 texID);
        ui32 textureID;
        ui32 indices;
        ui32 indexOffset;
    };*/

    // Indices are 0, 2, 3, 3, 1, 0
    void setData(const TileVertex* meshData, int vertexCount, VGTexture texture, QuadMeshDrawMode drawMode);
    void draw(const vg::GLProgram& program) const;

    bool isValid() const { return mIndexCount > 0; }

    void setAABB(ui32AABB& aabb) { mAABB = aabb; }
    const ui32AABB& getAABB() const { return mAABB; }

private:
    void bindVertexAttribs(const vg::GLProgram& program) const;

    VGVertexArray mVao = 0; ///< Vertex Array Object
    VGBuffer mVbo = 0; ///< Vertex Buffer Object
    VGBuffer mIbo = 0; ///< Index Buffer Object
    VGTexture mTexture = 0;
    ui32 mIndexCount = 0; ///< Current capacity of the m_ibo
    mutable const vg::GLProgram* mLastUsedProgram = nullptr;
    ui32AABB mAABB = ui32AABB(0, 0, UINT32_MAX, UINT32_MAX);  ///< Optional AABB to describe the bounds
   // std::vector<Batch> mBatches; ///< Vector of batches for rendering
};
