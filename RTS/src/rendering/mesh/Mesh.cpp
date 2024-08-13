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

void Mesh::bindStaticModelAttribs() const {
    assert(mGpuData.mVao);
    if (mCurrentAttribBinding != AttribBinding::Static) [[unlikely]] {
        unbindCurrentAttribs();
        mCurrentAttribBinding = AttribBinding::Static;
        // Transforms
        glEnableVertexArrayAttrib(mGpuData.mVao, 7);
        glEnableVertexArrayAttrib(mGpuData.mVao, 8);
        glEnableVertexArrayAttrib(mGpuData.mVao, 9);
        glEnableVertexArrayAttrib(mGpuData.mVao, 10);
        glVertexArrayAttribFormat(mGpuData.mVao, 7, 4, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribFormat(mGpuData.mVao, 8, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4));
        glVertexArrayAttribFormat(mGpuData.mVao, 9, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 2);
        glVertexArrayAttribFormat(mGpuData.mVao, 10, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 3);
        glVertexArrayAttribBinding(mGpuData.mVao, 7, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(mGpuData.mVao, 8, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(mGpuData.mVao, 9, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayAttribBinding(mGpuData.mVao, 10, MODEL_TRANSFORMS_BINDING_POINT);
        glVertexArrayBindingDivisor(mGpuData.mVao, MODEL_TRANSFORMS_BINDING_POINT, 1);

        // Variants
        glEnableVertexArrayAttrib(mGpuData.mVao, 11);
        glVertexArrayAttribIFormat(mGpuData.mVao, 11, 1, GL_UNSIGNED_BYTE, 0);
        glVertexArrayAttribBinding(mGpuData.mVao, 11, MODEL_VARIANT_INDICES_BINDING_POINT);
        glVertexArrayBindingDivisor(mGpuData.mVao, MODEL_VARIANT_INDICES_BINDING_POINT, 1);

        // Damage Model
        glEnableVertexArrayAttrib(mGpuData.mVao, 13);
        glVertexArrayAttribIFormat(mGpuData.mVao, 13, 1, GL_UNSIGNED_INT, 0);
        glVertexArrayAttribBinding(mGpuData.mVao, 13, MODEL_DAMAGE_INDICES_BINDING_POINT);
        glVertexArrayBindingDivisor(mGpuData.mVao, MODEL_DAMAGE_INDICES_BINDING_POINT, 1);
    }
}

void Mesh::bindDynamicModelAttribs() const {
    assert(mGpuData.mVao);
    if (mCurrentAttribBinding != AttribBinding::Dynamic) [[unlikely]] {
        // TODO: This can result in unbinding and rebinding shared static attribs
        unbindCurrentAttribs();
        mCurrentAttribBinding = AttribBinding::Dynamic;
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
        glVertexArrayAttribBinding(mGpuData.mVao, 11, MODEL_VARIANT_INDICES_BINDING_POINT);
        glVertexArrayBindingDivisor(mGpuData.mVao, MODEL_VARIANT_INDICES_BINDING_POINT, 1);
    }
}

void Mesh::unbindStaticModelAttribs() const {
    assert(mCurrentAttribBinding == AttribBinding::Static);
    mCurrentAttribBinding = AttribBinding::None;
    glDisableVertexArrayAttrib(mGpuData.mVao, 7);
    glDisableVertexArrayAttrib(mGpuData.mVao, 8);
    glDisableVertexArrayAttrib(mGpuData.mVao, 9);
    glDisableVertexArrayAttrib(mGpuData.mVao, 10);
    glDisableVertexArrayAttrib(mGpuData.mVao, 11);
    glDisableVertexArrayAttrib(mGpuData.mVao, 12);
    glDisableVertexArrayAttrib(mGpuData.mVao, 13);
}

void Mesh::unbindDynamicModelAttribs() const {
    assert(mCurrentAttribBinding == AttribBinding::Dynamic);
    mCurrentAttribBinding = AttribBinding::None;
    glDisableVertexArrayAttrib(mGpuData.mVao, 7);
    glDisableVertexArrayAttrib(mGpuData.mVao, 8);
    glDisableVertexArrayAttrib(mGpuData.mVao, 9);
    glDisableVertexArrayAttrib(mGpuData.mVao, 10);
    glDisableVertexArrayAttrib(mGpuData.mVao, 11);
    glDisableVertexArrayAttrib(mGpuData.mVao, 12);
}

void Mesh::bindSkeletalModelAttribs() const {
    assert(mGpuData.mVao);
    if (mCurrentAttribBinding != AttribBinding::Skeletal) [[unlikely]] {
        unbindCurrentAttribs();
        mCurrentAttribBinding = AttribBinding::Skeletal;
        glEnableVertexArrayAttrib(mGpuData.mVao, 11); // Bone weights
        glEnableVertexArrayAttrib(mGpuData.mVao, 12); // Bone ids
    }
}

void Mesh::unbindSkeletalModelAttribs() const {
    assert(mCurrentAttribBinding == AttribBinding::Skeletal);
    mCurrentAttribBinding = AttribBinding::None;
    glDisableVertexArrayAttrib(mGpuData.mVao, 11); // Bone weights
    glDisableVertexArrayAttrib(mGpuData.mVao, 12); // Bone ids
}

void Mesh::unbindCurrentAttribs() const {
    switch (mCurrentAttribBinding) {
        case AttribBinding::Static:
            unbindStaticModelAttribs();
            break;
        case AttribBinding::Dynamic:
            unbindDynamicModelAttribs();
            break;
        case AttribBinding::Skeletal:
            unbindSkeletalModelAttribs();
            break;
        default:
            break;
    }
    static_assert(e_count(AttribBinding) == 4, "Update unbindCurrentAttribs");
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
            case VertexType::STANDARD_MODEL:
                delete[] static_cast<StandardModelVertex*>(mVertsPtr);
                break;
            case VertexType::SKINNED_MODEL:
                delete[] static_cast<SkinnedModelVertex*>(mVertsPtr);
                break;
            default:
                assert(false);
        }
        static_assert(e_cast(VertexType::COUNT) == 5, "Delete new types");
        switch (mIndexType) {
            case MeshIndexType::USHORT:
                delete[] static_cast<ui16*>(mElementsPtr);
                break;
            case MeshIndexType::UINT:
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
    this->mElementsCount = o.mElementsCount;
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
    this->mElementsCount = o.mElementsCount;
    this->mLodData = o.mLodData;
    this->mIndexType = o.mIndexType;
    this->mVertexType = o.mVertexType;
    o.mVertsPtr = nullptr;
    o.mElementsPtr = nullptr;
    return *this;
}

TerrainMesh::~TerrainMesh() {
    glDeleteTextures(1, &mTerrainSurfaceDataTexture);
}
