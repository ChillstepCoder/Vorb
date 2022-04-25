#include "stdafx.h"
#include "Mesh.h"

#include "rendering/RenderStats.h"

Mesh::Mesh() {

}

Mesh::~Mesh() {

}

void Mesh::draw() const {
    assert(mMainMesh.mVao);
    assert(mMainMesh.mIndexCount);

    // Draw main mesh
    glBindVertexArray(mMainMesh.mVao);
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
        glDeleteBuffers(1, &mMainMesh.mVbo);
        // When using shared IBO we don't delete the IBO, which is the last buffer
        if (!isUsingShared){
            glDeleteBuffers(1, &mMainMesh.mIbo);
        }
        if (mMainMesh.mTextureUbo) {
            glDeleteBuffers(1, &mMainMesh.mTextureUbo);
        }
        mMainMesh.mVao = 0;
        for (auto&& subMesh : mSubMeshes) {
            subMesh.destroy(isUsingShared);
        }
        mSubMeshes.clear();
    }
}

void SubMeshData::destroy(bool isUsingSharedIbo) {
    if (mVao) {
        glDeleteBuffers(1, &mVbo);
        // When using shared IBO we don't delete the IBO, which is the last buffer
        if (!isUsingSharedIbo) {
            glDeleteBuffers(1, &mIbo);
        }
        if (mTextureUbo) {
            glDeleteBuffers(1, &mTextureUbo);
        }
        mVao = 0;
    }
}
