#include "stdafx.h"
#include "Mesh.h"

#include "rendering/RenderStats.h"

#include <boost/pool/singleton_pool.hpp>

#include "rendering/gl/GL.h"

struct mesh_pool {};
using singleton_task_pool = boost::singleton_pool<mesh_pool, sizeof(Mesh), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 512u>;

struct submesh_pool {};
using singleton_submesh_pool = boost::singleton_pool<submesh_pool, sizeof(MeshData), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 16u>;

Mesh::Mesh() {

}

Mesh::~Mesh() {
    destroy();
}

void Mesh::draw() const {
    assert(mMainMesh.mVao);
    assert(mMainMesh.mLODData.mTotalIndexCount);

    glBindVertexArray(mMainMesh.mVao);
    if (mMainMesh.mUbo) {
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, mMainMesh.mUbo);
    }
    if (mMainMesh.mSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, mMainMesh.mSSBO);
    }
    glDrawElements(GL_TRIANGLES, mMainMesh.mLODData.mTotalIndexCount, mMainMesh.mIndexType, (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(mMainMesh.mLODData.mTotalIndexCount / 3);

    glBindVertexArray(0);
}

void Mesh::draw(MeshLODLevel lod) const
{
    assert(mMainMesh.mVao);
    assert(mMainMesh.mLODData.mTotalIndexCount);

    MeshLODDrawInfo drawInfo = mMainMesh.mLODData.getDrawInfoForLOD(lod);
    glBindVertexArray(mMainMesh.mVao);
    if (mMainMesh.mUbo) {
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, mMainMesh.mUbo);
    }
    if (mMainMesh.mSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, mMainMesh.mSSBO);
    }

    glDrawElements(GL_TRIANGLES, drawInfo.indexCount, mMainMesh.mIndexType, (const GLvoid*)(drawInfo.startIndex * (mMainMesh.mIndexType == GL_UNSIGNED_INT ? sizeof(ui32) : sizeof(ui16))) /* offset */);
    RenderStats::recordDrawCall(drawInfo.indexCount / 3);

    glBindVertexArray(0);
}

void Mesh::drawInstanced(GLsizei instanceCount) const {
    assert(mMainMesh.mVao);
    assert(mMainMesh.mLODData.mTotalIndexCount);

    glBindVertexArray(mMainMesh.mVao);
    if (mMainMesh.mUbo) {
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, mMainMesh.mUbo);
    }
    if (mMainMesh.mSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, mMainMesh.mSSBO);
    }
    glDrawElementsInstanced(GL_TRIANGLES, mMainMesh.mLODData.mTotalIndexCount, mMainMesh.mIndexType, (const GLvoid*)(0) /* offset */, instanceCount);
    RenderStats::recordDrawCall(mMainMesh.mLODData.mTotalIndexCount / 3);

    glBindVertexArray(0);
}

void Mesh::drawInstanced(MeshLODLevel lod, GLsizei instanceCount) const {
    assert(mMainMesh.mVao);
    assert(mMainMesh.mLODData.mTotalIndexCount);

    MeshLODDrawInfo drawInfo = mMainMesh.mLODData.getDrawInfoForLOD(lod);
    glBindVertexArray(mMainMesh.mVao);
    if (mMainMesh.mUbo) {
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, mMainMesh.mUbo);
    }
    if (mMainMesh.mSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, mMainMesh.mSSBO);
    }
    glDrawElementsInstanced(GL_TRIANGLES, drawInfo.indexCount, mMainMesh.mIndexType, (const GLvoid*)(drawInfo.startIndex * (mMainMesh.mIndexType == GL_UNSIGNED_INT ? sizeof(ui32) : sizeof(ui16))) /* offset */, instanceCount);
    RenderStats::recordDrawCall(drawInfo.indexCount / 3);

    glBindVertexArray(0);
}

void Mesh::drawIndirect(size_t numDrawCommands, const GLIndirectBuffer* buffer) const
{
    assert(mMainMesh.mVao);
    assert(mMainMesh.mLODData.mTotalIndexCount);

    const MeshData* currentSubmesh = &mMainMesh;
    // Draw any submeshes
    GL.glBindVertexArray(mMainMesh.mVao);
    if (mMainMesh.mUbo) {
        GL.glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, mMainMesh.mUbo);
    }
    if (mMainMesh.mSSBO) {
        GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, mMainMesh.mSSBO);
    }
    GL.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer->getHandle());
    glMultiDrawElementsIndirect(GL_TRIANGLES, mMainMesh.mIndexType, nullptr, (GLsizei)numDrawCommands, 0);

    GL.glBindVertexArray(0);
}

void Mesh::destroy() {
    if (mMainMesh.mVao) {
        mMainMesh.destroy();
    }
}

void MeshData::destroy() {
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
    assert(IS_RENDER_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void Mesh::operator delete(void* pointer, size_t size) {
    assert(IS_RENDER_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}