#include "stdafx.h"
#include "MeshDrawer.h"

#include "rendering/gl/GL.h"
#include "rendering/RenderStats.h"

void MeshDrawer::draw(const MeshGpuData& meshData) {
    assert(meshData.mVao);
    assert(meshData.mLODData.mTotalIndexCount);
    assert(meshData.mIndexType != MeshIndexType::INVALID);

    glBindVertexArray(meshData.mVao);
    if (meshData.mUbo) {
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, meshData.mUbo);
    }
    if (meshData.mSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, meshData.mSSBO);
    }
    glDrawElements(GL_TRIANGLES, meshData.mLODData.mTotalIndexCount, e_cast(meshData.mIndexType), (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(meshData.mLODData.mTotalIndexCount / 3);
}

void MeshDrawer::draw(const MeshGpuData& meshData, MeshLODLevel lod) {
    assert(meshData.mVao);
    assert(meshData.mLODData.mTotalIndexCount);

    MeshLODDrawInfo drawInfo = meshData.mLODData.getDrawInfoForLOD(lod);
    glBindVertexArray(meshData.mVao);
    if (meshData.mUbo) {
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, meshData.mUbo);
    }
    if (meshData.mSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, meshData.mSSBO);
    }

    glDrawElements(GL_TRIANGLES, drawInfo.indexCount, e_cast(meshData.mIndexType), (const GLvoid*)(drawInfo.startIndex * (meshData.mIndexType == MeshIndexType::INT ? sizeof(ui32) : sizeof(ui16))) /* offset */);
    RenderStats::recordDrawCall(drawInfo.indexCount / 3);
}

void MeshDrawer::drawInstanced(const MeshGpuData& meshData, GLsizei instanceCount) {
    assert(meshData.mVao);
    assert(meshData.mLODData.mTotalIndexCount);

    glBindVertexArray(meshData.mVao);
    if (meshData.mUbo) {
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, meshData.mUbo);
    }
    if (meshData.mSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, meshData.mSSBO);
    }
    glDrawElementsInstanced(GL_TRIANGLES, meshData.mLODData.mTotalIndexCount, e_cast(meshData.mIndexType), (const GLvoid*)(0) /* offset */, instanceCount);
    RenderStats::recordDrawCall(meshData.mLODData.mTotalIndexCount / 3);

}

void MeshDrawer::drawInstanced(const MeshGpuData& meshData, MeshLODLevel lod, GLsizei instanceCount) {
    assert(meshData.mVao);
    assert(meshData.mLODData.mTotalIndexCount);

    MeshLODDrawInfo drawInfo = meshData.mLODData.getDrawInfoForLOD(lod);
    glBindVertexArray(meshData.mVao);
    if (meshData.mUbo) {
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, meshData.mUbo);
    }
    if (meshData.mSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, meshData.mSSBO);
    }
    glDrawElementsInstanced(GL_TRIANGLES, drawInfo.indexCount, e_cast(meshData.mIndexType), (const GLvoid*)(drawInfo.startIndex * (meshData.mIndexType == MeshIndexType::INT ? sizeof(ui32) : sizeof(ui16))) /* offset */, instanceCount);
    RenderStats::recordDrawCall(drawInfo.indexCount / 3);
}

void MeshDrawer::drawIndirect(const MeshGpuData& meshData, const GLDrawCommandBuffer* buffer) {
    assert(meshData.mVao);
    assert(meshData.mLODData.mTotalIndexCount);

    const MeshGpuData* currentSubmesh = &meshData;
    // Draw any submeshes
    GL.glBindVertexArray(meshData.mVao);
    if (meshData.mUbo) {
        GL.glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, meshData.mUbo);
    }
    if (meshData.mSSBO) {
        GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, meshData.mSSBO);
    }
    buffer->multiDrawElementsIndirect(GL_TRIANGLES, e_cast(meshData.mIndexType));
}

void MeshDrawer::drawMinimum(const MeshMinimumRenderData& meshData) {
    assert(meshData.mVao);
    assert(meshData.mIndexCount);
    assert(meshData.mIndexType != MeshIndexType::INVALID);

    glBindVertexArray(meshData.mVao);
    if (meshData.mUbo) {
        glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MESH_UBO, meshData.mUbo);
    }
    if (meshData.mSSBO) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MESH_SSBO, meshData.mSSBO);
    }
    glDrawElements(GL_TRIANGLES, meshData.mIndexCount, e_cast(meshData.mIndexType), (const GLvoid*)(0) /* offset */);
    RenderStats::recordDrawCall(meshData.mIndexCount / 3);
}
