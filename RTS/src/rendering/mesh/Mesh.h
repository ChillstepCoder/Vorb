#pragma once

#include "rendering/mesh/VertexType.h"
#include "rendering/mesh/MeshSkeletonData.h"
#include "rendering/model/MaterialRenderPassType.h"
#include "rendering/model/ModelSubmeshData.h"

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
    Mesh() = default;
    virtual ~Mesh();

    VORB_NON_COPYABLE(Mesh);

    void destroy();
    bool isValid() const { return mGpuData.mVao != 0; }

    const f32v3& getPosition() const { return mPosition; }
    void setPosition(const f32v3& position) { mPosition = position; }
    const BoundingSphere& getBoundingSphere() const { return mBoundingSphere; }
    void setBoundingSphere(const BoundingSphere& boundingSphere) { mBoundingSphere = boundingSphere; }
    MaterialRenderPassType getRenderPass() const { return mRenderPassType; }
    void setRenderPass(MaterialRenderPassType type) { mRenderPassType = type; }
    const ModelSubmeshData* getSubmeshData() const { return mSubmeshData; }
    void setSubmeshData(const ModelSubmeshData* data) { mSubmeshData = data; }
    void bindModelAttribs() const;

    bool castsShadow() const {
        return mRenderPassType != MaterialRenderPassType::Water;
    }

    // Override allocation to use boost::singleton_pool DOESNT WORK WITH POLYMORPHISM
    //static void* operator new(size_t count);
    //static void operator delete(void* pointer, size_t size);

    // TODO: Protected
    MeshGpuData            mGpuData;
    VGBuffer               mVariantDataUbo = 0; // Managed by ModelDef
protected:
    f32v3                  mPosition = f32v3(0.0f);
    BoundingSphere         mBoundingSphere;  ///< Optional
    MaterialRenderPassType mRenderPassType = MaterialRenderPassType::Default;
    const ModelSubmeshData* mSubmeshData = nullptr;
    mutable bool mHasModelAttribsBound = false; // Used by instanced model renderers
};

class TerrainMesh : public Mesh {
public:
    TerrainMesh(ui32 patchIndex) : mIndex(patchIndex) {};

    VORB_NON_COPYABLE_BUT_MOVABLE(TerrainMesh);

    ui32 mIndex;
    std::atomic<f32> mCrossfadeAlpha = 0.0f;
    std::atomic_int mCrossfadeDir = 0; // -1 = down, 0 = none, 1 = up
    f32v2 mUVRoot;
};

class SkeletalMesh : public Mesh {
    friend class ModelRepository;
public:
    const MeshSkeletonData& getSkeleton() const { return mSkeletonData; }

protected:
    MeshSkeletonData mSkeletonData;
};
//static_assert(sizeof(Mesh) == 104);