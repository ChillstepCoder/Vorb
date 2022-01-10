#pragma once

DECL_VG(class GLProgram);
enum class MeshDrawMode {
    DYNAMIC = GL_DYNAMIC_DRAW,
    STREAM = GL_STREAM_DRAW,
    STATIC = GL_STATIC_DRAW
};

// Enough for a full chunk of grass + padding
constexpr unsigned MAX_MESH_INDICES = CHUNK_SIZE * 8 * 8 * 6 + CHUNK_SIZE * 6;

struct BoundingSphere {
    f32v3 center = f32v3(0.0f);
    f32 radius = 0.0f;
};

inline BoundingSphere boundingSphereFromAABB(const f32AABB3& aabb) {
    BoundingSphere rv;
    rv.center = aabb.getCenter();
    rv.radius = sqrt(SQ(aabb.dims.x * 0.5f) + SQ(aabb.dims.y * 0.5f) + SQ(aabb.dims.z * 0.5f));
    return rv;
}

// TODO: Store material ID here?
class MeshBase {
public:
    MeshBase();
    virtual ~MeshBase();

    VORB_NON_COPYABLE_BUT_MOVABLE(MeshBase);

    static void initStaticIBO();

    void lazyInitBuffers(); ///< Called automatically on construction, but can be safely called twice to no effect
    virtual void destroy();

    virtual void draw(const vg::GLProgram& program) const;

    bool isValid() const { return mIndexCount > 0; }
    void setBoundingSphere(const BoundingSphere& boundingSphere) { mBoundingSphere = boundingSphere; }
    const BoundingSphere& getBoundingSphere() const { return mBoundingSphere; }

    virtual void finishMesh(MeshDrawMode drawMode) = 0;

protected:
    virtual void bindVertexAttribs(const vg::GLProgram& program) const = 0;

    VGVertexArray mVao = 0; ///< Vertex Array Object
    VGBuffer mVbo = 0; ///< Vertex Buffer Object
    ui32 mIndexCount = 0; ///< Current capacity of the m_ibo
    mutable VGProgram mLastUsedProgram = UINT32_MAX;
    BoundingSphere mBoundingSphere;  ///< Optional AABB to describe the bounds

    // Static
    static VGBuffer sQuadIbo; ///< Index Buffer Object
};