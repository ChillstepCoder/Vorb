#include "stdafx.h"
#include "MeshBuilderCommon.h"

void MeshBuilderCommon::initMeshBuffers(SubMeshData& subMesh, bool allocateIbo)
{
    // VAO
    if (subMesh.mVao == 0) {
        glGenVertexArrays(1, &subMesh.mVao);
        glBindVertexArray(subMesh.mVao);
        glGenBuffers(1, &subMesh.mVbo);
        glBindBuffer(GL_ARRAY_BUFFER, subMesh.mVbo);
        glGenBuffers(1, &subMesh.mUbo);
        glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1 /*index*/, subMesh.mUbo);
    }
    else {
        glBindVertexArray(subMesh.mVao);
    }
    // IBO
    if (allocateIbo && subMesh.mIbo == 0) {
        glGenBuffers(1, &subMesh.mIbo);
        // We dont need to delete IBO if non allocating, since
        // we will have already done so in finishMesh
        // TODO: Verify this is true still
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);

    checkGlError("MeshBuilder::initMeshBuffers");
}
