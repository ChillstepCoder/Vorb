#pragma once

// Enough for a full chunk of grass + padding
// TODO: How much do we really save doing this?
// Profile how often we use this...
constexpr unsigned MAX_QUAD_MESH_INDICES = CHUNK_SIZE * 8 * 8 * 6 + CHUNK_SIZE * 6;

typedef i32 SubmeshIndex;

enum class MeshLODLevel {
    Highest,
    Medium,
    Low,
    Lowest,
    COUNT
};

struct MeshLODDrawInfo {
    ui32 startIndex;
    ui32 indexCount;
};

struct MeshLODData {
    // TODO: High start is always 0 so why store it
    ui32 mLODStarts[e_cast(MeshLODLevel::COUNT)] = {};
    ui32 mTotalIndexCount = 0;

    MeshLODDrawInfo getDrawInfoForLOD(MeshLODLevel lod) const {
        const ui32 start = mLODStarts[e_cast(lod)];
        // TODO: Remove branching?
        if (lod == MeshLODLevel::Lowest) {
            return MeshLODDrawInfo{ start, mTotalIndexCount - start };
        }
        return MeshLODDrawInfo{ start, mLODStarts[e_cast(lod) + 1] - start };
    }
};

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
    MeshLODData mLODData;
    ui16 mIndexType = GL_UNSIGNED_INT; // SHORT OR INT
    BitFlags<MeshFlags> mFlags;
    SubMeshData* mNextSubmesh = nullptr; // We store these as a linked list, this is not a true parent

    void* operator new(size_t count);
    void operator delete(void* pointer, size_t size);

    void allocateSubmeshCount(size_t count);
    void destroy();
};

// TODO: Indirect https://cpp-rendering.io/indirect-rendering/
class Mesh
{
    friend class ProceduralMeshBuilder;
    friend class BillboardMeshBuilder;
    friend class TextMeshBuilder;
    friend class ModelMeshBuilder;
public:
    Mesh();
    ~Mesh();

    VORB_NON_COPYABLE_BUT_MOVABLE(Mesh);

    // TODO: Have each renderer implement its own "draw" method
    //   Renderer knows what type of mesh this is  so we can avoid
    //   branching and assert on internal state such as ubo
    void draw() const;
    void draw(MeshLODLevel lod) const;
    void drawInstanced(GLsizei instanceCount) const;
    void drawInstanced(MeshLODLevel lod, GLsizei instanceCount) const;
    void destroy();
    bool isValid() const { return mMainMesh.mVao != 0; }

    const f32v3& getPosition() const { return mPosition; }
    const BoundingSphere& getBoundingSphere() const { return mBoundingSphere; }

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);

public:    
    f32v3                    mPosition = f32v3(0.0f);
    BoundingSphere           mBoundingSphere;  ///< Optional
    SubMeshData              mMainMesh;
    // TODO: Pool allocate?
    // TODO: We dont need dynamic vector, just use a C array
   // std::vector<SubMeshData> mSubMeshes; ///< Most meshes wont have any submeshes so we store 2-infinity meshes in a separate data store to keep Mesh smaller

};
static_assert(sizeof(Mesh) == 88);