#include "stdafx.h"
#include "MeshBuilderCommon.h"

//glGenVertexArrays(1, &subMesh.mVao);
//glBindVertexArray(subMesh.mVao);
//// UBO
//glGenBuffers(1, &subMesh.mUbo);
//glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
//glBindBufferBase(GL_UNIFORM_BUFFER, 1 /*index*/, subMesh.mUbo);
//// SSBO
//glGenBuffers(1, &subMesh.mSSBO);
//glBindBuffer(GL_SHADER_STORAGE_BUFFER, subMesh.mSSBO);
//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, subMesh.mSSBO);
//// IBO
//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ProceduralMeshBuilder::sQuadIbo);
//subMesh.mFlags.setBit(MeshFlags::USING_SHARED_IBO);

void MeshBuilderCommon::initMeshBuffers(SubMeshData& subMesh, OPT VGBuffer* sharedIbo, BitFlags<MeshBuilderBufferFlags> flags) {
    // VAO
    if (subMesh.mVao == 0) {
        glGenVertexArrays(1, &subMesh.mVao);
        glBindVertexArray(subMesh.mVao);

        // Ubo
        glGenBuffers(1, &subMesh.mUbo);
        glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1 /*index*/, subMesh.mUbo);

        // VBO
        if (!flags.isBitSet(MeshBuilderBufferFlags::NO_VBO)) {
            glGenBuffers(1, &subMesh.mVbo);
            glBindBuffer(GL_ARRAY_BUFFER, subMesh.mVbo);
        }
        else {
            assert(!subMesh.mVbo);
        }
        // SSBO
        if (flags.isBitSet(MeshBuilderBufferFlags::SSBO)) {
            glGenBuffers(1, &subMesh.mSSBO);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, subMesh.mSSBO);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, subMesh.mSSBO);
        }
        else {
            assert(!subMesh.mSSBO);
        }
    }
    else {
        glBindVertexArray(subMesh.mVao);
    }
    // IBO
    if (sharedIbo) {
        // Delete old IBO if needed
        if (subMesh.mIbo && !subMesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO)) {
            glDeleteBuffers(1, &subMesh.mIbo);
        }
        subMesh.mIbo = *sharedIbo;
        subMesh.mFlags.setBit(MeshFlags::USING_SHARED_IBO);
    }
    else if (subMesh.mIbo == 0) {
        glGenBuffers(1, &subMesh.mIbo);
        subMesh.mFlags.clearBit(MeshFlags::USING_SHARED_IBO);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);

    checkGlError("MeshBuilderCommon::initMeshBuffers");
}
