#include "stdafx.h"
#include "MeshBuilderCommon.h"

#include <meshoptimizer.h>

#include "rendering/texture/SubTexture.h"

#include "rendering/gl/GL.h"

// DAS Reference : https://github.com/fendevel/Guide-to-Modern-OpenGL-Functions

void MeshBuilderCommon::initMeshBuffers(SubMeshData& subMesh, OPT VGBuffer* sharedIbo, BitFlags<MeshBuilderBufferFlags> flags) {
    // VAO
    if (subMesh.mVao == 0) {
        GL.glCreateVertexArrays(1, &subMesh.mVao);

        // Ubo
        if (flags.isBitSet(MeshBuilderBufferFlags::UBO)) {
            GL.glCreateBuffers(1, &subMesh.mUbo);
        }
       
        // SSBO
        if (flags.isBitSet(MeshBuilderBufferFlags::SSBO)) {
            GL.glCreateBuffers(1, &subMesh.mSSBO);
        }
        else {
            assert(!subMesh.mSSBO);
        }
    }

    // IBO
    if (sharedIbo) {
        // Delete old IBO if needed
        if (subMesh.mIbo && !subMesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO)) {
            GL.glDeleteBuffers(1, &subMesh.mIbo);
        }
        subMesh.mIbo = *sharedIbo;
        subMesh.mFlags.setBit(MeshFlags::USING_SHARED_IBO);
        glVertexArrayElementBuffer(subMesh.mVao, subMesh.mIbo);
    }
    else if (subMesh.mIbo == 0) {
        GL.glCreateBuffers(1, &subMesh.mIbo);
        subMesh.mFlags.clearBit(MeshFlags::USING_SHARED_IBO);
    }

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
template void MeshBuilderCommon::optimizeMeshAndGenerateLODs(SubMeshData& subMesh, std::vector<ui16>& indices, std::vector<Vertex64>& vertices);

template<typename VERTEX>
void MeshBuilderCommon::optimizeMeshAndGenerateLODs(SubMeshData& subMesh, std::vector<ui32>& indices, std::vector<VERTEX>& vertices) {
    PROFILE_FUNCTION();
    assert(!subMesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO));

    static_assert(sizeof(unsigned int) == sizeof(ui32));

    std::vector<ui32> remap(indices.size());
    size_t vertexCount = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(VERTEX));

    std::vector<ui32> remappedIndices(indices.size());
    std::vector<VERTEX> remappedVertices(vertexCount);

    meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(), remap.data());
    meshopt_remapVertexBuffer(remappedVertices.data(), vertices.data(), vertices.size(), sizeof(VERTEX), remap.data());

    // Optimize initial mesh to get rid of mostly useless polygons
    constexpr float threshold = 0.2f;
    {
        std::vector<ui32> optimizedIndices;
        optimizedIndices.resize(remappedIndices.size());
        constexpr f32 targetError = 0.0006f;
        const size_t targetIndexCount = size_t(remappedIndices.size() * threshold);
        ui32 newSize = meshopt_simplify(&optimizedIndices[0], &remappedIndices[0], remappedIndices.size(), (const f32*)(&remappedVertices[0]), vertexCount, sizeof(VERTEX), targetIndexCount, targetError);
        optimizedIndices.resize(newSize);
        remappedIndices.swap(optimizedIndices);
    }

    // Re-remap to delete unused vertices
    {
        remap.resize(remappedIndices.size());
        vertexCount = meshopt_generateVertexRemap(remap.data(), remappedIndices.data(), remappedIndices.size(), remappedVertices.data(), remappedVertices.size(), sizeof(VERTEX));

        std::vector<ui32> remappedIndices2(remappedIndices.size());
        std::vector<VERTEX> remappedVertices2(vertexCount);

        meshopt_remapIndexBuffer(remappedIndices2.data(), remappedIndices.data(), remappedIndices.size(), remap.data());
        meshopt_remapVertexBuffer(remappedVertices2.data(), remappedVertices.data(), remappedVertices.size(), sizeof(VERTEX), remap.data());

        remappedIndices.swap(remappedIndices2);
        remappedVertices.swap(remappedVertices2);
    }


    meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(), remappedIndices.size(), vertexCount);
    // TODO: THIS ASSUMES POSITION IS ALWAYS THE FIRST FIELD! It better be :P
    meshopt_optimizeOverdraw(remappedIndices.data(), remappedIndices.data(), remappedIndices.size(), (const f32*)(&remappedVertices[0]), vertexCount, sizeof(VERTEX), 1.05f);
    meshopt_optimizeVertexFetch(remappedVertices.data(), remappedIndices.data(), remappedIndices.size(), remappedVertices.data(), vertexCount, sizeof(VERTEX));
     // TODO: Test compression - https://github.com/zeux/meshoptimizer

     /*TODO: https://github.com/zeux/meshoptimizer When a sequence of LOD meshes is generated that all use the original vertex buffer, care must be taken to order vertices optimally to not penalize mobile 
      GPU architectures that are only capable of transforming a sequential vertex buffer range.It's recommended in this case to first optimize each LOD for vertex cache, then assemble all LODs in one large
      index buffer starting from the coarsest LOD (the one with fewest triangles), and call meshopt_optimizeVertexFetch on the final large index buffer. This will make sure that coarser LODs require a smaller
      vertex range and are efficient wrt vertex fetch and transform. */

    constexpr float targetErrors[3]{
        0.01f, //0.0006f,
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
        const size_t targetIndexCount = size_t(prevSize * threshold);
        lodData.mLODStarts[i] = prevTotalSize;
        size_t maxLODSize = prevSize;
        remappedIndices.resize(prevTotalSize + maxLODSize);
        prevSize = meshopt_simplify(&remappedIndices[prevTotalSize], &remappedIndices[prevStart], prevSize, (const f32*)(&remappedVertices[0]), vertexCount, sizeof(VERTEX), targetIndexCount, targetError);
        prevStart = prevTotalSize;
        remappedIndices.resize(prevTotalSize + prevSize);

        // TODO: Optimize vertex cache, overdraw, vertex fetch per LOD?
    }
    lodData.mTotalIndexCount = remappedIndices.size();

    indices.swap(remappedIndices);
    vertices.swap(remappedVertices);
}
template void MeshBuilderCommon::optimizeMeshAndGenerateLODs(SubMeshData& subMesh, std::vector<ui32>& indices, std::vector<Vertex32>& vertices);
template void MeshBuilderCommon::optimizeMeshAndGenerateLODs(SubMeshData& subMesh, std::vector<ui32>& indices, std::vector<Vertex64>& vertices);

void MeshBuilderCommon::uploadIndexData(SubMeshData& subMesh, const std::vector<ui32>& indices, GLbitfield flags) {
    subMesh.mLODData.mTotalIndexCount = indices.size();
    const ui32 indexBufferSizeBytes = subMesh.mLODData.mTotalIndexCount * sizeof(ui32);
    assert(subMesh.mIbo);
    assert(!subMesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO));

    glNamedBufferStorage(subMesh.mIbo, indexBufferSizeBytes, indices.data(), flags);
    glVertexArrayElementBuffer(subMesh.mVao, subMesh.mIbo);
}

void MeshBuilderCommon::uploadIndexData(SubMeshData& subMesh, const ui16* indices, int indexCount, GLbitfield flags) {
    subMesh.mLODData.mTotalIndexCount = indexCount;
    subMesh.mIndexType = GL_UNSIGNED_SHORT;
    const ui32 indexBufferSizeBytes = indexCount * sizeof(ui16);
    assert(subMesh.mIbo);
    assert(!subMesh.mFlags.isBitSet(MeshFlags::USING_SHARED_IBO));

    glNamedBufferStorage(subMesh.mIbo, indexBufferSizeBytes, indices, flags);
    glVertexArrayElementBuffer(subMesh.mVao, subMesh.mIbo);
}

template<typename VERTEX>
void MeshBuilderCommon::uploadVertexData(SubMeshData& subMesh, const std::vector<VERTEX>& vertices, GLbitfield flags) {
    const unsigned bufferSizeBytes = vertices.size() * sizeof(VERTEX);
    subMesh.mVbo.allocate(bufferSizeBytes, vertices.data(), flags);
    glVertexArrayVertexBuffer(subMesh.mVao, 0, subMesh.mVbo.getHandle(), 0, sizeof(VERTEX));
}
template void MeshBuilderCommon::uploadVertexData(SubMeshData& subMesh, const std::vector<Vertex32>& vertices, GLbitfield flags);
template void MeshBuilderCommon::uploadVertexData(SubMeshData& subMesh, const std::vector<Vertex64>& vertices, GLbitfield flags);

void MeshBuilderCommon::uploadStandardTextureUboData(SubMeshData& subMesh, const f32v3& pos, const std::vector<TextureHandle>& textures, GLbitfield flags) {
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

    glNamedBufferStorage(subMesh.mUbo, uboSizeBytes, byteBuffer, flags);
}
