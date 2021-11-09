#pragma once

DECL_VG(class GLProgram);
enum class MeshDrawMode {
    DYNAMIC = GL_DYNAMIC_DRAW,
    STREAM = GL_STREAM_DRAW,
    STATIC = GL_STATIC_DRAW
};

constexpr unsigned MAX_MESH_INDICES = CHUNK_SIZE * 12 * 6;

// TODO: Store material ID here?
class MeshBase {
public:
    MeshBase();
    virtual ~MeshBase();

    VORB_NON_COPYABLE_BUT_MOVABLE(MeshBase);

    static void initStaticIBO();

    virtual void init(); ///< Called automatically on construction, but can be safely called twice to no effect
    void destroy();

    virtual void draw(const vg::GLProgram& program) const;

    bool isValid() const { return mIndexCount > 0; }
    void setAABB(ui32AABB2& aabb) { mAABB = aabb; }
    const ui32AABB2& getAABB() const { return mAABB; }

    virtual void finishMesh(MeshDrawMode drawMode) = 0;

protected:
    virtual void bindVertexAttribs(const vg::GLProgram& program) const = 0;

    VGVertexArray mVao = 0; ///< Vertex Array Object
    VGBuffer mVbo = 0; ///< Vertex Buffer Object
    static VGBuffer sQuadIbo; ///< Index Buffer Object
    ui32 mIndexCount = 0; ///< Current capacity of the m_ibo
    mutable const vg::GLProgram* mLastUsedProgram = nullptr;
    ui32AABB2 mAABB = ui32AABB2(0, 0, UINT32_MAX, UINT32_MAX);  ///< Optional AABB to describe the bounds
};