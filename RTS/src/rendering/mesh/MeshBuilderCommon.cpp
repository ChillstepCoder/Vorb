#include "stdafx.h"
#include "MeshBuilderCommon.h"

#include <meshoptimizer.h>

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

template<typename VERTEX>
void MeshBuilderCommon::optimizeMeshAndGenerateLODs(SubMeshData& subMesh, std::vector<ui16>& indices, std::vector<VERTEX>& vertices) {
    static_assert(sizeof(unsigned int) == sizeof(ui32));
    std::vector<ui32> indicesUi32;
    indicesUi32.resize(indices.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indicesUi32[i] = (ui32)indices[i];
    }

    optimizeMeshAndGenerateLODs(subMesh, indicesUi32, vertices);

    indices.resize(indicesUi32.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = (ui16)indicesUi32[i];
    }
}
template void MeshBuilderCommon::optimizeMeshAndGenerateLODs(SubMeshData& subMesh, std::vector<ui16>& indices, std::vector<Vertex32>& vertices);
template void MeshBuilderCommon::optimizeMeshAndGenerateLODs(SubMeshData& subMesh, std::vector<ui16>& indices, std::vector<Vertex96>& vertices);

template<typename VERTEX>
void MeshBuilderCommon::optimizeMeshAndGenerateLODs(SubMeshData& subMesh, std::vector<ui32>& indices, std::vector<VERTEX>& vertices) {
    PROFILE_FUNCTION();
    assert(!subMesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO));

    static_assert(sizeof(unsigned int) == sizeof(ui32));

    std::vector<ui32> remap(indices.size());
    const size_t vertexCount = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(VERTEX));

    std::vector<ui32> remappedIndices(indices.size());
    std::vector<VERTEX> remappedVertices(vertexCount);

    meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(), remap.data());
    meshopt_remapVertexBuffer(remappedVertices.data(), vertices.data(), vertices.size(), sizeof(VERTEX), remap.data());

    meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), indices.size(), vertexCount);
    // TODO: THIS ASSUMES POSITION IS ALWAYS THE FIRST FIELD! It better be :P
    meshopt_optimizeOverdraw(remappedIndices.data(), remappedIndices.data(), indices.size(), (const f32*)(&remappedVertices[0]), vertexCount, sizeof(VERTEX), 1.05f);
    meshopt_optimizeVertexFetch(remappedVertices.data(), remappedIndices.data(), indices.size(), remappedVertices.data(), vertexCount, sizeof(VERTEX));

    const float threshold = 0.2f;

    constexpr float targetErrors[3]{
        0.01f,
        0.02f,
        0.04f
    };

    // Level of detail
    MeshLODData& lodData = subMesh.mLODData;
    lodData.mLODStarts[0] = 0;
    size_t prevSize = remappedIndices.size();
    size_t prevStart = 0;
    for (int i = 1; i < 4; ++i) {
        const float targetError = targetErrors[i - 1];
        size_t prevTotalSize = remappedIndices.size();
        const size_t target_index_count = size_t(prevSize * threshold);
        lodData.mLODStarts[i] = prevTotalSize;
        size_t maxLODSize = prevSize;
        remappedIndices.resize(prevTotalSize + maxLODSize);
        prevSize = meshopt_simplify(&remappedIndices[prevTotalSize], &remappedIndices[prevStart], prevSize, (const f32*)(&remappedVertices[0]), vertexCount, sizeof(VERTEX), target_index_count, targetError);
        prevStart = prevTotalSize;
        remappedIndices.resize(prevTotalSize + prevSize);
    }
    lodData.mTotalIndexCount = remappedIndices.size();

    indices.swap(remappedIndices);
    vertices.swap(remappedVertices);
}
template void MeshBuilderCommon::optimizeMeshAndGenerateLODs(SubMeshData& subMesh, std::vector<ui32>& indices, std::vector<Vertex32>& vertices);
template void MeshBuilderCommon::optimizeMeshAndGenerateLODs(SubMeshData& subMesh, std::vector<ui32>& indices, std::vector<Vertex96>& vertices);

void MeshBuilderCommon::uploadIndexData(SubMeshData& subMesh, const std::vector<ui32>& indices, MeshDrawMode drawMode) {
    subMesh.mLODData.mTotalIndexCount = indices.size();
    const ui32 indexBufferSizeBytes = subMesh.mLODData.mTotalIndexCount * sizeof(ui32);
    assert(subMesh.mIbo);
    assert(!subMesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO));
    // Allocate orphaned
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexBufferSizeBytes, indices.data());
}

void MeshBuilderCommon::uploadIndexData(SubMeshData& subMesh, const ui16* indices, int indexCount, MeshDrawMode drawMode) {
    subMesh.mLODData.mTotalIndexCount = indexCount;
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
