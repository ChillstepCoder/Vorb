#pragma once

#include "Mesh.h"
#include "Vertex.h"

enum class MeshBuilderBufferFlags : ui8 {
    NO_VBO = BIT(0),
    SSBO = BIT(1),
    UBO = BIT(2),
};

// Static common utils
class MeshBuilderCommon
{
public:
    MeshBuilderCommon() = delete;
    // Will create buffers and bind VAO
    static void initMeshBuffers(MeshData& subMesh, OPT VGBuffer* sharedIbo, BitFlags<MeshBuilderBufferFlags> flags = {});
    // TODO: REMOVE VERTEX
    template<typename VERTEX>
    static void optimizeMeshAndGenerateLODs(MeshData& subMesh, std::vector<ui16>& indices, std::vector<VERTEX>& vertices);
    template<typename VERTEX>
    static void optimizeMeshAndGenerateLODs(MeshData& subMesh, std::vector<ui32>& indices, std::vector<VERTEX>& vertices);
    // Requires VAO still bound
    static void uploadIndexData(MeshData& subMesh, const std::vector<ui32>& indices, GLbitfield flags);
    static void uploadIndexData(MeshData& subMesh, const ui16* indices, int indexCount, GLbitfield flags);

    static void uploadVertexData(MeshData& subMesh, const void* vertexData, ui32 vertexCount, size_t vertexSize, GLbitfield flags);
    static void uploadVertexDataNonInterleavedPositions(MeshData& subMesh, const f32v3* positionData, const void* vertexData, ui32 vertexCount, size_t vertexSize, GLbitfield flags);
    static void uploadStandardTextureUboData(MeshData& subMesh, const f32v3& pos, const std::vector<TextureHandle>& textures, GLbitfield flags);

};

