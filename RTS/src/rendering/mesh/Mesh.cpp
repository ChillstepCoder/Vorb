#include "stdafx.h"
#include "Mesh.h"

#include "rendering/RenderStats.h"

#include <boost/pool/singleton_pool.hpp>

struct mesh_pool {};
using singleton_task_pool = boost::singleton_pool<mesh_pool, sizeof(Mesh), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 512u>;

struct submesh_pool {};
using singleton_submesh_pool = boost::singleton_pool<submesh_pool, sizeof(SubMeshData), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 16u>;

void* SubMeshData::operator new(size_t count) {
    UNUSED(count);
    assert(IS_RENDER_THREAD());
    return singleton_submesh_pool::malloc();
}

void SubMeshData::operator delete(void* pointer, size_t size) {
    UNUSED(size);
    assert(IS_RENDER_THREAD());
    return singleton_submesh_pool::free(pointer);
}

void SubMeshData::allocateSubmeshCount(size_t count)
{
    SubMeshData* subMesh = mNextSubmesh;
    int i = 0;
    bool didDelete = false;
    // Delete old submeshes
    while (subMesh != nullptr) {
        if (i++ >= count) {
            SubMeshData* prevSubMesh = subMesh;
            subMesh = prevSubMesh->mNextSubmesh;
            prevSubMesh->destroy();
            delete prevSubMesh;
            didDelete = true;
        }
    }
    // Allocate new submeshes
    if (i < count) {
        mNextSubmesh = new SubMeshData;
        subMesh = mNextSubmesh;
        while (++i < count) {
            subMesh->mNextSubmesh = new SubMeshData;
            subMesh = subMesh->mNextSubmesh;
        }
        subMesh->mNextSubmesh = nullptr;
    } else if (i == 0) {
        mNextSubmesh = nullptr;
    }
    else if (didDelete) {
        // Append the nullptr tail
        subMesh = mNextSubmesh;
        int i = 0;
        while (++i < count) {
            subMesh = mNextSubmesh;
        }
        subMesh->mNextSubmesh = nullptr;
    }
}

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
    const SubMeshData* currentSubmesh = &mMainMesh;
    glDrawElements(GL_TRIANGLES, mMainMesh.mIndexCount, mMainMesh.mIndexType, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mMainMesh.mIndexCount / 3);
    currentSubmesh = currentSubmesh->mNextSubmesh;
    // Draw any submeshes
    while (currentSubmesh != nullptr) {
        glBindVertexArray(currentSubmesh->mVao);
        glDrawElements(GL_TRIANGLES, currentSubmesh->mIndexCount, currentSubmesh->mIndexType, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(currentSubmesh->mIndexCount / 3);
        currentSubmesh = currentSubmesh->mNextSubmesh;
    }

    glBindVertexArray(0);
}

void Mesh::destroy() {
    if (mMainMesh.mVao) {
        SubMeshData* subMesh = &mMainMesh;
        do {
            SubMeshData* prevSubMesh = subMesh;
            subMesh = prevSubMesh->mNextSubmesh;
            prevSubMesh->destroy();
            delete prevSubMesh;
        } while (subMesh != nullptr);
    }
}

void SubMeshData::destroy() {
    if (mVao) {
        // glDeleteBuffers silently ignores 0
        glDeleteBuffers(1, &mUbo);
        mUbo = 0;
        glDeleteBuffers(1, &mVbo);
        mVbo = 0;
        // When using shared IBO we don't delete the IBO, which is the last buffer
        if (!mFlags.isBitSet(MeshFlags::USING_SHARED_IBO)) {
            glDeleteBuffers(1, &mIbo);
        }
        mIbo = 0;
        glDeleteBuffers(1, &mSSBO);
        mSSBO = 0;
        glDeleteVertexArrays(1, &mVao);
        mVao = 0;
        mFlags.clearBits();
    }
}

void* Mesh::operator new(size_t count) {
    assert(IS_RENDER_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void Mesh::operator delete(void* pointer, size_t size) {
    assert(IS_RENDER_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}