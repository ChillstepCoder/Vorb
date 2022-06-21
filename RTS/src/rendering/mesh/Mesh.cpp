#include "stdafx.h"
#include "Mesh.h"

#include "rendering/RenderStats.h"

#include <boost/pool/singleton_pool.hpp>

struct mesh_pool {};
using singleton_task_pool = boost::singleton_pool<mesh_pool, sizeof(Mesh), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 512u>;

Mesh::Mesh() {

}

Mesh::~Mesh() {
    destroy();
}

void Mesh::draw() const {
    assert(mMainMesh.mVao);
    assert(mMainMesh.mIndexCount);

    // Draw main mesh
    glBindVertexArray(mMainMesh.mVao);
    // TODO: Why do we have to do this every call
    // texture UBOs go at index 1 since globalUBO is index 0
    if (mMainMesh.mUbo) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 1 /*index*/, mMainMesh.mUbo);
    }
    if (mMainMesh.mSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2 /*index*/, mMainMesh.mSSBO);
    }
    glDrawElements(GL_TRIANGLES, mMainMesh.mIndexCount, mMainMesh.mIndexType, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mMainMesh.mIndexCount / 3);

    // Draw any submeshes
    for (auto&& submesh : mSubMeshes) {
        glBindVertexArray(submesh.mVao);
        glDrawElements(GL_TRIANGLES, submesh.mIndexCount, submesh.mIndexType, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(mMainMesh.mIndexCount / 3);
    }

    glBindVertexArray(0);
}

void Mesh::destroy() {
    if (mMainMesh.mVao) {
        const bool isUsingShared = mFlags.isBitSet(MeshFlags::USING_SHARED_IBO);
        // When using shared IBO we don't delete the IBO, which is the last buffer
        if (!isUsingShared){
            glDeleteBuffers(1, &mMainMesh.mIbo);
        }
        mMainMesh.mIbo = 0;
        if (mMainMesh.mUbo) {
            glDeleteBuffers(1, &mMainMesh.mUbo);
            mMainMesh.mUbo = 0;
        }
        if (mMainMesh.mSSBO) {
            glDeleteBuffers(1, &mMainMesh.mSSBO);
            mMainMesh.mSSBO = 0;
        }
        if (mMainMesh.mVbo) {
            glDeleteBuffers(1, &mMainMesh.mVbo);
            mMainMesh.mVbo = 0;
        }
        glDeleteVertexArrays(1, &mMainMesh.mVao);
        mMainMesh.mVao = 0;
        mMainMesh.mIndexCount = 0;
        for (auto&& subMesh : mSubMeshes) {
            subMesh.destroy(isUsingShared);
        }
        mSubMeshes.clear();
    }
}

void SubMeshData::destroy(bool isUsingSharedIbo) {
    if (mVao) {
        if (mVbo) {
            glDeleteBuffers(1, &mVbo);
        }
        // When using shared IBO we don't delete the IBO, which is the last buffer
        if (!isUsingSharedIbo) {
            glDeleteBuffers(1, &mIbo);
        }
        if (mUbo) {
            glDeleteBuffers(1, &mUbo);
        }
        if (mSSBO) {
            glDeleteBuffers(1, &mSSBO);
        }
        glDeleteVertexArrays(1, &mVao);
        mVao = 0;
    }
}

void* Mesh::operator new(size_t count) {
    assert(IS_MAIN_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void Mesh::operator delete(void* pointer, size_t size) {
    assert(IS_MAIN_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}