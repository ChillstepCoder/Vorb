#include "stdafx.h"
#include "Mesh.h"

#include "rendering/mesh/Vertex.h"

#include <boost/pool/singleton_pool.hpp>

#include "rendering/gl/GL.h"
#include "rendering/model/ModelConst.h"

//struct mesh_pool {};
//using singleton_mesh_pool = boost::singleton_pool<mesh_pool, sizeof(Mesh), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 512u>;

Mesh::~Mesh() {
    destroy();
}

void Mesh::destroy() {
    mGpuData.destroy();
}

void Mesh::bindModelAttribs() const {
    assert(mGpuData.mVao);
    if (!mHasModelAttribsBound) [[unlikely]] {
        mHasModelAttribsBound = true;
        // Transforms
        glEnableVertexArrayAttrib(mGpuData.mVao, 7);
        glEnableVertexArrayAttrib(mGpuData.mVao, 8);
        glEnableVertexArrayAttrib(mGpuData.mVao, 9);
        glEnableVertexArrayAttrib(mGpuData.mVao, 10);
        glVertexArrayAttribFormat(mGpuData.mVao, 7, 4, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(mGpuData.mVao, 8, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4));
        glVertexArrayAttribFormat(mGpuData.mVao, 9, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 2.0f);
        glVertexArrayAttribFormat(mGpuData.mVao, 10, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 3.0f);
        glVertexArrayAttribBinding(mGpuData.mVao, 7, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(mGpuData.mVao, 8, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(mGpuData.mVao, 9, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(mGpuData.mVao, 10, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayBindingDivisor(mGpuData.mVao, MODEL_TRANSFORMS_BINDING_POINT, 1);

        // Variants
        glEnableVertexArrayAttrib(mGpuData.mVao, 11);
        glVertexArrayAttribIFormat(mGpuData.mVao, 11, 1, GL_UNSIGNED_BYTE, 0);
        glVertexArrayAttribBinding(mGpuData.mVao, 11, MODEL_VARIANTS_BINDING_POINT);
        glVertexArrayBindingDivisor(mGpuData.mVao, MODEL_VARIANTS_BINDING_POINT, 1);
    }
}

void Mesh::unbindModelAttribs() const {
    mHasModelAttribsBound = false;
    glDisableVertexArrayAttrib(mGpuData.mVao, 7);
    glDisableVertexArrayAttrib(mGpuData.mVao, 8);
    glDisableVertexArrayAttrib(mGpuData.mVao, 9);
    glDisableVertexArrayAttrib(mGpuData.mVao, 10);
    glDisableVertexArrayAttrib(mGpuData.mVao, 11);
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

//void* Mesh::operator new(size_t count) {
//    ASSERT_RENDER_THREAD();
//    UNUSED(count);
//    return singleton_mesh_pool::malloc();
//}
//
//void Mesh::operator delete(void* pointer, size_t size) {
//    ASSERT_RENDER_THREAD();
//    UNUSED(size);
//    return singleton_mesh_pool::free(pointer);
//}

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
