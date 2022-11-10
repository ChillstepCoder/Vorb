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
};

// Static common utils
class MeshBuilderCommon
{
public:
    MeshBuilderCommon() = delete;
    // Will create buffers and bind VAO
    static void initMeshBuffers(SubMeshData& subMesh, OPT VGBuffer* sharedIbo, BitFlags<MeshBuilderBufferFlags> flags = {});
    // Requires VAO still bound
    static void uploadIndexData(SubMeshData& subMesh, const std::vector<ui32>& indices, MeshDrawMode drawMode);
    static void uploadIndexData(SubMeshData& subMesh, const ui16* indices, int indexCount, MeshDrawMode drawMode);

    static void uploadVertexData(SubMeshData& subMesh, const std::vector<Vertex32>& vertices, MeshDrawMode drawMode);
    static void uploadStandardTextureUboData(SubMeshData& subMesh, const f32v3& pos, const std::vector<TextureHandle>& textures, MeshDrawMode drawMode);

};

