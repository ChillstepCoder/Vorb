#pragma once

#include "rendering/mesh/VertexType.h"
#include "rendering/mesh/MeshSkeletonData.h"
#include "rendering/model/MaterialRenderPassType.h"
#include "rendering/model/ModelSubmeshData.h"
#include "rendering/mesh/MeshLODData.h"
#include "rendering/mesh/MeshIndexType.h"

enum class MeshDrawMode {
    DYNAMIC = GL_DYNAMIC_DRAW,
    STREAM = GL_STREAM_DRAW,
    STATIC = GL_STATIC_DRAW
};

enum class MeshFlags : ui8 {
    USING_SHARED_IBO = 1 << 0
};



class MeshCpuData final {
public:
    MeshCpuData() = default;
    VORB_NON_COPYABLE(MeshCpuData);
    ~MeshCpuData();
    MeshCpuData(MeshCpuData&& o);
    MeshCpuData& operator=(MeshCpuData&& o);

    // These are cleaned up in destructor
    void* mVertsPtr = nullptr;
    void* mElementsPtr = nullptr;
    ui32 mVertsCount = 0;
    ui32 mElementsCount = 0;
    MeshIndexType mIndexType = MeshIndexType::INVALID;
    VertexType mVertexType = VertexType::INVALID;
    MeshLODData mLodData;
    // MAKE SURE TO UPDATE MOVE CONSTRUCTOR IF YOU CHANGE THIS DATA
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

    ui32 getIndexSizeBytes() const { return mIndexType == MeshIndexType::UINT ? sizeof(ui32) : sizeof(ui16); }
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
    void bindStaticModelAttribs() const;
    void unbindStaticModelAttribs() const;
    void bindSkeletalModelAttribs() const;
    void unbindSkeletalModelAttribs() const;
    void unbindCurrentAttribs() const;

    bool castsShadow() const {
        return mRenderPassType != MaterialRenderPassType::Water;
    }

    // Override allocation to use boost::singleton_pool DOESNT WORK WITH POLYMORPHISM
    //static void* operator new(size_t count);
    //static void operator delete(void* pointer, size_t size);

    // TODO: Protected
    MeshCpuData            mCpuData;
    MeshGpuData            mGpuData;
    VGBuffer               mVariantDataUbo = 0; // Managed by ModelDef // TODO: REMOVE
protected:

    mutable AttribBinding mCurrentAttribBinding = AttribBinding::None;

    f32v3                  mPosition = f32v3(0.0f);
    BoundingSphere         mBoundingSphere;  ///< Optional
    MaterialRenderPassType mRenderPassType = MaterialRenderPassType::Default;
    const ModelSubmeshData* mSubmeshData = nullptr;
};

class TerrainMesh : public Mesh {
public:
    TerrainMesh(ui32 patchIndex) : mIndex(patchIndex) {};
    ~TerrainMesh();

    VORB_NON_COPYABLE(TerrainMesh);

    ui32 mIndex;
    std::atomic<f32> mCrossfadeAlpha = 0.0f;
    std::atomic_int mCrossfadeDir = 0; // -1 = down, 0 = none, 1 = up
    f32v2 mUVRoot;
    VGTexture mTerrainSurfaceDataTexture = 0;
};

class SkeletalMesh : public Mesh {
    friend class ModelRepository;
public:
    MeshSkeletonData& getSkeletonData() { return mSkeletonData; }
    const MeshSkeletonData& getSkeletonData() const { return mSkeletonData; }


protected:
    MeshSkeletonData mSkeletonData;
};
