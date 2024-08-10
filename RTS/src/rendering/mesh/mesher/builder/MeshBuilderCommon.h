#pragma once

#include "rendering/mesh/Mesh.h"
#include "rendering/mesh/Vertex.h"
#include "rendering/mesh/FBXRawModel.h"

struct RawMeshVertex;

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
    // TODO: REMOVE VERTEX
    template<typename VERTEX>
    static void optimizeMeshAndGenerateLODs(MeshGpuData& subMesh, std::vector<ui16>& indices, std::vector<VERTEX>& vertices);
    template<typename VERTEX>
    static void optimizeMeshAndGenerateLODs(MeshGpuData& subMesh, std::vector<ui32>& indices, std::vector<VERTEX>& vertices);
    static OptimizedCpuMeshData optimizeMeshAndGenerateLODs(const std::vector<ui32>& indices, const std::vector<RawMeshVertex>& vertices, f32 baseOptimizeErrorThreshold);
    // Requires VAO still bound
    static void uploadIndexData(MeshGpuData& subMesh, const std::vector<ui32>& indices, GLbitfield flags);
    static void uploadIndexData(MeshGpuData& subMesh, const ui16* indices, int indexCount, GLbitfield flags);

    static void uploadVertexData(MeshGpuData& subMesh, const void* vertexData, ui32 vertexCount, ui32 vertexSize, GLbitfield flags);
    static void uploadVertexDataNonInterleavedPositions(MeshGpuData& subMesh, const f32v3* positionData, const void* vertexData, ui32 vertexCount, size_t vertexSize, GLbitfield flags);


};

