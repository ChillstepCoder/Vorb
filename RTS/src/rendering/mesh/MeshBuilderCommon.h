#pragma once

#include "Mesh.h"
#include "Vertex.h"

struct SubMeshBufferData {
    void clear() {
        mVerts.clear();
        mIndices.clear();
        mTextures.clear();
    }

    // TODO: Pool allocators or reserve?
    std::vector<Vertex32> mVerts;
    std::vector<ui32> mIndices;
    std::vector<TextureHandle> mTextures;
};

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
    template<typename VERTEX>
    static void optimizeMeshAndGenerateLODs(MeshData& subMesh, std::vector<ui16>& indices, std::vector<VERTEX>& vertices);
    template<typename VERTEX>
    static void optimizeMeshAndGenerateLODs(MeshData& subMesh, std::vector<ui32>& indices, std::vector<VERTEX>& vertices);
    // Requires VAO still bound
    static void uploadIndexData(MeshData& subMesh, const std::vector<ui32>& indices, GLbitfield flags);
    static void uploadIndexData(MeshData& subMesh, const ui16* indices, int indexCount, GLbitfield flags);

    template<typename VERTEX>
    static void uploadVertexData(MeshData& subMesh, const std::vector<VERTEX>& vertices, GLbitfield flags);
    static void uploadStandardTextureUboData(MeshData& subMesh, const f32v3& pos, const std::vector<TextureHandle>& textures, GLbitfield flags);

};

