#pragma once

// Enough for a full chunk of grass + padding
// TODO: How much do we really save doing this?
// Profile how often we use this...
constexpr unsigned MAX_QUAD_MESH_INDICES = CHUNK_SIZE * 8 * 8 * 6 + CHUNK_SIZE * 6;

enum class MeshDrawMode {
    DYNAMIC = GL_DYNAMIC_DRAW,
    STREAM = GL_STREAM_DRAW,
    STATIC = GL_STATIC_DRAW
};

enum class MeshFlags : ui8 {
    USING_SHARED_IBO = 1 << 0
};

struct SubMeshData {
    VGBuffer  mVao = 0;
    VGBuffer  mVbo = 0;
    VGBuffer  mUbo = 0;
    VGBuffer  mIbo = 0;
    VGBuffer  mSSBO = 0;
    ui32 mIndexCount = 0; ///< Current capacity of mIbo
    ui16 mIndexType = GL_UNSIGNED_INT; // SHORT OR INT // TODO: DELETE

    void destroy(bool isUsingSharedIbo);
};

class Mesh
{
    friend class MeshBuilder;
    friend class BillboardMeshBuilder;
public:
    Mesh();
    virtual ~Mesh();

    VORB_NON_COPYABLE_BUT_MOVABLE(Mesh);

    virtual void draw() const;
    void destroy();
    bool isValid() const { return mMainMesh.mIndexCount != 0; }

    const BoundingSphere& getBoundingSphere() const { return mBoundingSphere; }

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);

protected:
    SubMeshData              mMainMesh;
    // TODO: Pool allocate?
    // TODO: We dont need dynamic vector, just use a C array
    std::vector<SubMeshData> mSubMeshes; ///< Most meshes wont have any submeshes so we store 2-infinity meshes in a separate data store to keep Mesh smaller
    BoundingSphere           mBoundingSphere;  ///< Optional
    BitFlags<MeshFlags>      mFlags;
};