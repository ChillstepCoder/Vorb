#include "stdafx.h"
#include "Mesh.h"

#include "rendering/mesh/Vertex.h"

#include <boost/pool/singleton_pool.hpp>

#include "rendering/gl/GL.h"

struct mesh_pool {};
using singleton_mesh_pool = boost::singleton_pool<mesh_pool, sizeof(Mesh), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 512u>;

Mesh::Mesh() {

}

Mesh::Mesh(Mesh&& o) {
    mPosition = std::move(o.mPosition);
    mBoundingSphere = std::move(o.mBoundingSphere);
    memcpy(&mMainMesh, &o.mMainMesh, sizeof(MeshGpuData));
    mSkeletonData = std::move(o.mSkeletonData);
    // Prevent double destroy
    o.mMainMesh.mVao = 0;
}

Mesh& Mesh::operator=(Mesh&& o) {
    mPosition = std::move(o.mPosition);
    mBoundingSphere = std::move(o.mBoundingSphere);
    memcpy(&mMainMesh, &o.mMainMesh, sizeof(MeshGpuData));
    mSkeletonData = std::move(o.mSkeletonData);
    // Prevent double destroy
    o.mMainMesh.mVao = 0;
    return *this;
}

Mesh::~Mesh() {
    destroy();
}

void Mesh::destroy() {
    mMainMesh.destroy();
}

void MeshGpuData::destroy() {
    if (mVao) {
        // glDeleteBuffers silently ignores 0
        GL.glDeleteBuffers(1, &mUbo);
        mUbo = 0;
        mVbo.destroy();
        // When using shared IBO we don't delete the IBO, which is the last buffer
        if (!mFlags.isBitSet(MeshFlags::USING_SHARED_IBO)) {
            GL.glDeleteBuffers(1, &mIbo);
        }
        mIbo = 0;
        GL.glDeleteBuffers(1, &mSSBO);
        mSSBO = 0;
        GL.glDeleteVertexArrays(1, &mVao);
        mVao = 0;
        mFlags.clearBits();
    }
}

void* Mesh::operator new(size_t count) {
    ASSERT_RENDER_THREAD();
    UNUSED(count);
    return singleton_mesh_pool::malloc();
}

void Mesh::operator delete(void* pointer, size_t size) {
    ASSERT_RENDER_THREAD();
    UNUSED(size);
    return singleton_mesh_pool::free(pointer);
}

MeshCpuData::~MeshCpuData() {
    if (mVertsPtr) {
        assert(mElementsPtr); // For now we always guarantee both
        switch (mVertexType) {
            case VertexType::TERRAIN:
                delete[] static_cast<TerrainVertex*>(mVertsPtr);
                break;
            case VertexType::WATER:
                delete[] static_cast<WaterVertex*>(mVertsPtr);
                break;
            case VertexType::STATIC_MODEL:
                delete[] static_cast<StaticModelVertex*>(mVertsPtr);
                break;
            case VertexType::SKINNED_MODEL:
                delete[] static_cast<SkinnedModelVertex*>(mVertsPtr);
                break;
            default:
                assert(false);
        }
        static_assert(e_cast(VertexType::COUNT) == 5, "Delete new types");
        switch (mIndexType) {
            case MeshIndexType::SHORT:
                delete[] static_cast<ui16*>(mElementsPtr);
                break;
            case MeshIndexType::INT:
                delete[] static_cast<ui32*>(mElementsPtr);
                break;
            default:
                assert(false);
        }
    }
}

MeshCpuData::MeshCpuData(MeshCpuData&& o) {
    this->mVertsPtr = o.mVertsPtr;
    this->mElementsPtr = o.mElementsPtr;
    this->mVertsCount = o.mVertsCount;
    this->mLodData = o.mLodData;
    this->mIndexType = o.mIndexType;
    this->mVertexType = o.mVertexType;
    o.mVertsPtr = nullptr;
    o.mElementsPtr = nullptr;
}

MeshCpuData& MeshCpuData::operator=(MeshCpuData&& o) {
    this->mVertsPtr = o.mVertsPtr;
    this->mElementsPtr = o.mElementsPtr;
    this->mVertsCount = o.mVertsCount;
    this->mLodData = o.mLodData;
    this->mIndexType = o.mIndexType;
    this->mVertexType = o.mVertexType;
    o.mVertsPtr = nullptr;
    o.mElementsPtr = nullptr;
    return *this;
}
