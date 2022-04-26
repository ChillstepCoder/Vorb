#pragma once

// Enough for a full chunk of grass + padding
// TODO: How much do we really save doing this?
// Profile how often we use this...
constexpr unsigned MAX_QUAD_MESH_INDICES = CHUNK_SIZE * 8 * 8 * 6 + CHUNK_SIZE * 6;
constexpr ui32 MAX_TEXTURES_PER_MESH = 500; // Must be even

enum class MeshDrawMode {
    DYNAMIC = GL_DYNAMIC_DRAW,
    STREAM = GL_STREAM_DRAW,
    STATIC = GL_STATIC_DRAW
};

enum class MeshFlags : ui8 {
    USING_SHARED_IBO = 1 << 0
};

struct SubMeshData {
    union {
        struct {
            VGBuffer  mVao;
            VGBuffer  mVbo;
            VGBuffer  mTextureUbo;
            VGBuffer  mIbo; // This should stay last since its possible to be shared
        };
        VGBuffer mBuffers[4] = {};
    };
    ui32 mIndexCount = 0; ///< Current capacity of mIbo
    ui16 mIndexType = GL_UNSIGNED_INT; // SHORT OR INT

    void destroy(bool isUsingSharedIbo);
};

class Mesh
{
    friend class MeshBuilder;
public:
    class Mesh();
    virtual ~Mesh();

    virtual void draw() const;
    void destroy();
    bool isValid() const { return mMainMesh.mVao != 0; }

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