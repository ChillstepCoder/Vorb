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
    assert(mMainMesh.mLODData.mTotalIndexCount);

    const SubMeshData* currentSubmesh = &mMainMesh;
    do {
        glBindVertexArray(currentSubmesh->mVao);
        if (currentSubmesh->mUbo) {
            glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, currentSubmesh->mUbo);
        }
        if (currentSubmesh->mSSBO) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, currentSubmesh->mSSBO);
        }
        glDrawElements(GL_TRIANGLES, currentSubmesh->mLODData.mTotalIndexCount, currentSubmesh->mIndexType, (const GLvoid*)(0) /* offset */);
        RenderStats::recordDrawCall(currentSubmesh->mLODData.mTotalIndexCount / 3);
        currentSubmesh = currentSubmesh->mNextSubmesh;
    } while (currentSubmesh != nullptr);

    glBindVertexArray(0);
}

void Mesh::draw(MeshLODLevel lod) const
{
    assert(mMainMesh.mVao);
    assert(mMainMesh.mLODData.mTotalIndexCount);

    const SubMeshData* currentSubmesh = &mMainMesh;
    do {
        MeshLODDrawInfo drawInfo = currentSubmesh->mLODData.getDrawInfoForLOD(lod);
        glBindVertexArray(currentSubmesh->mVao);
        if (currentSubmesh->mUbo) {
            glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, currentSubmesh->mUbo);
        }
        if (currentSubmesh->mSSBO) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, currentSubmesh->mSSBO);
        }

        glDrawElements(GL_TRIANGLES, drawInfo.indexCount, currentSubmesh->mIndexType, (const GLvoid*)(drawInfo.startIndex * (currentSubmesh->mIndexType == GL_UNSIGNED_INT ? sizeof(ui32) : sizeof(ui16))) /* offset */);
        RenderStats::recordDrawCall(drawInfo.indexCount / 3);
        currentSubmesh = currentSubmesh->mNextSubmesh;
    } while (currentSubmesh != nullptr);

    glBindVertexArray(0);
}

void Mesh::drawInstanced(GLsizei instanceCount) const {
    assert(mMainMesh.mVao);
    assert(mMainMesh.mLODData.mTotalIndexCount);

    const SubMeshData* currentSubmesh = &mMainMesh;
    // Draw any submeshes
     do {
        glBindVertexArray(currentSubmesh->mVao);
        if (currentSubmesh->mUbo) {
            glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, currentSubmesh->mUbo);
        }
        if (currentSubmesh->mSSBO) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, currentSubmesh->mSSBO);
        }
        glDrawElementsInstanced(GL_TRIANGLES, currentSubmesh->mLODData.mTotalIndexCount, currentSubmesh->mIndexType, (const GLvoid*)(0) /* offset */, instanceCount);
        RenderStats::recordDrawCall(currentSubmesh->mLODData.mTotalIndexCount / 3);
        currentSubmesh = currentSubmesh->mNextSubmesh;
    } while (currentSubmesh != nullptr);

    glBindVertexArray(0);
}

void Mesh::drawInstanced(MeshLODLevel lod, GLsizei instanceCount) const {
    assert(mMainMesh.mVao);
    assert(mMainMesh.mLODData.mTotalIndexCount);

    const SubMeshData* currentSubmesh = &mMainMesh;
    // Draw any submeshes
    do {
        MeshLODDrawInfo drawInfo = currentSubmesh->mLODData.getDrawInfoForLOD(lod);
        glBindVertexArray(currentSubmesh->mVao);
        if (currentSubmesh->mUbo) {
            glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, currentSubmesh->mUbo);
        }
        if (currentSubmesh->mSSBO) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, currentSubmesh->mSSBO);
        }
        glDrawElementsInstanced(GL_TRIANGLES, drawInfo.indexCount, currentSubmesh->mIndexType, (const GLvoid*)(drawInfo.startIndex * (currentSubmesh->mIndexType == GL_UNSIGNED_INT ? sizeof(ui32) : sizeof(ui16))) /* offset */, instanceCount);
        RenderStats::recordDrawCall(drawInfo.indexCount / 3);
        currentSubmesh = currentSubmesh->mNextSubmesh;
    } while (currentSubmesh != nullptr);

    glBindVertexArray(0);
}

void Mesh::drawIndirect(size_t numDrawCommands, const GLIndirectBuffer* buffer) const
{
    assert(mMainMesh.mVao);
    assert(mMainMesh.mLODData.mTotalIndexCount);

    const SubMeshData* currentSubmesh = &mMainMesh;
    // Draw any submeshes
    do {
        glBindVertexArray(currentSubmesh->mVao);
        if (currentSubmesh->mUbo) {
            glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, currentSubmesh->mUbo);
        }
        if (currentSubmesh->mSSBO) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, currentSubmesh->mSSBO);
        }
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer->getHandle());
        glMultiDrawElementsIndirect(GL_TRIANGLES, currentSubmesh->mIndexType, nullptr, (GLsizei)numDrawCommands, 0);
        currentSubmesh = currentSubmesh->mNextSubmesh;
    } while (currentSubmesh != nullptr);

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
        mVbo.destroy();
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