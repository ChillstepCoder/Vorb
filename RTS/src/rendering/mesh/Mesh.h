#pragma once


#include "rendering/mesh/VertexType.h"
#include "rendering/mesh/MeshSkeletonData.h"
#include "rendering/model/MaterialRenderPassType.h"

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

    MeshLODData& operator=(const MeshLODData& o) {
        this->mTotalIndexCount = o.mTotalIndexCount;
        memcpy(this->mLODStarts, o.mLODStarts, sizeof(ui32) * e_cast(MeshLODLevel::COUNT));
        return *this;
    }

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

enum class MeshIndexType : ui16 {
    INVALID = 0,
    SHORT = GL_UNSIGNED_SHORT,
    INT = GL_UNSIGNED_INT
};

class MeshCpuData final {
public:
    MeshCpuData() = default;
    VORB_NON_COPYABLE(MeshCpuData);
    ~MeshCpuData();
    MeshCpuData(MeshCpuData&& o);
    MeshCpuData& operator=(MeshCpuData&& o);

    // These are cleaned up in destrutor
    void* mVertsPtr = nullptr;
    void* mElementsPtr = nullptr;
    ui32 mVertsCount = 0;
    MeshIndexType mIndexType = MeshIndexType::INVALID;
    VertexType mVertexType = VertexType::INVALID;
    MeshLODData mLodData;
};

struct MeshGpuData {
    VGBuffer  mVao = 0;
    GLBuffer  mVbo;
    VGBuffer  mUbo = 0;
    VGBuffer  mIbo = 0;
    VGBuffer  mSSBO = 0;
    MeshLODData mLODData;
    MeshIndexType mIndexType = MeshIndexType::INVALID;
    BitFlags<MeshFlags> mFlags;
    VertexType mVertexType = VertexType::INVALID;

    void destroy();
};

// Small struct for rendering a simple Mesh.draw() with no LOD support
struct MeshMinimumRenderData {
    MeshMinimumRenderData(MeshGpuData& gpuData) : mVao(gpuData.mVao), mUbo(gpuData.mUbo), mSSBO(gpuData.mSSBO), mIndexCount(gpuData.mLODData.mTotalIndexCount), mIndexType(gpuData.mIndexType) {}
    VGBuffer  mVao;
    VGBuffer  mUbo;
    VGBuffer  mSSBO;
    ui32 mIndexCount;
    MeshIndexType mIndexType; // TODO: we could eliminate this with two separate classes or template
};
static_assert(sizeof(MeshMinimumRenderData) == 20, "Keep tiny");

// TODO - This?
//class BatchedMesh {
//public:
//    BoundingSphere getBoundingSphere() const { return BoundingSphere{ mPosition, mDrawInfo.mBoundingSphereRadius }; }
//    const f32v3& getPosition() const { return mPosition; }
//    const ModelDrawInfo& getDrawInfo() const { return mDrawInfo; }
//private:
//    ModelDrawInfo mDrawInfo;
//    f32v3 mPosition = f32v3(0.0f);
//};


class Mesh {
    friend class ProceduralMeshBuilder;
    friend class BillboardMeshBuilder;
    friend class TextMeshBuilder;
    friend class ModelMeshBuilder;
public:
    Mesh();
    virtual ~Mesh();

    VORB_NON_COPYABLE(Mesh);

    void destroy();
    bool isValid() const { return mMainMesh.mVao != 0; }

    const f32v3& getPosition() const { return mPosition; }
    void setPosition(const f32v3& position) { mPosition = position; }
    const BoundingSphere& getBoundingSphere() const { return mBoundingSphere; }
    void setBoundingSphere(const BoundingSphere& boundingSphere) { mBoundingSphere = boundingSphere; }
    MaterialRenderPassType getRenderPass() const { return mRenderPassType; }
    void setRenderPass(MaterialRenderPassType type) { mRenderPassType = type; }

    // Override allocation to use boost::singleton_pool DOESNT WORK WITH POLYMORPHISM
    //static void* operator new(size_t count);
    //static void operator delete(void* pointer, size_t size);

    // TODO: Protected
    MeshGpuData            mMainMesh;
protected:
    f32v3                  mPosition = f32v3(0.0f);
    BoundingSphere         mBoundingSphere;  ///< Optional
    MaterialRenderPassType mRenderPassType = MaterialRenderPassType::Default;
};

class SkeletalMesh : public Mesh {
    friend class ModelRepository;
public:
    const MeshSkeletonData& getSkeleton() const { return mSkeletonData; }

protected:
    MeshSkeletonData mSkeletonData;
};
//static_assert(sizeof(Mesh) == 104);