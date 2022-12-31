#pragma once

#include "Mesh.h"
#include "Vertex.h"
#include "rendering/mesh/RawMesh.h"

struct RawMeshVertex;

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

struct OptimizedCpuMeshData {
    OptimizedCpuMeshData() = default;
    VORB_NON_COPYABLE_BUT_MOVABLE(OptimizedCpuMeshData);

    MeshLODData lodData;
    std::vector<ui32> indices;
    std::vector<RawMeshVertex> vertices;
};

// Static common utils
class MeshBuilderCommon
{
public:
    MeshBuilderCommon() = delete;
    // Will create buffers and bind VAO
    static void initMeshBuffers(MeshGpuData& subMesh, OPT VGBuffer* sharedIbo, BitFlags<MeshBuilderBufferFlags> flags = {});
    template<typename VERTEX>
    static void optimizeMeshAndGenerateLODs(MeshGpuData& subMesh, std::vector<ui16>& indices, std::vector<VERTEX>& vertices);
    template<typename VERTEX>
    static void optimizeMeshAndGenerateLODs(MeshGpuData& subMesh, std::vector<ui32>& indices, std::vector<VERTEX>& vertices);
    static OptimizedCpuMeshData optimizeMeshAndGenerateLODs(const std::vector<ui32>& indices, const std::vector<RawMeshVertex>& vertices);
    // Requires VAO still bound
    static void uploadIndexData(MeshGpuData& subMesh, const std::vector<ui32>& indices, GLbitfield flags);
    static void uploadIndexData(MeshGpuData& subMesh, const ui16* indices, int indexCount, GLbitfield flags);

    static void uploadVertexData(MeshGpuData& subMesh, const void* vertexData, ui32 vertexCount, ui32 vertexSize, GLbitfield flags);
    static void uploadStandardTextureUboData(MeshGpuData& subMesh, const f32v3& pos, const std::vector<TextureHandle>& textures, GLbitfield flags);

};

