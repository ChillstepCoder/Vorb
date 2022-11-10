#include "stdafx.h"
#include "MeshBuilderCommon.h"


#include "rendering/texture/SubTexture.h"

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

void MeshBuilderCommon::uploadIndexData(SubMeshData& subMesh, const std::vector<ui32>& indices, MeshDrawMode drawMode) {
    subMesh.mIndexCount = indices.size();
    const ui32 indexBufferSizeBytes = subMesh.mIndexCount * sizeof(ui32);
    assert(subMesh.mIbo);
    assert(!subMesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO));
    // Allocate orphaned
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexBufferSizeBytes, indices.data());
}

void MeshBuilderCommon::uploadIndexData(SubMeshData& subMesh, const ui16* indices, int indexCount, MeshDrawMode drawMode) {
    subMesh.mIndexCount = indexCount;
    subMesh.mIndexType = GL_UNSIGNED_SHORT;
    const ui32 indexBufferSizeBytes = indexCount * sizeof(ui16);
    assert(subMesh.mIbo);
    assert(!subMesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO));
    // Allocate orphaned
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexBufferSizeBytes, indices);
}

template<typename VERTEX>
void MeshBuilderCommon::uploadVertexData(SubMeshData& subMesh, const std::vector<VERTEX>& vertices, MeshDrawMode drawMode) {
    const unsigned bufferSizeBytes = vertices.size() * sizeof(VERTEX);
    // VBO Allocate orphaned
    assert(subMesh.mVbo);
    glBindBuffer(GL_ARRAY_BUFFER, subMesh.mVbo);
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, vertices.data());
}
template void MeshBuilderCommon::uploadVertexData(SubMeshData& subMesh, const std::vector<Vertex32>& vertices, MeshDrawMode drawMode);
template void MeshBuilderCommon::uploadVertexData(SubMeshData& subMesh, const std::vector<Vertex96>& vertices, MeshDrawMode drawMode);

void MeshBuilderCommon::uploadStandardTextureUboData(SubMeshData& subMesh, const f32v3& pos, const std::vector<TextureHandle>& textures, MeshDrawMode drawMode) {
    // UBO
    const ui32 uboSizeBytes = sizeof(f32v4) + textures.size() * sizeof(TextureHandle);
    // Pack into uvec2 - https://www.khronos.org/opengl/wiki/Bindless_Texture
    // With position in front
    constexpr size_t BUFFER_SIZE = sizeof(f32v4) + MAX_TEXTURES_PER_MESH * 2 * sizeof(ui32v2);
    ui8 byteBuffer[BUFFER_SIZE];
    *(f32v3*)byteBuffer = pos;
    ui32v2* buffer = (ui32v2*)(byteBuffer + sizeof(f32v4));
    for (ui32 i = 0; i < textures.size(); ++i) {
        TextureHandle handle = textures[i];
        buffer[i].x = handle & 0xffffffff;
        buffer[i].y = handle >> 32;
    }
    assert(subMesh.mUbo);
    // Allocate orphaned
    glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
    glBufferData(GL_UNIFORM_BUFFER, uboSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_UNIFORM_BUFFER, 0, uboSizeBytes, byteBuffer);
}
